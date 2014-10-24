AC_PREREQ([2.53])
AC_INIT([blicksemcheck],[1.0])
AC_CONFIG_SRCDIR([./bliksemcheck.c])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])

# Checks for library functions.
AC_CHECK_TOOL([STRIP],[strip])
AC_CHECK_HEADERS([libpq-fe.h],
                  [break],
                  [AC_MSG_ERROR([PostgreSQL headers not found or not usable])])
AC_DEFINE(LINUX)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
