# $Id$

APP = tds100_strtok_r
SRC = strtok_r

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS100_INCLUDE) -I$(srcdir)/.. $(ORIG_CPPFLAGS)
LIB      = tds_ftds100
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD =

WATCHERS = ucko satskyse
