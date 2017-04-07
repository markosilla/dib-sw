AM_CFLAGS = @AM_CFLAGS@ -I$(top_srcdir)/libtestbox $(GLIB_CFLAGS)

bin_PROGRAMS = testboxd

testboxd_SOURCES = \
 testboxd_args.c \
 testboxd_args.h \
 testboxd_libs.c \
 testboxd_libs.h \
 testboxd_main.c \
 testboxd_print.c \
 testboxd_print.h \
 testboxd.sh \
 testboxd_sockets.c \
 testboxd_sockets.h \
 testboxd_threads.c \
 testboxd_threads.h \
 testboxd_types.h \
 testboxd_utilities.c \
 testboxd_utilities.h

testboxd_LDADD = $(top_builddir)/libtestbox/libtestbox.la

testboxd_LDFLAGS = -lpthread -lgcc_s -lrt -ldl

bin_SCRIPTS = testboxd.sh
