Description
===========

D2V Witch indexes various video streams and writes D2V files. These can
be used with the VapourSynth plugin d2vsource.

Supported container formats:

* MPEG elementary streams

* MPEG program streams (aka DVD)

* MPEG transport streams (aka Blu-ray and TV captures)

* PVA streams.

Supported video codecs:

* MPEG 1

* MPEG 2

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

        --relative-paths <yes|no>
            If 'yes', the paths to the video files written in the d2v file
            will be relative to the location of the d2v file. If 'no', the
            paths will be absolute paths. If the name of the d2v file is
            '-' (standard output), then this option is ignored and the
            paths will be absolute paths. The default is to use the value
            stored in the program's configuration file. If no value is
            stored in the configuration file, then the default is 'no'.

        --single-input
            Index only the one file provided on the command line. Without
            this parameter, D2V Witch will detect sequences of files
            called "VTS_xx_y.VOB" and it will index any such files that
            follow the provided file in the sequence. The detection and
            sorting are case-insensitive.

            For example, if a folder contains the files vts_01_1.vob,
            vts_01_2.vob, vts_01_3.vob, vts_02_1.vob, vts_03_1.vob,
            file1.mpg, and file2.mpg:
                `d2vwitch vts_01_2.vob` will index vts_01_2.vob and
                    vts_01_3.vob. It will not index vts_01_1.vob because
                    it comes before the file provided in the command. It
                    will not index vts_02_1.vob or vts_03_1.vob because
                    those don't go together with the file provided in the
                    command.
                `d2vwitch --single-input vts_01_2.vob` will index only
                    vts_01_2.vob because --single-input disables the
                    automatic detection of sequences.
                `d2vwitch file1.mpg` will index only file1.mpg because the
                    file name doesn't follow the special pattern.
                `d2vwitch vts_01_1.vob vts_01_2.vob` will index only the
                    files provided in the command, because more than one
                    file was provided.


Compilation
===========

The usual steps work::

    ./autogen.sh
    ./configure
    make

Alternatively::

    meson build
    cd build
    ninja

Requirements:
    - A C++11 compiler

    - FFmpeg 3.4 or newer (Libav maybe works too)

    - Qt 5.2, or maybe newer (5.5.1 works)

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
