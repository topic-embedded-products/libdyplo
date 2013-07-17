ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = $(OPENMP_CFLAGS)
AM_DEFAULT_SOURCE_EXT = .cpp

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
    cooperativeprocess.hpp \
    pthreadscheduler.hpp \
    threadedprocess.hpp
libdyplo_la_SOURCES = \
    noopscheduler.cpp \
    pthreadscheduler.cpp \
    $(dyplo_libinclude_HEADERS)
libdyplo_la_CXXFLAGS = $(OPENMP_CFLAGS)
dyplo_libincludedir = $(includedir)/dyplo

TESTS = testdyplo
bin_PROGRAMS = $(TESTS) dyplodemoapp

testdyplo_SOURCES = yaffut.h \
	testdyplomain.cpp \
	testdyploqueue.cpp \
	testdyplocooperative.cpp \
	testdyplothreaded.cpp
testdyplo_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
dyplodemoapp_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
