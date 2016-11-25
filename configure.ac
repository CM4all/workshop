AC_PREREQ(2.60)
AC_INIT(CM4all Workshop, 2.0.3, mk@cm4all.com, cm4all-snowball)
AC_COPYRIGHT(Copyright 2006-2016 Content Management AG)
AC_CONFIG_SRCDIR(src/main.cxx)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR(build)
AM_INIT_AUTOMAKE([foreign 1.11 subdir-objects silent-rules])
AM_SILENT_RULES

dnl Checks for programs.

AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_RANLIB

dnl Checks for libraries.

AX_BOOST_BASE([1.54], [], AC_MSG_ERROR([Boost not found]))
AX_BOOST_SYSTEM([1.54], [], AC_MSG_ERROR([boost::system not found]))
AX_BOOST_FILESYSTEM([1.54], [], AC_MSG_ERROR([boost::filesystem not found]))

PKG_CHECK_MODULES([LIBEVENT], [libevent >= 2.0.19],,
	[AC_MSG_ERROR([libevent not found])])

PKG_CHECK_MODULES([LIBSYSTEMD], [libsystemd],,
	[AC_MSG_ERROR([libsystemd not found])])

PKG_CHECK_MODULES([LIBCM4ALL_DAEMON], [libcm4all-daemon >= 0.4.2],,
	[AC_MSG_ERROR([libcm4all-daemon not found])])

PKG_CHECK_MODULES([LIBCM4ALL_INLINE], [libcm4all-inline >= 0.3.2],,
	[AC_MSG_ERROR([libcm4all-inline not found])])

PKG_CHECK_MODULES([LIBCM4ALL_SOCKET], [libcm4all-socket >= 0.3.5],,
	[AC_MSG_ERROR([libcm4all-socket not found])])

PKG_CHECK_MODULES([LIBCM4ALL_BENG_PROXY], [libcm4all-beng-proxy >= 12],,
	[AC_MSG_ERROR([libcm4all-beng-proxy not found])])

AC_ARG_WITH(pg-config,
	AS_HELP_STRING([--with-pg-config=PATH],
		[The path of libpq's pg_config script]),
		[],
                [AC_PATH_PROG(with_pg_config, pg_config,
			AC_MSG_ERROR([pg_config not found]))])
AC_SUBST(LIBPQ_CPPFLAGS,"-isystem `$with_pg_config --includedir`")
AC_SUBST(LIBPQ_LIBS,"-lpq")

dnl
dnl CFLAGS
dnl

AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_CXXFLAGS)
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(AM_LDFLAGS)

AM_CFLAGS=""
AM_CXXFLAGS="-std=c++14"
AM_CPPFLAGS="-D_REENTRANT -D_GNU_SOURCE"
AM_LDFLAGS="-Wl,--as-needed"

AM_CFLAGS="$AM_CFLAGS -Wall -Wextra -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Waggregate-return -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Wno-long-long -Wstrict-prototypes -Wundef -pedantic"

AM_CXXFLAGS="$AM_CXXFLAGS -Wall"
AM_CXXFLAGS="$AM_CXXFLAGS -Wextra"
AM_CXXFLAGS="$AM_CXXFLAGS -Wwrite-strings -Wcast-qual -Wcast-align -Wfloat-equal"
AM_CXXFLAGS="$AM_CXXFLAGS -Wshadow -Wpointer-arith -Wsign-compare"
AM_CXXFLAGS="$AM_CXXFLAGS -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute"
AM_CXXFLAGS="$AM_CXXFLAGS -Wredundant-decls -Wno-long-long -Wundef"
AM_CXXFLAGS="$AM_CXXFLAGS -Wno-missing-field-initializers"

dnl
dnl Compile-time options
dnl

AC_ARG_ENABLE(documentation,
	AS_HELP_STRING([--enable-documentation],
		[Enables documentation (default: disabled)]),,
	enable_documentation=no)
AM_CONDITIONAL(DOCUMENTATION, test x$enable_documentation = xyes)

dnl
dnl DEBUG
dnl

AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--disable-debug],
		[Disable debugging (default: enabled)]),,
	enable_debug=yes)
AM_CONDITIONAL(TEST, test x$enable_test = xyes)

if test x$enable_debug = xyes; then
	:
else
	AM_CFLAGS="$AM_CFLAGS -ffunction-sections -fdata-sections -fvisibility=hidden"
	AM_CXXFLAGS="$AM_CXXFLAGS -ffunction-sections -fdata-sections -fvisibility=hidden"
	AM_CPPFLAGS="$AM_CPPFLAGS -DNDEBUG"
	AM_LDFLAGS="$AM_LDFLAGS -Wl,-gc-sections"
fi

AC_ARG_ENABLE(werror,
	AS_HELP_STRING([--enable-werror],
		[treat warnings as errors]),,
	enable_werror=$enable_debug)

if test x$enable_werror = xyes; then
	AM_CFLAGS="$AM_CFLAGS -Werror -pedantic-errors"
	AM_CXXFLAGS="$AM_CXXFLAGS -Werror"
fi

dnl
dnl Output files
dnl

AC_OUTPUT(Makefile)
