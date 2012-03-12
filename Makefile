MODULES = parse_url
DATA = parse_url.sql

PG_CONFIG := pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
