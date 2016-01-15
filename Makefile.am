warningflags = -Wall -Wextra -Wshadow -Wno-unused-function
commoncflags = -O2 $(warningflags)
AM_CXXFLAGS = -std=c++11 $(commoncflags)
AM_CFLAGS = -std=c99 $(commoncflags)
AM_CPPFLAGS = $(libavcodec_CFLAGS) $(libavformat_CFLAGS) $(libavutil_CFLAGS)


bin_PROGRAMS = D2VWitch

D2VWitch_SOURCES = src/Bullshit.h \
				   src/D2V.cpp \
				   src/D2V.h \
				   src/D2VWitch.cpp \
				   src/FakeFile.cpp \
				   src/FakeFile.h \
				   src/FFMPEG.cpp \
				   src/FFMPEG.h \
				   src/MPEGParser.cpp \
				   src/MPEGParser.h

D2VWitch_LDFLAGS = $(UNICODELDFLAGS)


LDADD = $(libavcodec_LIBS) $(libavformat_LIBS) $(libavutil_LIBS)
