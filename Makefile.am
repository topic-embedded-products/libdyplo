ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = $(OPENMP_CFLAGS)
AM_DEFAULT_SOURCE_EXT = .cpp

lib_LTLIBRARIES = libdyplo.la libdyplosw.la
dyplo_libinclude_HEADERS = \
    exceptions.hpp \
    fileio.hpp \
    mmapio.hpp \
    directoryio.hpp \
    hardware.hpp
libdyplo_la_SOURCES = \
    fileio.cpp \
    hardware.cpp \
    $(dyplo_libinclude_HEADERS)
libdyplo_la_CPPFLAGS = -DBITSTREAM_DATA_PATH=\"${datadir}/bitstreams\"
dyplo_libincludedir = $(includedir)/dyplo

dyplosw_libinclude_HEADERS = \
    generics.hpp \
    condition.hpp \
    mutex.hpp \
    thread.hpp \
    queue.hpp \
    filequeue.hpp \
    scopedlock.hpp \
    noopscheduler.hpp \
    cooperativescheduler.hpp \
    cooperativeprocess.hpp \
    pthreadscheduler.hpp \
    threadedprocess.hpp
libdyplosw_la_SOURCES = \
    noopscheduler.cpp \
    pthreadscheduler.cpp \
    filequeue.cpp \
    $(dyplosw_libinclude_HEADERS)
libdyplosw_la_CXXFLAGS = $(OPENMP_CFLAGS)
libdyplosw_la_CPPFLAGS = -DBITSTREAM_DATA_PATH=\"${datadir}/bitstreams\"
dyplosw_libincludedir = $(includedir)/dyplo

TESTS = testdyplo
bin_PROGRAMS = $(TESTS) testdyplodriver testdyplostress testdyplobenchmark \
	dyplodemoapp dyplodemocryptoapp dyplodemohdlapp

testdyplo_SOURCES = yaffut.h \
	testdyplomain.cpp \
	testdyploqueue.cpp \
	testdyplocooperative.cpp \
	testdyplohardware.cpp \
	testdyplothreaded.cpp \
	testdyplosolver.cpp
testdyplo_LDADD = libdyplosw.la libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
testdyplodriver_LDADD = libdyplosw.la libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) -lrt
testdyplostress_LDADD = libdyplosw.la libdyplo.la
testdyplobenchmark_LDADD = libdyplo.la
dyplodemoapp_LDADD = libdyplosw.la libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
dyplodemocryptoapp_LDADD = libdyplosw.la libdyplo.la
dyplodemohdlapp_LDADD = libdyplosw.la libdyplo.la

dist_bin_SCRIPTS = testdyploall.sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dyplo.pc dyplosw.pc
