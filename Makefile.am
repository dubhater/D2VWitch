warningflags = -Wall -Wextra -Wshadow
commoncflags = $(FPIC) -O2 $(warningflags)
AM_CXXFLAGS = -std=c++11 $(commoncflags)
AM_CFLAGS = -std=c99 $(commoncflags)
AM_CPPFLAGS = $(QT5PLATFORMSUPPORT_CFLAGS) $(QT5WIDGETS_CFLAGS) $(vapoursynth_CFLAGS) $(libavcodec_CFLAGS) $(libavformat_CFLAGS) $(libavutil_CFLAGS)
AM_LDFLAGS = $(WINDOWS_SUBSYSTEM)


moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose_$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC     " $@;


moc_%.cpp : %.h
	$(moc_verbose)$(MOC) -o "$@" "$<"

bin_PROGRAMS = D2VWitch

moc_files = src/moc_GUIWindow.cpp \
			src/moc_ScrollArea.cpp

MOSTLYCLEANFILES = $(moc_files)

D2VWitch_SOURCES = src/Bullshit.cpp \
				   src/Bullshit.h \
				   src/Audio.cpp \
				   src/Audio.h \
				   src/D2V.cpp \
				   src/D2V.h \
				   src/D2VWitch.cpp \
				   src/FakeFile.cpp \
				   src/FakeFile.h \
				   src/FFMPEG.cpp \
				   src/FFMPEG.h \
				   src/GUIWindow.cpp \
				   src/GUIWindow.h \
				   src/MPEGParser.cpp \
				   src/MPEGParser.h \
				   src/ScrollArea.cpp \
				   src/ScrollArea.h \
				   $(moc_files)


LDADD = $(libavcodec_LIBS) $(libavformat_LIBS) $(libavutil_LIBS) $(QT5PLATFORMPLUGIN) $(QT5PLATFORMSUPPORT_LIBS) $(QT5WIDGETS_LIBS)
