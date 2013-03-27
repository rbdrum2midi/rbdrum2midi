#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([rbdrum2midi], [0.2], [randomas@hotmail.com])
AM_INIT_AUTOMAKE(rbdrum2midi, 0.2)
AC_CONFIG_SRCDIR([src/main.c])
#AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([DEPS], [libusb-1.0 >= 1.0.6 alsa >= 1.0.22])
# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT16_T

# Checks for library functions.
AC_CHECK_FUNCS([memset])
AC_CONFIG_FILES([Makefile src/Makefile])


AC_OUTPUT
