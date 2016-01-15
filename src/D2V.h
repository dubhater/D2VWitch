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


#ifndef D2V_WITCH_D2V_H
#define D2V_WITCH_D2V_H


#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

#include "FakeFile.h"
#include "FFMPEG.h"


// Magic numbers assigned to AVCodecParserContext::repeat_pict in libavcodec/mpegvideo_parser.c.
enum ParserMagicRepeatPict {
    PARSER_MAGIC_NO_REPEAT0 = 0,
    PARSER_MAGIC_NO_REPEAT1 = 1,
    PARSER_MAGIC_RFF_3_FIELDS = 2, // make 3 fields out of 1 progressive frame
    PARSER_MAGIC_RFF_2_FRAMES = 3, // make 2 frames out of 1 progressive frame
    PARSER_MAGIC_RFF_3_FRAMES = 5  // make 3 frames out of 1 progressive frame
};


enum D2VStreamTypes {
    D2V_STREAM_UNSUPPORTED = -1,
    D2V_STREAM_ELEMENTARY = 0,
    D2V_STREAM_PROGRAM = 1,
    D2V_STREAM_TRANSPORT = 2,
    D2V_STREAM_PVA = 3
};


// 12 bits
enum D2VInfoField {
    D2V_INFO_BIT11 = (1 << 11),
    D2V_INFO_CLOSED_GOP = (1 << 10),
    D2V_INFO_PROGRESSIVE_SEQUENCE = (1 << 9),
    D2V_INFO_STARTS_NEW_GOP = (1 << 8)
    // The rest are reserved (0).
};


// 8 bits
enum D2VFlagsField {
    D2V_FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP = (1 << 7),
    D2V_FLAGS_PROGRESSIVE = (1 << 6),
    D2V_FLAGS_P_PICTURE = (1 << 5),
    D2V_FLAGS_I_PICTURE = (1 << 4),
    D2V_FLAGS_B_PICTURE = (3 << 4),
    // Reserved (0).
    // Reserved (0).
    D2V_FLAGS_TFF = (1 << 1),
    D2V_FLAGS_RFF = 1
};


static int getStreamType(const char *name) {
    std::unordered_map<std::string, int> stream_types_map = {
        { "mpegvideo", D2V_STREAM_ELEMENTARY },
        { "mpeg", D2V_STREAM_PROGRAM },
        { "mpegts", D2V_STREAM_TRANSPORT },
        { "pva", D2V_STREAM_PVA }
    };

    int stream_type;

    try {
        stream_type = stream_types_map.at(name);
    } catch (std::out_of_range &) {
        stream_type = D2V_STREAM_UNSUPPORTED;
    }

    return stream_type;
}


class D2V {

public:
    typedef void (*ProgressFunction)(int64_t current_position, int64_t total_size);
    typedef void (*LoggingFunction)(const std::string &message);

    struct Stats {
        int video_frames;
        int progressive_frames;
        int tff_frames;
        int rff_frames;

        Stats()
            : video_frames(0)
            , progressive_frames(0)
            , tff_frames(0)
            , rff_frames(0)
        { }
    };

    D2V(FILE *_d2v_file, const std::unordered_map<int, FILE *> &_audio_files, FakeFile *_fake_file, FFMPEG *_f, AVStream *_video_stream, ProgressFunction _progress_report, LoggingFunction _log_message);

    const Stats &getStats() const;

    const std::string &getError() const;

    bool engage();

private:
    struct DataLine {
        int info;
        int matrix;
        int file;
        int64_t position;
        int skip;
        int vob;
        int cell;
        std::vector<uint8_t> flags;

        DataLine()
            : info(0)
            , matrix(0)
            , file(0)
            , position(0)
            , skip(0)
            , vob(0)
            , cell(0)
            , flags{ }
        { }
    };

    FILE *d2v_file;
    std::unordered_map<int, FILE *> audio_files;
    FakeFile* fake_file;
    FFMPEG *f;
    AVStream *video_stream;
    ProgressFunction progress_report;
    LoggingFunction log_message;

    DataLine line;

    Stats stats;

    std::string error;


    void clearDataLine();

    bool isDataLineNull() const;

    void reorderDataLineFlags();

    bool printHeader();

    bool printSettings();

    bool printDataLine();

    bool handleVideoPacket(AVPacket *packet);

    bool handleAudioPacket(AVPacket *packet);

    bool printStreamEnd();
};


#endif // D2V_WITCH_D2V_H

