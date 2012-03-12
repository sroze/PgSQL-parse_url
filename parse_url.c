/*
 * parse_url.c
 *
 *  Created on: 14 oct. 2009
 *  Modified on: 21 oct. 2009
 *      Author: Samuel ROZE <samuel.roze@gmail.com>
 */
#include "postgres.h"
#include <string.h>
#include <ctype.h>
#include "fmgr.h"
#include "access/heapam.h"
#include "utils/builtins.h"
#include "funcapi.h"
#include <stdio.h>


#include "parse_url.h"

#define numlen(n) ((n==0)?1:(int)log10(fabs(n)))

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(parse_url_key);
Datum parse_url_key (PG_FUNCTION_ARGS)
{
	char *str = TextDatumGetCString(PG_GETARG_TEXT_P(0));
	char *key = TextDatumGetCString(PG_GETARG_TEXT_P(1));
	char *ret = NULL;
	int ret_length = 0;

	url *url_ret = parse_url_exec(str);

	if (strcmp(key, "scheme") == 0) {
		ret = url_ret->scheme;
	} else if (strcmp(key, "user") == 0) {
		ret = url_ret->user;
	} else if (strcmp(key, "pass") == 0) {
		ret = url_ret->pass;
	} else if (strcmp(key, "host") == 0) {
		ret = url_ret->host;
	} else if (strcmp(key, "port") == 0) {
		ret = portToString(url_ret->port);
	} else if (strcmp(key, "path") == 0) {
		ret = url_ret->path;
	} else if (strcmp(key, "query") == 0) {
		ret = url_ret->query;
	} else if (strcmp(key, "fragment") == 0) {
		ret = url_ret->fragment;
	} else if (strcmp(key, "path+query") == 0) {
		ret_length = strlen(url_ret->path) + 2;
		if (url_ret->query) {
			ret_length += strlen(url_ret->query);
		}
		ret = palloc(ret_length);
		memset(ret, 0, ret_length);
		strcat(ret, url_ret->path);
		if (url_ret->query) {
			strcat(ret, "?");
			strcat(ret, url_ret->query);
		}
	} else if (strcmp(key, "host+port") == 0) {
		ret_length = strlen(url_ret->host) + numlen(url_ret->port) + 3;
		ret = palloc(ret_length);
		memset(ret, 0, ret_length);
		strcat(ret, url_ret->host);
		if (url_ret->port) {
			strcat(ret, ":");
			strcat(ret, portToString(url_ret->port));
		}
	} else {
		ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("Invalid part name of url")));
	}

	if (ret) {
		PG_RETURN_TEXT_P(CStringGetTextDatum(ret));
	} else {
		PG_RETURN_NULL();
	}
}

char *portToString (unsigned port)
{
	char *ret;

	if (port > 0) {
		ret = palloc(numlen(port) * sizeof(unsigned) + 1);
		sprintf(ret, "%d", port);
		return ret;
	} else {
		return NULL;
	}
}

PG_FUNCTION_INFO_V1(parse_url_record);
Datum parse_url_record (PG_FUNCTION_ARGS)
{
	// Vars about the params
	//text *str2 = PG_GETARG_TEXT_P(0);
	char* str = TextDatumGetCString(PG_GETARG_TEXT_P(0));

	// Some vars which will used to create the composite output type
	TupleDesc	tupdesc;
	char		**values;
	HeapTuple	tuple;
	AttInMetadata *attinmeta;
	bool		nulls[8];
	url 		*ret;

	// Check NULLs values
	if(PG_ARGISNULL(0) || PG_ARGISNULL(1)) {
		PG_RETURN_NULL();
	}

	ret = parse_url_exec(str);

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {
	    ereport(ERROR,
	    		(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
	             errmsg("function returning record called in context that cannot accept type record")));
	}
	attinmeta = TupleDescGetAttInMetadata(tupdesc);

	// ...
	values = (char **) palloc(8 * sizeof(char *));
	memset(values, 0, (8 * sizeof(char *)));

	// Add datas into the values Datum
	values[0] = (char *) ret->scheme;
	values[1] = (char *) ret->user;
	values[2] = (char *) ret->pass;
	values[3] = (char *) ret->host;
	values[4] = portToString(ret->port);
	values[5] = (char *) ret->path;
	values[6] = (char *) ret->query;
	values[7] = (char *) ret->fragment;

	// Convert values into a composite type
	memset(nulls, 0, sizeof(nulls));

	// build tuple from datum array
	tuple = BuildTupleFromCStrings(attinmeta, values);

	// Return the composite type
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

// Inspired from PHP's parse_url function
url* parse_url_exec (char* str) {
	char port_buf[6];
	int length = strlen(str);
	url *result;
	char const *s, *e, *p, *pp, *ue;

	result = (url *) palloc(sizeof(url));
	memset(result, 0, sizeof(url));

	s = str;
	ue = s + length;

	/* parse scheme */
	if ((e = memchr(s, ':', length)) && (e - s)) {
		/* validate scheme */
		p = s;
		while (p < e) {
			/* scheme = 1*[ lowalpha | digit | "+" | "-" | "." ] */
			if (!isalpha(*p) && !isdigit(*p) && *p != '+' && *p != '.' && *p != '-') {
				if (e + 1 < ue) {
					goto parse_port;
				} else {
					goto just_path;
				}
			}
			p++;
		}

		if (*(e + 1) == '\0') { /* only scheme is available */
			result->scheme = _url_alloc_str(s, (e - s));
			goto end;
		}

		/*
		 * certain schemas like mailto: and zlib: may not have any / after them
		 * this check ensures we support those.
		 */
		if (*(e+1) != '/') {
			/* check if the data we get is a port this allows us to
			 * correctly parse things like a.com:80
			 */
			p = e + 1;
			while (isdigit(*p)) {
				p++;
			}

			if ((*p == '\0' || *p == '/') && (p - e) < 7) {
				goto parse_port;
			}

			result->scheme = _url_alloc_str(s, (e-s));

			length -= ++e - s;
			s = e;
			goto just_path;
		} else {
			result->scheme = _url_alloc_str(s, (e-s));

			if (*(e+2) == '/') {
				s = e + 3;
				if (!strncasecmp("file", result->scheme, sizeof("file"))) {
					if (*(e + 3) == '/') {
						/* support windows drive letters as in:
						   file:///c:/somedir/file.txt
						*/
						if (*(e + 5) == ':') {
							s = e + 4;
						}
						goto nohost;
					}
				}
			} else {
				if (!strncasecmp("file", result->scheme, sizeof("file"))) {
					s = e + 1;
					goto nohost;
				} else {
					length -= ++e - s;
					s = e;
					goto just_path;
				}
			}
		}
	} else if (e) { /* no scheme, look for port */
		parse_port:
		p = e + 1;
		pp = p;

		while (pp-p < 6 && isdigit(*pp)) {
			pp++;
		}

		if (pp-p < 6 && (*pp == '/' || *pp == '\0')) {
			memcpy(port_buf, p, (pp-p));
			port_buf[pp-p] = '\0';
			result->port = atoi(port_buf);
		} else {
			goto just_path;
		}
	} else {
		just_path:
		ue = s + length;
		goto nohost;
	}

	e = ue;

	if (!(p = memchr(s, '/', (ue - s)))) {
		if ((p = memchr(s, '?', (ue - s)))) {
			e = p;
		} else if ((p = memchr(s, '#', (ue - s)))) {
			e = p;
		}
	} else {
		e = p;
	}

	/* check for login and password */
	if ((p = memchr(s, '@', (e-s)))) { //zend_memrchr
		if ((pp = memchr(s, ':', (p-s)))) {
			if ((pp-s) > 0) {
				result->user = _url_alloc_str(s, (pp-s));;
			}

			pp++;
			if (p-pp > 0) {
				result->pass = _url_alloc_str(pp, (p-pp));
			}
		} else {
			result->user = _url_alloc_str(s, (p-s));
		}

		s = p + 1;
	}

	/* check for port */
	if (*s == '[' && *(e-1) == ']') {
		/* Short circuit portscan,
		   we're dealing with an
		   IPv6 embedded address */
		p = s;
	} else {
		/* memrchr is a GNU specific extension
		   Emulate for wide compatability */
		for(p = e; *p != ':' && p >= s; p--);
	}

	if (p >= s && *p == ':') {
		if (!result->port) {
			p++;
			if (e-p > 5) { /* port cannot be longer then 5 characters */
				_url_free_str(result->scheme);
				_url_free_str(result->user);
				_url_free_str(result->pass);
				free(result);
				return NULL;
			} else if (e - p > 0) {
				memcpy(port_buf, p, (e-p));
				port_buf[e-p] = '\0';
				result->port = atoi(port_buf);
			}
			p--;
		}
	} else {
		p = e;
	}

	/* check if we have a valid host, if we don't reject the string as url */
	if ((p-s) < 1) {
		_url_free_str(result->scheme);
		_url_free_str(result->user);
		_url_free_str(result->pass);
		free(result);
		return NULL;
	}

	result->host = _url_alloc_str(s, (p-s));

	if (e == ue) {
		return result;
	}

	s = e;

	nohost:

	if ((p = memchr(s, '?', (ue - s)))) {
		pp = strchr(s, '#');

		if (pp && pp < p) {
			p = pp;
			pp = strchr(pp+2, '#');
		}

		if (p - s) {
			result->path = _url_alloc_str(s, (p-s));
		}

		if (pp) {
			if (pp - ++p) {
				result->query = _url_alloc_str(p, (pp-p));
			}
			p = pp;
			goto label_parse;
		} else if (++p - ue) {
			result->query = _url_alloc_str(p, (ue-p));
		}
	} else if ((p = memchr(s, '#', (ue - s)))) {
		if (p - s) {
			result->path = _url_alloc_str(s, (p-s));
		}

		label_parse:
		p++;

		if (ue - p) {
			result->fragment = _url_alloc_str(p, (ue-p));
		}
	} else {
		result->path = _url_alloc_str(s, (ue-s));
	}
end:
	return result;
}

char *_url_alloc_str (const char *s, int length)
{
	char *p;

	p = (char *) palloc(length+1);
	if (p == NULL) {
		return p;
	}
	memcpy(p, s, length);
	p[length] = 0;

	return p;
}

void _url_free_str (const char *s)
{
	pfree((void *) s);
}
