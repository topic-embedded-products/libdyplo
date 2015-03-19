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
    mmapio.hpp \
    directoryio.hpp \
    filequeue.hpp \
    hardware.hpp \
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
    hardware.cpp \
    $(dyplo_libinclude_HEADERS)
libdyplo_la_CXXFLAGS = $(OPENMP_CFLAGS)
libdyplo_la_CPPFLAGS = -DBITSTREAM_DATA_PATH=\"${datadir}/bitstreams\"
dyplo_libincludedir = $(includedir)/dyplo

TESTS = testdyplo
bin_PROGRAMS = $(TESTS) testdyplodriver testdyplostress \
	dyplodemoapp dyplodemocryptoapp dyplodemohdlapp

testdyplo_SOURCES = yaffut.h \
	testdyplomain.cpp \
	testdyploqueue.cpp \
	testdyplocooperative.cpp \
	testdyplohardware.cpp \
	testdyplothreaded.cpp \
	testdyplosolver.cpp
testdyplo_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
testdyplodriver_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) -lrt
testdyplostress_LDADD = libdyplo.la
dyplodemoapp_LDADD = libdyplo.la $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
dyplodemocryptoapp_LDADD = libdyplo.la
dyplodemohdlapp_LDADD = libdyplo.la

dist_bin_SCRIPTS = testdyploall.sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dyplo.pc
