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

Datum parse_url_key (PG_FUNCTION_ARGS);
Datum parse_url_record (PG_FUNCTION_ARGS);

url *parse_url_exec (char* str);

char *_url_alloc_str (const char *s, int length);
void _url_free_str (const char *s);

char *portToString (unsigned port);

#endif /* PARSE_URL_H */
