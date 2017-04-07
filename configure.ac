AC_PREREQ([2.69])
AC_INIT([dibc],[0.1],[no-reply@ericsson.com])
AC_CONFIG_SRCDIR([dibc/dibc.c])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AM_PROG_AR
LT_INIT

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([rt], [pthread], [dl], [gcc_s], [m])

AM_PATH_GLIB_2_0(2.22.0, [], AC_MSG_ERROR([Can not find valid version of glib]))

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h netdb.h netinet/in.h stdint.h stdlib.h \
                  string.h sys/socket.h sys/time.h syslog.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime gethostbyname gettimeofday memset socket strchr \
                strdup strerror strrchr strstr strtol])

# Some additional CFLAGS
AM_CFLAGS="$AM_CFLAGS -std=gnu99 -funwind-tables -fno-omit-frame-pointer"
AC_SUBST([AM_CFLAGS])

# FIXME: do/Makefile
AC_CONFIG_FILES([Makefile
                 dibc/Makefile])

AC_OUTPUT
