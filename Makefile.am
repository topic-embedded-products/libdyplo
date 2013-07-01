AM_LDFLAGS = $(OPENMP_CFLAGS)

lib_LTLIBRARIES = libhmu.la
libhmu_la_SOURCES = \
    hmu.cpp hmu.h
libhmu_la_CXXFLAGS = $(OPENMP_CFLAGS)
hmu_libincludedir = $(includedir)/tmb
hmu_libinclude_HEADERS = hmu.h

TESTS = testhmu
bin_PROGRAMS = $(TESTS)

AM_DEFAULT_SOURCE_EXT = .cpp
testhmu_SOURCES = testhmu.cpp yaffut.h
testhmu_LDADD = libhmu.la

