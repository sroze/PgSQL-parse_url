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
	url_internal *varlena = PG_GETARG_URL_P(0);
	char *key = TextDatumGetCString(PG_GETARG_TEXT_P(1));
	char *ret = NULL;
	int ret_length = 0;

	url *url_ret = url_from_varlena(varlena);

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
	// Some vars which will used to create the composite output type
	TupleDesc	tupdesc;
	char		**values;
	HeapTuple	tuple;
	AttInMetadata *attinmeta;
	bool		nulls[8];
	url 		*ret;
	url_internal *varlena = PG_GETARG_URL_P(0);

	// Check NULLs values
	if(PG_ARGISNULL(0) || PG_ARGISNULL(1)) {
		PG_RETURN_NULL();
	}

	ret = url_from_varlena(varlena);

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

PG_FUNCTION_INFO_V1(url_in);
Datum url_in (PG_FUNCTION_ARGS)
{
	char *str = PG_GETARG_CSTRING(0);
	url *object = parse_url_exec(str);

	url_internal *varlena_url = url_to_varlena(object);

#ifdef DEBUG
  elog(NOTICE, "url_in| str=%s", str);
  elog(NOTICE, "url_in: url_parsed. host=%s, path=%s", object->host, object->path);
  elog(NOTICE, "url_in: varlened (%d/%d)", (int) sizeof(varlena_url), (int) sizeof(url_internal));
  elog(NOTICE, "url_in: %d %d", varlena_url->host, varlena_url->host_len);
#endif

	PG_RETURN_URL_P(varlena_url);
}

PG_FUNCTION_INFO_V1(url_out);
Datum url_out (PG_FUNCTION_ARGS)
{
	url_internal *varlena = PG_GETARG_URL_P(0);
	int size = url_internal_size(varlena, URL_SIZE_CONTENTS_OUT);
	char *t = palloc(size), *u = palloc(size), *out = palloc(size + 1);

	memset(out, 0, size+1);

#ifdef DEBUG
  elog(NOTICE, "url_out (%d)", (int) sizeof(varlena));
  elog(NOTICE, "url_out (%d)", size);
  elog(NOTICE, "url_out: varlena->data=%s varlena->host=%d", varlena->data, varlena->host);
#endif

	if (varlena->scheme_len) {
		memset(t, 0, size);
		memcpy(t, (void *) (varlena->data + varlena->scheme), varlena->scheme_len);
		strcat(t, "://");
		strcat(out, t);
	}
	if (varlena->user_len) {
		memset(t, 0, size);
		memset(u, 0, size);
		memcpy(t, (void *) (varlena->data + varlena->user), varlena->user_len);
		if (varlena->pass_len) {
			strcat(t, ":");
			memcpy(u, (void *) (varlena->data + varlena->pass), varlena->pass_len);
			strcat(t, u);
		}
		strcat(t, "@");
		strcat(out, t);
	}
	if (varlena->host_len) {
		memset(t, 0, size);
		memcpy(t, (void *) (varlena->data + varlena->host), varlena->host_len);
		strcat(out, t);
	}
	if (varlena->port_len) {
		memset(t, 0, size);
		memset(u, 0, size);
		memcpy(t, ":", 1);
		memcpy(u, (void *) (varlena->data + varlena->port), varlena->port_len);
		strcat(t, u);
		strcat(out, t);
	}
	if (varlena->path_len) {
		memset(t, 0, size);
		memcpy(t, (void *) (varlena->data + varlena->path), varlena->path_len);
		strcat(out, t);
	}
	if (varlena->query_len) {
		memset(t, 0, size);
		memset(u, 0, size);
		memcpy(t, "?", 1);
		memcpy(u, (void *) (varlena->data + varlena->query), varlena->query_len);
		strcat(t, u);
		strcat(out, t);
	}
	if (varlena->fragment_len) {
		memset(t, 0, size);
		memset(u, 0, size);
		memcpy(t, "#", 1);
		memcpy(u, (void *) (varlena->data + varlena->fragment), varlena->fragment_len);
		strcat(t, u);
		strcat(out, t);
	}

	pfree(t);
	pfree(u);

#ifdef DEBUG
  elog(NOTICE, "url_out: out=%s", out);
#endif

	PG_RETURN_CSTRING(out);
}

url_internal* url_to_varlena (url *object)
{
	int size = url_size(object, URL_SIZE_CONTENTS_OUT);
	char *data = palloc(size), *port_str;
	url_internal *varlena = palloc(sizeof(url_internal) + size);

#ifdef DEBUG
  elog(NOTICE, "url_to_varlena: object->host=%s", object->host);
  elog(NOTICE, "url_to_varlena: size=%d", size);
#endif

	memset(varlena, 0, sizeof(url_internal) + size);
	memset(data, 0, size);

	if (object->scheme) {
		varlena->scheme = strlen(data);
		varlena->scheme_len = strlen(object->scheme);
		strcat(data, object->scheme);
	}
	if (object->user) {
		varlena->user = strlen(data);
		varlena->user_len = strlen(object->user);
		strcat(data, object->user);
		if (object->pass) {
			varlena->pass = strlen(data);
			varlena->pass_len = strlen(object->pass);
			strcat(data, object->pass);
		}
	}
	if (object->host) {
		varlena->host = strlen(data);
		varlena->host_len = strlen(object->host);
		strcat(data, object->host);
	}
	if (object->port) {
		port_str = portToString(object->port);
		varlena->port = strlen(data);
		varlena->port_len = strlen(port_str);
		strcat(data, port_str);
	}
	if (object->path) {
		varlena->path = strlen(data);
		varlena->path_len = strlen(object->path);
		strcat(data, object->path);
	}
	if (object->query) {
		varlena->query = strlen(data);
		varlena->query_len = strlen(object->query);
		strcat(data, object->query);
	}
	if (object->fragment) {
		varlena->fragment = strlen(data);
		varlena->fragment_len = strlen(object->fragment);
		strcat(data, object->fragment);
	}

#ifdef DEBUG
  elog(NOTICE, "url_to_varlena: varlena->host=%d,varlena->host_len=%d,data=%s", varlena->host, varlena->host_len, data);
#endif
	memcpy(varlena->data, data, size);

#ifdef DEBUG
  elog(NOTICE, "url_to_varlena: varlena->data=%s", varlena->data);
#endif
	return varlena;
}

url* url_from_varlena (url_internal* varlena)
{
	url* object = palloc(sizeof(url));
	char port_buf[7] = {0};

	memset(object, 0, sizeof(url));

#ifdef DEBUG
  elog(NOTICE, "url_from_varlena (%d)", (int) sizeof(varlena));
  elog(NOTICE, "url_from_varlena: varlena->data=%s varlena->host=%d", varlena->data, varlena->host);
  elog(NOTICE, "url_from_varlena: varlena->host_len=%d", varlena->host_len);
#endif

	if (varlena->scheme_len) {
		object->scheme = palloc(varlena->scheme_len+1);
		memset(object->scheme, 0, varlena->scheme_len+1);
		memcpy(object->scheme, (void *) (varlena->data + varlena->scheme), varlena->scheme_len);
	}
	if (varlena->user_len) {
		object->user = palloc(varlena->user_len+1);
		memset(object->user, 0, varlena->user_len+1);
		memcpy(object->user, (void *) (varlena->data + varlena->user), varlena->user_len);
		if (varlena->pass_len) {
			object->pass = palloc(varlena->pass_len+1);
			object->pass = palloc(varlena->pass_len+1);
			memcpy(object->pass, (void *) (varlena->data + varlena->pass), varlena->pass_len);
		}
	}
	if (varlena->host_len) {
		object->host = palloc(varlena->host_len+1);
		memset(object->host, 0, varlena->host_len+1);
		memcpy(object->host, (void *) (varlena->data + varlena->host), varlena->host_len);
	}
	if (varlena->port_len) {
		memcpy(port_buf, (void *) (varlena->data + varlena->port), varlena->port_len);
		object->port = atoi(port_buf);
	}
	if (varlena->path_len) {
		object->path = palloc(varlena->path_len+1);
		memset(object->path, 0, varlena->path_len+1);
		memcpy(object->path, (void *) (varlena->data + varlena->path), varlena->path_len);
	}
	if (varlena->query_len) {
		object->query = palloc(varlena->query_len+1);
		memset(object->query, 0, varlena->query_len+1);
		memcpy(object->query, (void *) (varlena->data + varlena->query), varlena->query_len);
	}
	if (varlena->fragment_len) {
		object->fragment = palloc(varlena->fragment_len+1);
		memset(object->fragment, 0, varlena->fragment_len+1);
		memcpy(object->fragment, (void *) (varlena->data + varlena->fragment), varlena->fragment_len);
	}

#ifdef DEBUG
  elog(NOTICE, "url_from_varlena: object->host=%s", object->host);
#endif

	return object;
}

int url_size (url *object, int type)
{
	int	size = 0, size_out = 0;

#ifdef DEBUG
  elog(NOTICE, "url_size: object->host=%s", object->host);
#endif

	if (type == URL_SIZE_EMPTY) {
		return sizeof(url);
	} // In others types, the content will be counted
	else {
		if (object->scheme) {
			size += (int) strlen(object->scheme);
			size_out += 3; // ://
		}
		if (object->user) {
			size += (int) strlen(object->user);
			size_out += 1; // @
		}
		if (object->pass) {
			size += (int) strlen(object->pass);
			size_out += 1; // :
		}
		if (object->host) {
			size += (int) strlen(object->host);
		}
		if (object->port) {
			size += (int) object->port;
			size_out += 1; // :
		}
		if (object->path) {
			size += (int) strlen(object->path);
		}
		if (object->query) {
			size += (int) strlen(object->query);
			size_out += 1; // ?
		}
		if (object->fragment) {
			size += (int) strlen(object->fragment);
			size_out += 1; // #
		}

		if (type == URL_SIZE_CONTENTS) {
			return (int) size;
		} else if (type == URL_SIZE_TOTAL) {
			return (int) size + sizeof(url);
		} else if (type == URL_SIZE_CONTENTS_OUT) {
			return (int) size + size_out;
		}
	}
	return 0;
}

int url_internal_size (url_internal *url, int type)
{
	int	size = 0, size_out = 0;

	if (type == URL_SIZE_EMPTY) {
		return sizeof(url_internal);
	} // In others types, the content will be counted
	else {
		if (url->scheme_len) {
			size += url->scheme_len;
			size_out += 3; // ://
		}
		if (url->user_len) {
			size += url->user_len;
			size_out += 1; // @
		}
		if (url->pass_len) {
			size += url->pass_len;
			size_out += 1; // :
		}
		if (url->host_len) {
			size += url->host_len;
		}
		if (url->port_len) {
			size += url->port_len;
			size_out += 1; // :
		}
		if (url->path_len) {
			size += url->path_len;
		}
		if (url->query_len) {
			size += url->query_len;
			size_out += 1; // ?
		}
		if (url->fragment_len) {
			size += url->fragment_len;
			size_out += 1; // #
		}

		if (type == URL_SIZE_CONTENTS) {
			return (int) size;
		} else if (type == URL_SIZE_CONTENTS_OUT) {
			return (int) size + size_out;
		}
	}
	return 0;
}

static inline struct varlena *make_varlena(url_internal *url)
{
	struct varlena *vdat;
	int size;

	size = sizeof(url_internal) + ((strlen(url->data)+1)*sizeof(char)) + VARHDRSZ;
	vdat = palloc(size);
	URL_SET_VARSIZE(vdat, size);
	memcpy(URL_VARDATA(vdat), url, (size - VARHDRSZ));

	return vdat;
}
