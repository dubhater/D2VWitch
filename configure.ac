AC_INIT([D2VWitch], [3], [https://github.com/dubhater/D2VWitch/issues], [D2VWitch], [https://github.com/dubhater/D2VWitch])

: ${CFLAGS=""}
: ${CXXFLAGS=""}

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz subdir-objects no-define])
AM_SILENT_RULES([yes])


AC_CANONICAL_HOST


AC_PROG_CXX
AC_PROG_GREP


AC_SYS_LARGEFILE
AC_FUNC_FSEEKO


PKG_CHECK_MODULES([vapoursynth], [vapoursynth])
PKG_CHECK_MODULES([libavcodec], [libavcodec])
PKG_CHECK_MODULES([libavformat], [libavformat])
PKG_CHECK_MODULES([libavutil], [libavutil])


qt_host_bins="$( eval $PKG_CONFIG --variable=host_bins Qt5Core )"

PKG_CHECK_MODULES([QT5WIDGETS], [Qt5Widgets])

AC_ARG_WITH(
            [moc],
            [AS_HELP_STRING([--with-moc], [Path of the Qt meta object compiler.])],
            [AC_SUBST([MOC], [$with_moc])],
            [
             AC_PATH_PROGS([MOC], [moc-qt5 moc], [not_found], [$qt_host_bins])
             AS_IF(
                   [test "x$MOC" = "x$not_found"],
                   [AC_MSG_ERROR(["moc (Qt's meta object compiler) was not found."])]
             )
            ]
)

AC_MSG_CHECKING([whether Qt is static])
if [ `$PKG_CONFIG --variable=qt_config Qt5Core | $GREP 'static' > /dev/null` ]; then
    static_qt="yes"
else
    static_qt="no"
fi
AC_MSG_RESULT([$static_qt])

AS_IF(
      [test "x$static_qt" = "xyes"],
      [
       PKG_CHECK_MODULES([QT5PLATFORMSUPPORT], [Qt5PlatformSupport])
       AC_DEFINE([D2VWITCH_STATIC_QT])
      ]
)


AS_CASE(
        [$host_os],
        [*linux*|gnu*|dragonfly*|*bsd*], # The BSDs are close enough, right?
        [
         AC_SUBST([FPIC], ["-fPIC"])
        ],
        [cygwin*|mingw*],
        [
         AC_SUBST([WINDOWS_SUBSYSTEM], ["-Wl,-subsystem,windows"])

         AS_IF([test "x$static_qt" = "xyes"],
               [
                AC_SUBST([QT5PLATFORMPLUGIN], ["$qt_host_bins/../plugins/platforms/libqwindows.a"])
               ]
         )
        ]
)


dnl Workaround for a bug in libtool
dnl The windows libtool uses a file magic checking method that only accepts
dnl dynamic libraries. Change it for libtool's alternative checking method.

dnl Workaround found in configure.ac from ffms2.

dnl Reproducing the error message below, for search engines and people
dnl looking for a solution...

dnl *** Warning: linker path does not have real file for library -lfftw3f.
dnl *** I have the capability to make that library automatically link in when
dnl *** you link to this library.  But I can only do this if you have a
dnl *** shared version of the library, which you do not appear to have
dnl *** because I did check the linker path looking for a file starting
dnl *** with libfftw3f and none of the candidates passed a file format test
dnl *** using a file magic. Last file checked: /home/asdf/mingw-w64/i686/mingw/lib/libfftw3f.a
dnl *** The inter-library dependencies that have been dropped here will be
dnl *** automatically added whenever a program is linked with this library
dnl *** or is declared to -dlopen it.
dnl *
dnl *** Since this library must not contain undefined symbols,
dnl *** because either the platform does not support them or
dnl *** it was explicitly requested with -no-undefined,
dnl *** libtool will only create a static version of it.

if test "$lt_cv_file_magic_cmd" = "func_win32_libid" ; then
    deplibs_check_method='file_magic file format pei*-(i386|x86-64)|(.*architecture: i386)?'
    file_magic_cmd='$OBJDUMP -f'
fi



AC_CONFIG_FILES([Makefile])
AC_OUTPUT
