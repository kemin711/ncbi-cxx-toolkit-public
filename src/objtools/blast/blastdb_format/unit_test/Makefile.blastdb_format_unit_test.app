# $Id$

APP = blastdb_format_unit_test
SRC = seq_writer_unit_test seq_formatter_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB_ = test_boost blastdb_format xobjutil seqdb blastdb $(SOBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC)) $(LMDB_LIB)
LIBS = $(BLAST_THIRD_PARTY_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = blastdb_format_unit_test
CHECK_COPY = blastdb_format_unit_test.ini data

REQUIRES = Boost.Test.Included


WATCHERS = zaretska jianye camacho fongah2
