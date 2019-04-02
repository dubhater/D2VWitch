/*

Copyright (c) 2016, John Smith

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/


#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}


#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#include <QApplication>
#include <QDir>
#include <QFileInfo>


#ifdef D2VWITCH_STATIC_QT

#ifdef _WIN32
#include <QtPlugin>
Q_IMPORT_PLUGIN (QWindowsIntegrationPlugin);
#else
#error "Not sure what to do with a static Qt on this platform. File a bug report."
#endif // _WIN32

#endif // D2VWITCH_STATIC_QT


#include "Audio.h"
#include "Bullshit.h"
#include "D2V.h"
#include "FakeFile.h"
#include "FFMPEG.h"
#include "GUIWindow.h"


void printProgress(int64_t current_position, int64_t total_size, void *) {
    fprintf(stderr, "%3d%%\r", (int)(current_position * 100 / total_size));
}


void printWarnings(const std::string &message, void *) {
    fprintf(stderr, "%s\n", message.c_str());
}


void printHelp() {
    const char usage[] = R"usage(
D2V Witch indexes various streams and writes D2V files. These can
be used with the VapourSynth plugin d2vsource.

Supported container formats: MPEG elementary streams, MPEG program
streams, MPEG transport streams, PVA streams.

Supported video formats: MPEG 1, MPEG 2.

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

)usage";

    fprintf(stderr, "%s", usage);
}


void printVersions() {
    unsigned lavf = avformat_version();
    unsigned lavc = avcodec_version();
    unsigned lavu = avutil_version();

    fprintf(stderr,
            "D2V Witch version: %s\n"
            "libavformat version: %u.%u.%u\n"
            "libavcodec version: %u.%u.%u\n"
            "libavutil version: %u.%u.%u\n"
            "\n"
            "libavformat configuration:\n%s\n\n"
            "libavcodec configuration:\n%s\n\n"
            "libavutil configuration:\n%s\n\n",
            PACKAGE_VERSION,
            (lavf >> 16) & 0xff, (lavf >> 8) & 0xff, lavf & 0xff,
            (lavc >> 16) & 0xff, (lavc >> 8) & 0xff, lavc & 0xff,
            (lavu >> 16) & 0xff, (lavu >> 8) & 0xff, lavu & 0xff,
            avformat_configuration(),
            avcodec_configuration(),
            avutil_configuration());
}


void printInfo(const AVFormatContext *fctx, const FakeFile &fake_file) {
    fprintf(stderr, "Input file(s):\n");
    for (size_t i = 0; i < fake_file.size(); i++)
        fprintf(stderr, "    %s\n", fake_file[i].name.c_str());

    fprintf(stderr, "\n    Type: %s\n", fctx->iformat->long_name ? fctx->iformat->long_name : fctx->iformat->name);

    fprintf(stderr, "\nVideo tracks:\n");

    for (unsigned i = 0; i < fctx->nb_streams; i++) {
        if (fctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            const char *type = "unknown";
            const AVCodecDescriptor *desc = avcodec_descriptor_get(fctx->streams[i]->codec->codec_id);
            if (desc)
                type = desc->long_name ? desc->long_name : desc->name;

            int width, height;
            if (av_opt_get_image_size(fctx->streams[i]->codec, "video_size", 0, &width, &height) < 0)
                width = height = -1;

            const char *pixel_format = av_get_pix_fmt_name(fctx->streams[i]->codec->pix_fmt);
            if (!pixel_format)
                pixel_format = "unknown";

            fprintf(stderr, "    Id: %x, type: %s, %dx%d, %s\n",
                    fctx->streams[i]->id,
                    type,
                    width,
                    height,
                    pixel_format);
        }
    }

    fprintf(stderr, "\nAudio tracks:\n");

    for (unsigned i = 0; i < fctx->nb_streams; i++) {
        if (fctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            const char *type = "unknown";
            const AVCodecDescriptor *desc = avcodec_descriptor_get(fctx->streams[i]->codec->codec_id);
            if (desc)
                type = desc->long_name ? desc->long_name : desc->name;

            int64_t bit_rate, channel_layout, sample_rate;

            if (av_opt_get_int(fctx->streams[i]->codec, "ab", 0, &bit_rate) < 0)
                bit_rate = -1;

            channel_layout = getChannelLayout(fctx->streams[i]->codec);

            if (av_opt_get_int(fctx->streams[i]->codec, "ar", 0, &sample_rate) < 0)
                sample_rate = -1;

            char channels[512] = { 0 };
            av_get_channel_layout_string(channels, 512, 0, channel_layout);

            fprintf(stderr, "    Id: %x, type: %s, %d kbps, %s, %d Hz\n",
                    fctx->streams[i]->id,
                    type,
                    (int)(bit_rate / 1000),
                    channels,
                    (int)sample_rate);
        }
    }
}


struct CommandLine {
    bool help_wanted;

    bool version_wanted;

    bool info_wanted;

    bool stay_quiet;

    std::string d2v_path;

    std::vector<int> audio_ids;
    bool audio_ids_all;

    int video_id;
    bool have_video_id;

    D2V::ColourRange input_range;

    int ffmpeg_log_level;

    bool relative_paths;
    bool have_relative_paths;

    bool single_input;

    std::string error;

    CommandLine()
        : help_wanted(false)
        , version_wanted(false)
        , info_wanted(false)
        , stay_quiet(false)
        , d2v_path{ }
        , audio_ids{ }
        , audio_ids_all(false)
        , video_id(0)
        , have_video_id(false)
        , input_range(D2V::ColourRangeLimited)
        , ffmpeg_log_level(AV_LOG_PANIC)
        , relative_paths(KEY_DEFAULT_USE_RELATIVE_PATHS)
        , have_relative_paths(false)
        , single_input(false)
        , error{ }
    { }

    const std::string &getError() const {
        return error;
    }

    // char** or std::vector<std::string>
    template<typename Args>
    bool parse(int argc, Args argv, FakeFile &fake_file) {
        const char *opt_help = "--help";
        const char *opt_version = "--version";
        const char *opt_info = "--info";
        const char *opt_quiet = "--quiet";
        const char *opt_output = "--output";
        const char *opt_audio_ids = "--audio-ids";
        const char *opt_video_id = "--video-id";
        const char *opt_input_range = "--input-range";
        const char *opt_ffmpeg_log_level = "--ffmpeg-log-level";
        const char *opt_relative_paths = "--relative-paths";
        const char *opt_single_input = "--single-input";

        std::unordered_set<std::string> valid_options = {
            opt_help,
            opt_version,
            opt_info,
            opt_quiet,
            opt_output,
            opt_audio_ids,
            opt_video_id,
            opt_input_range,
            opt_ffmpeg_log_level,
            opt_relative_paths,
            opt_single_input,
        };

        for (int i = 1; i < argc; i++) {
            std::string arg(argv[i]);
            if (arg.size() > 1 && arg[0] == '-' && !valid_options.count(arg)) {
                error = "Unknown option '" + arg + "'. If this is the name of a file, please pass it as './" + arg + "'.";
                return false;
            }
        }

        for (int i = 1; i < argc; i++) {
            std::string arg(argv[i]);

            if (arg == opt_help) {
                help_wanted = true;
                return true;
            } else if (arg == opt_version) {
                version_wanted = true;
                return true;
            } else if (arg == opt_info) {
                info_wanted = true;
            } else if (arg == opt_quiet) {
                stay_quiet = true;
            } else if (arg == opt_output) {
                if (i == argc - 1 || valid_options.count(argv[i + 1])) {
                    error = opt_output;
                    error += " requires a file name.";
                    return false;
                }

                d2v_path = argv[i + 1];
                i++;
            } else if (arg == opt_audio_ids) {
                if (i == argc - 1 || valid_options.count(argv[i + 1])) {
                    error = opt_audio_ids;
                    error += " requires a list of audio track ids, or the special value 'all'.";
                    return false;
                }

                std::string ids(argv[i + 1]);
                i++;

                if (ids == "all") {
                    audio_ids_all = true;
                } else {
                    size_t id_start = 0, id_end;

                    do {
                        id_end = ids.find(',', id_start);
                        std::string id;
                        if (id_end == std::string::npos)
                            id = ids.substr(id_start);
                        else
                            id = ids.substr(id_start, id_end - id_start);

                        size_t converted_chars;
                        try {
                            audio_ids.push_back(std::stoi(id, &converted_chars, 16));
                        } catch (...) {
                            error = "Invalid audio id '" + id + "'.";
                            return false;
                        }

                        if (id.size() != converted_chars) {
                            error = "Audio id '" + id + "' is not a valid hexadecimal number.";
                            return false;
                        }

                        id_start = id_end + 1;
                    } while (id_end != std::string::npos);
                }
            } else if (arg == opt_video_id) {
                if (i == argc - 1 || valid_options.count(argv[i + 1])) {
                    error = opt_video_id;
                    error += " requires a video id.";
                    return false;
                }

                std::string id(argv[i + 1]);
                i++;

                size_t converted_chars;
                try {
                    video_id = std::stoi(id, &converted_chars, 16);
                } catch (...) {
                    error = "Invalid video id '" + id + "'.";
                    return false;
                }

                if (id.size() != converted_chars) {
                    error = "Video id '" + id + "' is not a valid hexadecimal number.";
                    return false;
                }

                have_video_id = true;
            } else if (arg == opt_input_range) {
                if (i == argc - 1 || valid_options.count(argv[i + 1])) {
                    error = opt_input_range;
                    error += " requires either 'limited' or 'full'";
                    return false;
                }

                std::unordered_map<std::string, D2V::ColourRange> range_map = {
                    { "limited", D2V::ColourRangeLimited },
                    { "full", D2V::ColourRangeFull }
                };

                try {
                    input_range = range_map.at(argv[i + 1]);
                    i++;
                } catch (std::out_of_range &) {
                    error = std::string("Input range '") + argv[i + 1] + "' is neither 'limited' nor 'full'.";
                    return false;
                }
            } else if (arg == opt_ffmpeg_log_level) {
                if (i == argc - 1 || valid_options.count(argv[i + 1])) {
                    error = opt_ffmpeg_log_level;
                    error += " requires one of 'quiet', 'panic', 'fatal', 'error', 'warning', 'info', 'verbose', 'debug', or 'trace'.";
                    return false;
                }

                std::unordered_map<std::string, int> log_map = {
                    { "quiet",      AV_LOG_QUIET },
                    { "panic",      AV_LOG_PANIC },
                    { "fatal",      AV_LOG_FATAL },
                    { "error",      AV_LOG_ERROR },
                    { "warning",    AV_LOG_WARNING },
                    { "info",       AV_LOG_INFO },
                    { "verbose",    AV_LOG_VERBOSE },
                    { "debug",      AV_LOG_DEBUG },
                    { "trace",      AV_LOG_TRACE }
                };

                try {
                    ffmpeg_log_level = log_map.at(argv[i + 1]);
                    i++;
                } catch (std::out_of_range &) {
                    error = std::string("FFMPEG log level '") + argv[i + 1] + "' is not one of 'quiet', 'panic', 'fatal', 'error', 'warning', 'info', 'verbose', 'debug', or 'trace'.";
                    return false;
                }
            } else if (arg == opt_relative_paths) {
                if (i == argc - 1 || valid_options.count(argv[i + 1])) {
                    error = opt_relative_paths;
                    error += " requires one of 'yes' or 'no'.";
                    return false;
                }

                std::unordered_map<std::string, bool> yes_no_map = {
                    { "yes", true },
                    { "no", false }
                };

                try {
                    relative_paths = yes_no_map.at(argv[i + 1]);
                    i++;
                } catch (std::out_of_range &) {
                    error = std::string("The value of ") + opt_relative_paths + " is neither 'yes' nor 'no'. Instead it is '" + argv[i + 1] + "'.";
                    return false;
                }

                have_relative_paths = true;
            } else if (arg == opt_single_input) {
                single_input = true;
            } else { // Input files.
                std::string err;
                makeAbsolute(arg, err);
                if (err.size()) {
                    error = "Failed to turn '" + arg + "' into an absolute path: " + err;
                    return false;
                }

                fake_file.push_back(arg);
            }
        }

        if (!fake_file.size()) {
            error = "No files given. Try '--help'.";
            return false;
        }

        return true;
    }
};


#ifdef _WIN32
static BOOL WINAPI HandlerRoutine(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        _exit(1);
    default:
        return FALSE;
    }
}
#endif


int main(int argc, char **_argv) {
#if !defined(Q_OS_WIN)
    // Can't put it with the other QSettings below because of the braces.
    QSettings settings("d2vwitch", "d2vwitch");
#endif

#if !defined(Q_OS_WIN) && !defined(Q_OS_DARWIN)
#define PROBABLY_USES_X_OR_WAYLAND 1
#endif


#ifdef PROBABLY_USES_X_OR_WAYLAND
    if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY")) {
#endif
        QApplication app(argc, _argv);
        QStringList arguments = QApplication::arguments();

#if defined(Q_OS_WIN)
        // Must put it here because QApplication::applicationDirPath returns ""
        // if it's called before a QApplication is created.
        QString ini_name = QApplication::applicationFilePath();
        int last_dot = ini_name.lastIndexOf('.');
        ini_name = ini_name.left(last_dot) + ".ini";
        QSettings settings(ini_name, QSettings::IniFormat);
#endif

        if (arguments.size() == 1) {
            // ffmpeg init part 0
            av_log_set_level(AV_LOG_PANIC);
            av_register_all();
            avcodec_register_all();

            app.setOrganizationName("d2vwitch");
            app.setApplicationName("d2vwitch");

            GUIWindow w(settings);

            w.show();

            return app.exec();
        }
#ifdef PROBABLY_USES_X_OR_WAYLAND
    }
#endif

#ifdef _WIN32
    AttachConsole(ATTACH_PARENT_PROCESS);

    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
    freopen("CON", "r", stdin);


    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        fprintf(stderr, "Failed to set stdout to binary mode.\n");

    SetConsoleCtrlHandler(HandlerRoutine, TRUE);


    std::vector<std::string> argv;
    argc = arguments.size();
    argv.reserve(argc);

    for (int i = 0; i < argc; i++)
        argv.push_back(arguments[i].toStdString());
#else
    char **argv = _argv;
#endif


    // command line parsing
    FakeFile fake_file;

    CommandLine cmd;
    if (!cmd.parse(argc, argv, fake_file)) {
        fprintf(stderr, "%s\n", cmd.getError().c_str());
        return 1;
    }

    if (!cmd.have_relative_paths)
        cmd.relative_paths = settings.value(KEY_USE_RELATIVE_PATHS, KEY_DEFAULT_USE_RELATIVE_PATHS).toBool();

    if (cmd.relative_paths && cmd.d2v_path == "-")
        cmd.relative_paths = false;


    if (cmd.help_wanted) {
        printHelp();
        return 0;
    }

    if (cmd.version_wanted) {
        printVersions();
        return 0;
    }


    // pick up the files in a VTS_xx_y.VOB sequence
    if (!cmd.single_input && fake_file.size() == 1) {
        QFileInfo info(QString::fromStdString(fake_file[0].name));

        QDir input_dir = info.dir();

        QStringList entries = input_dir.entryList(QStringList(QStringLiteral("vts_[0-9][0-9]_[0-9].vob")), QDir::Files, QDir::Name | QDir::IgnoreCase);

        if (entries.size()) {
            QString name = info.fileName();

            int index = entries.indexOf(name);

            if (index > -1) {
                for (int i = index + 1; i < entries.size(); i++) {
                    if (entries[i].midRef(4, 2) == name.midRef(4, 2))
                        fake_file.push_back(input_dir.absoluteFilePath(entries[i]).toStdString());
                }
            }
        }

    }


    // ffmpeg init part 0
    av_log_set_level(cmd.ffmpeg_log_level);
    av_register_all();
    avcodec_register_all();


    // input opening
    if (!fake_file.open()) {
        fprintf(stderr, "%s\n", fake_file.getError().c_str());

        fake_file.close();

        return 1;
    }


    FFMPEG f;

    // ffmpeg init part 1
    if (!f.initFormat(fake_file)) {
        fprintf(stderr, "%s\n", f.getError().c_str());

        f.cleanup();
        fake_file.close();

        return 1;
    }


    // info printing
    if (cmd.info_wanted) {
        printInfo(f.fctx, fake_file);

        f.cleanup();
        fake_file.close();

        return 0;
    }


    // container format check
    if (D2V::getStreamType(f.fctx->iformat->name) == D2V::UNSUPPORTED_STREAM) {
        fprintf(stderr, "Unsupported container type '%s'.\n", f.fctx->iformat->long_name ? f.fctx->iformat->long_name : f.fctx->iformat->name);

        f.cleanup();
        fake_file.close();

        return 1;
    }


    // stream selection
    f.deselectAllStreams();

    AVStream *video_stream;
    if (cmd.have_video_id) {
        video_stream = f.selectVideoStreamById(cmd.video_id);
        if (!video_stream) {
            fprintf(stderr, "Couldn't find video track with id %x.\n", cmd.video_id);

            f.cleanup();
            fake_file.close();

            return 1;
        }
    } else {
        video_stream = f.selectFirstVideoStream();
        if (!video_stream) {
            fprintf(stderr, "Couldn't find any video tracks.\n");

            f.cleanup();
            fake_file.close();

            return 1;
        }
    }

    if (cmd.audio_ids.size()) {
        if (!f.selectAudioStreamsById(cmd.audio_ids)) {
            for (size_t i = 0; i < cmd.audio_ids.size(); i++)
                fprintf(stderr, "Couldn't find audio track with id %x.\n", cmd.audio_ids[i]);

            f.cleanup();
            fake_file.close();

            return 1;
        }
    } else if (cmd.audio_ids_all) {
        if (!f.selectAllAudioStreams()) {
            fprintf(stderr, "Couldn't find any audio tracks.\n");

            f.cleanup();
            fake_file.close();

            return 1;
        }
    }


    if (!D2V::isSupportedVideoCodecID(video_stream->codec->codec_id)) {
        const char *type = "unknown";
        const AVCodecDescriptor *desc = avcodec_descriptor_get(video_stream->codec->codec_id);
        if (desc)
            type = desc->long_name ? desc->long_name : desc->name;

        fprintf(stderr, "Unsupported video codec: %s (id: %d)\n", type, video_stream->codec->codec_id);

        f.cleanup();
        fake_file.close();

        return 1;
    }


    // ffmpeg init part 2: audio and video decoders
    if (!f.initAudioCodecs()) {
        fprintf(stderr, "%s\n", f.getError().c_str());

        f.cleanup();
        fake_file.close();

        return 1;
    }

    if (!f.initVideoCodec(video_stream->index)) {
        fprintf(stderr, "%s\n", f.getError().c_str());

        f.cleanup();
        fake_file.close();

        return 1;
    }


    // calculate the audio delays if needed
    AudioDelayMap audio_delay_map;
    int64_t first_video_keyframe_pos = -1;
    if (cmd.audio_ids.size() || cmd.audio_ids_all) {
        std::string error;

        if (!calculateAudioDelays(fake_file, video_stream->id, audio_delay_map, &first_video_keyframe_pos, error)) {
            fprintf(stderr, "%s\n", error.c_str());

            f.cleanup();
            fake_file.close();

            return 1;
        }
    }


    // d2v file opening
    FILE *d2v_file;
    if (cmd.d2v_path == "-") {
        d2v_file = stdout;
    } else {
        if (!cmd.d2v_path.size())
            cmd.d2v_path = suggestD2VName(fake_file[0].name);

        d2v_file = openFile(cmd.d2v_path.c_str(), "wb");
        if (!d2v_file) {
            fprintf(stderr, "Failed to open d2v file '%s' for writing: %s\n", cmd.d2v_path.c_str(), strerror(errno));

            f.cleanup();
            fake_file.close();

            return 1;
        }
    }


    // audio files opening
    AudioFilesMap audio_files;
    for (unsigned i = 0; i < f.fctx->nb_streams; i++) {
        if (f.fctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
            f.fctx->streams[i]->discard != AVDISCARD_ALL) {

            std::string path = cmd.d2v_path;

            // chop off the extension if it exists
            size_t last_dot = path.find_last_of('.');
            if (last_dot != std::string::npos)
                path.erase(last_dot);

            path += suggestAudioTrackSuffix(f.fctx->streams[i], audio_delay_map);

            if (codecIDRequiresWave64(f.fctx->streams[i]->codec->codec_id)) {
                std::string error;

                AVFormatContext *w64_ctx = openWave64(path, f.fctx->streams[i]->codec, error);
                if (!w64_ctx) {
                    fprintf(stderr, "%s\n", error.c_str());

                    fclose(d2v_file);
                    closeAudioFiles(audio_files, f.fctx);
                    f.cleanup();
                    fake_file.close();

                    return 1;
                }

                audio_files.insert({ f.fctx->streams[i]->index, w64_ctx });
            } else {
                FILE *file = openFile(path.c_str(), "wb");
                if (!file) {
                    fprintf(stderr, "Failed to open audio file '%s' for writing: %s\n", path.c_str(), strerror(errno));

                    fclose(d2v_file);
                    closeAudioFiles(audio_files, f.fctx);
                    f.cleanup();
                    fake_file.close();

                    return 1;
                }

                audio_files.insert({ f.fctx->streams[i]->index, file });
            }
        }
    }


    // engage
    D2V::ProgressFunction progress_func = printProgress;
    D2V::LoggingFunction logging_func = printWarnings;
    if (cmd.stay_quiet) {
        progress_func = nullptr;
        logging_func = nullptr;
    }

    D2V d2v(cmd.d2v_path, d2v_file, audio_files, &fake_file, &f, video_stream, first_video_keyframe_pos, cmd.input_range, cmd.relative_paths, progress_func, nullptr, logging_func, nullptr);

    d2v.index();

    if (d2v.getResult() == D2V::ProcessingError) {
        fprintf(stderr, "%s\n", d2v.getError().c_str());

        f.cleanup();
        fake_file.close();

        return 1;
    }


    // some cleanup
    f.cleanup();
    fake_file.close();

    return 0;
}
