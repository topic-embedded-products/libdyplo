ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = $(OPENMP_CFLAGS)
AM_DEFAULT_SOURCE_EXT = .cpp

lib_LTLIBRARIES = libdyplo.la
dyplo_libinclude_HEADERS = \
    generics.hpp \
    exceptions.hpp \
    condition.hpp \
    mutex.hpp \
    thread.hpp \
    queue.hpp \
    fileio.hpp \
    filequeue.hpp \
    scopedlock.hpp \
    noopscheduler.hpp \
    cooperativescheduler.hpp \
    cooperativeprocess.hpp \
    pthreadscheduler.hpp \
    threadedprocess.hpp
libdyplo_la_SOURCES = \
    noopscheduler.cpp \
    pthreadscheduler.cpp \
    fileio.cpp \
    filequeue.cpp \
    $(dyplo_libinclude_HEADERS)
libdyplo_la_CXXFLAGS = $(OPENMP_CFLAGS)
dyplo_libincludedir = $(includedir)/dyplo

TESTS = testdyplo
bin_PROGRAMS = $(TESTS) testdyplodriver dyplodemoapp dyplodemocryptoapp

testdyplo_SOURCES = yaffut.h \
	testdyplomain.cpp \
	testdyploqueue.cpp \
	testdyplocooperative.cpp \
	testdyplothreaded.cpp \
	testdyplosolver.cpp
testdyplo_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
testdyplodriver_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
dyplodemoapp_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
dyplodemocryptoapp_LDADD = libdyplo.la

dist_bin_SCRIPTS = testdyplohardware.sh
