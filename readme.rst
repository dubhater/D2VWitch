Description
===========

D2V Witch indexes various video streams and writes D2V files. These can
be used with the VapourSynth plugin d2vsource.

Supported container formats:

* MPEG elementary streams

* MPEG program streams (aka DVD)

* MPEG transport streams (aka Blu-ray and TV captures)

* PVA streams.

* H264 elementary streams

Supported video codecs:

* MPEG 1

* MPEG 2

* H264

The executable is both a console application and a graphical one. The
graphical interface is shown if the command line parameters passed
are all `recognised by Qt <http://doc.qt.io/qt-5/qapplication.html#QApplication>`_,
or if there are no command line parameters.

Due to technical reasons, D2V Witch will not look quite right in
cmd.exe, but it's still pretty usable.

In order to help with cutting and demuxing parts of the video, the
graphical interface will use the VapourSynth plugin d2vsource to
display the video, if VapourSynth and the plugin can be found. Cutting
parts of the video can still be done even if they are not found.

::

    Usage: d2vwitch [options] input_file1 input_file2 ...

    Options:
        --help
            Display this message.

        --version
            Display version numbers and ffmpeg configuration.

        --info
            Display relevant information about the input file(s), such as a
            list of all the video and audio tracks and their ids.

        --quiet
            Do not print progress information or warnings. Fatal errors are
            always printed.

        --output <d2v name>
            Specify the name of the D2V file. The special name "-" means
            standard output. If not specified, the name of the D2V file is
            deduced from the name of the first input file.

        --audio-ids <id1,id2,...>
            Demux the audio tracks with the specified ids. The special
            value "all" means that all audio tracks should be demuxed. By
            default, no audio tracks are demuxed. The names of the audio
            files are deduced from the name of the D2V file.

        --video-id <id>
            Process the video track with this id. By default, the first
            video track found will be processed.

        --input-range <range>
            Set the YUVRGB_Scale field in the d2v file according to the
            video's input colour range. Possible values are "limited" and
            "full". By default, the video is assumed to have limited range.

        --ffmpeg-log-level <level>
            Control how much of ffmpeg's messages will be printed. Possible
            values: 'quiet', 'panic', 'fatal', 'error', 'warning', 'info',
            'verbose', 'debug', and 'trace'. The default is 'panic'.


Compilation
===========

The usual steps work::

    ./autogen.sh
    ./configure
    make

Requirements:
    - A C++11 compiler

    - FFmpeg (Libav maybe works too)

    - Qt 5.2, or maybe newer

    - VapourSynth.h


Limitations
===========

The "skip" field is always 0, because the author has no clue about this
stuff/the DGIndex manual's Appendix A doesn't explain it well enough.

The "vob" and "cell" fields are always 0, because ffmpeg doesn't know
about the structure of DVDs, and the author doesn't care.


License
=======

MPEGParser.cpp is available under the LGPL v2.1 license. The rest of
the code is available under the ISC license.
