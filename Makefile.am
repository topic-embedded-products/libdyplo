ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = $(OPENMP_CFLAGS)

lib_LTLIBRARIES = libhmu.la
hmu_libinclude_HEADERS = \
    generics.hpp \
    condition.hpp \
    cooperativescheduler.hpp \
    mutex.hpp \
    queue.hpp \
    scopedlock.hpp \
    noopscheduler.hpp \
    pthreadscheduler.hpp
libhmu_la_SOURCES = \
    noopscheduler.cpp \
    pthreadscheduler.cpp \
    $(hmu_libinclude_HEADERS)
libhmu_la_CXXFLAGS = $(OPENMP_CFLAGS)
hmu_libincludedir = $(includedir)/hmu

TESTS = testhmu
bin_PROGRAMS = $(TESTS)

AM_DEFAULT_SOURCE_EXT = .cpp
testhmu_SOURCES = testhmu.cpp yaffut.h
testhmu_LDADD = libhmu.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

