Description
===========

::

    D2V Witch indexes MPEG (1, 2) streams and writes D2V files. These can
    be used with the VapourSynth plugin d2vsource.

    Usage: D2VWitch [options] input_file1 input_file2 ...

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


Compilation
===========

The usual steps work::

    ./autogen.sh
    ./configure
    make

Requirements:
    - A C++11 compiler

    - FFmpeg (Libav probably works too)


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
