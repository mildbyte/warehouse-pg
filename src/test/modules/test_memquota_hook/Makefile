# src/test/modules/test_memquota_hook/Makefile

MODULES = test_memquota_hook
PGFILEDESC = "test_memquota_hook - test plan_mem_intensive_ops_hook"

REGRESS = test_memquota_hook

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = src/test/modules/test_memquota_hook
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
