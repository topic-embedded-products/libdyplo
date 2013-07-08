ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = $(OPENMP_CFLAGS)

lib_LTLIBRARIES = libhmu.la
libhmu_la_SOURCES = \
    hmu.cpp \
    generics.hpp \
    queue.hpp \
    scopedlock.hpp \
    noopscheduler.hpp \
    pthreadscheduler.hpp
libhmu_la_CXXFLAGS = $(OPENMP_CFLAGS)
hmu_libincludedir = $(includedir)/hmu
hmu_libinclude_HEADERS = \
    generics.hpp \
    queue.hpp \
    scopedlock.hpp \
    noopscheduler.hpp \
    pthreadscheduler.hpp

TESTS = testhmu
bin_PROGRAMS = $(TESTS)

AM_DEFAULT_SOURCE_EXT = .cpp
testhmu_SOURCES = testhmu.cpp yaffut.h
testhmu_LDADD = libhmu.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

