# $Id$

APP = db100_t0004
SRC = t0004 common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS100_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds100 tds_ftds100
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-db100 db100_t0004
CHECK_COPY = t0004.sql

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
