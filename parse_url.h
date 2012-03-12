/*
 * parse_url.h
 *
 *  Created on: 14 oct. 2009
 *      Author: Samuel ROZE <samuel.roze@gmail.com>
 */

#ifndef PARSE_URL_H
#define PARSE_URL_H

typedef struct url {
	char *scheme;
	char *user;
	char *pass;
	char *host;
	unsigned port;
	char *path;
	char *query;
	char *fragment;
} url;

typedef struct url_internal {
    int scheme;
    int scheme_len;

    int user;
    int user_len;

    int pass;
    int pass_len;

    int host;
    int host_len;

    int port;
    int port_len;

    int path;
    int path_len;

    int query;
    int query_len;

    int fragment;
    int fragment_len;

    char data[1];
} url_internal;

Datum parse_url_key (PG_FUNCTION_ARGS);
Datum parse_url_record (PG_FUNCTION_ARGS);
Datum url_in (PG_FUNCTION_ARGS);
Datum url_out (PG_FUNCTION_ARGS);

url *parse_url_exec (char* str);
url_internal* url_to_varlena (url *object);
url* url_from_varlena (url_internal* varlena);

int url_size (url *url, int type);
int url_internal_size (url_internal *url, int type);

char *_url_alloc_str (const char *s, int length);
void _url_free_str (const char *s);

char *portToString (unsigned port);
static inline struct varlena *make_varlena(url_internal *url);

#if PG_VERSION_NUM / 100 <= 802
#define URL_VARSIZE(x)        (VARSIZE(x) - VARHDRSZ)
#define URL_VARDATA(x)        (VARDATA(x))
#define URL_PG_GETARG_TEXT(x) (PG_GETARG_TEXT_P(x))
#define URL_SET_VARSIZE(p, s) (VARATT_SIZEP(p) = s)

#else
#define URL_VARSIZE(x)        (VARSIZE_ANY_EXHDR(x))
#define URL_VARDATA(x)        (VARDATA_ANY(x))
#define URL_PG_GETARG_TEXT(x) (PG_GETARG_TEXT_PP(x))
#define URL_SET_VARSIZE(p, s) (SET_VARSIZE(p, s))
#endif

#define DatumGetUrl(X)	          ((url_internal *) URL_VARDATA(DatumGetPointer(X)) )
#define PG_GETARG_URL_P(n)	  DatumGetUrl(PG_DETOAST_DATUM(PG_GETARG_DATUM(n)))

#define UrlGetDatum(X)	          PointerGetDatum(make_varlena(X))
#define PG_RETURN_URL_P(x)	  return UrlGetDatum(x)

#define DEBUG 1

enum {
	URL_SIZE_EMPTY,
	URL_SIZE_CONTENTS,
	URL_SIZE_TOTAL,
	URL_SIZE_CONTENTS_OUT // URL_SIZE_TOTAL + 0-4 ("@", ":", "?", "#")
};

#endif /* PARSE_URL_H */
