ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = $(OPENMP_CFLAGS)

lib_LTLIBRARIES = libdyplo.la
dyplo_libinclude_HEADERS = \
    generics.hpp \
    condition.hpp \
    mutex.hpp \
    thread.hpp \
    queue.hpp \
    scopedlock.hpp \
    noopscheduler.hpp \
    cooperativescheduler.hpp \
    pthreadscheduler.hpp
libdyplo_la_SOURCES = \
    noopscheduler.cpp \
    pthreadscheduler.cpp \
    $(dyplo_libinclude_HEADERS)
libdyplo_la_CXXFLAGS = $(OPENMP_CFLAGS)
dyplo_libincludedir = $(includedir)/dyplo

TESTS = testdyplo testdyploqueue
bin_PROGRAMS = $(TESTS)

AM_DEFAULT_SOURCE_EXT = .cpp
testdyplo_SOURCES = testdyplo.cpp yaffut.h
testdyplo_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

testdyploqueue_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)