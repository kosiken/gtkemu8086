#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([emu8086], [1.0.2], [allisonkosy@gmail.com])
AC_CONFIG_SRCDIR([src/emu8086app.c])
AM_INIT_AUTOMAKE([-Wall foreign])
AM_PROG_AR
AC_CONFIG_HEADERS(config.h)
LT_INIT(disable-static)
# Checks for programs.
AC_PROG_CC
PKG_PROG_PKG_CONFIG

# Checks for libraries.
PKG_CHECK_MODULES([EMU8086], [
    glib-2.0
    gtk+-3.0
    libpeas-1.0 >= 1.2.0
	libpeas-gtk-1.0 >= 1.2.0
])

AM_PATH_PYTHON([3.0])
AC_CONFIG_MACRO_DIRS([m4])
GOBJECT_INTROSPECTION_CHECK([0.9.3])

if test "$found_introspection" = "yes"; then
	have_introspection=yes
	AC_DEFINE([HAVE_INTROSPECTION], [1], [Define to enable GObject Introspection])
else
	have_introspection=no
fi

EMU8086_PLUGINS_LIBS_DIR="$datadir/emu8086/plugins"



# AH_ADD
AC_SUBST(EMU8086_PLUGINS_LIBS_DIR)
GLIB_GSETTINGS

AC_OUTPUT([
Makefile
src/Makefile
include/Makefile
pixmaps/Makefile
pixbufs/Makefile
egs/Makefile
egs/plugins/Makefile
mime/Makefile
plugins/Makefile
plugins/lcdhitachi/Makefile
plugins/lcdhitachi/lcdhitachi/Makefile
plugins/hexcalculator/Makefile
plugins/hexcalculator/hexcalculator/Makefile
plugins/outlineview/Makefile
plugins/outlineview/outlineview/Makefile
themes/Makefile
])
