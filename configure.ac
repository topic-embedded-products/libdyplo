AC_INIT([dyplo], [0.1], [mike.looijmans@topic.nl])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CXX
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_LIBTOOL
AX_PTHREAD(HAVE_PTHREAD=yes, AC_MSG_ERROR([Need pthreads]))
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
