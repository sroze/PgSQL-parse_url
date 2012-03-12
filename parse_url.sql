DROP TYPE url CASCADE;
DROP TYPE url_record CASCADE;

CREATE TYPE url_record AS ("scheme" text, "user" text, "pass" text, "host" text, "port" integer, "path" text, "query" text, "fragment" text);
CREATE TYPE url;
CREATE FUNCTION url_in (cstring) RETURNS url AS '$libdir/parse_url.so', 'url_in' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION url_out (url) RETURNS cstring AS '$libdir/parse_url.so', 'url_out' LANGUAGE C IMMUTABLE STRICT;
CREATE TYPE url (
	input = url_in,
	output = url_out
);
CREATE FUNCTION parse_url(url, text) RETURNS text AS '$libdir/parse_url.so', 'parse_url_key' LANGUAGE C STRICT;
CREATE FUNCTION parse_url (url) RETURNS url_record AS '$libdir/parse_url.so', 'parse_url_record' LANGUAGE C STRICT;

