CREATE TYPE url_record AS ("scheme" text, "user" text, "pass" text, "host" text, "port" integer, "path" text, "query" text, "fragment" text);

CREATE OR REPLACE FUNCTION parse_url(text, text) RETURNS text AS 'parse_url.so', 'parse_url_key' LANGUAGE C STRICT;
CREATE OR REPLACE FUNCTION parse_url (text) RETURNS url_record AS 'parse_url.so', 'parse_url_record' LANGUAGE C STRICT;
