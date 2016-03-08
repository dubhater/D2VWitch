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
#include "MPEGParser.h"


class D2V {

public:
    enum StreamTypes {
        UNSUPPORTED_STREAM = -1,
        ELEMENTARY_STREAM = 0,
        PROGRAM_STREAM = 1,
        TRANSPORT_STREAM = 2,
        PVA_STREAM = 3
    };


    typedef void (*ProgressFunction)(int64_t current_position, int64_t total_size);
    typedef void (*LoggingFunction)(const std::string &message);
    typedef std::unordered_map<int, void *> AudioFilesMap;

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

    D2V(FILE *_d2v_file, const AudioFilesMap &_audio_files, FakeFile *_fake_file, FFMPEG *_f, AVStream *_video_stream, ProgressFunction _progress_report, LoggingFunction _log_message);

    const Stats &getStats() const;

    const std::string &getError() const;

    bool engage();

private:
    // 12 bits
    enum InfoField {
        INFO_BIT11 = (1 << 11),
        INFO_CLOSED_GOP = (1 << 10),
        INFO_PROGRESSIVE_SEQUENCE = (1 << 9),
        INFO_STARTS_NEW_GOP = (1 << 8)
        // The rest are reserved (0).
    };


    // 8 bits
    enum FlagsField {
        FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP = (1 << 7),
        FLAGS_PROGRESSIVE = (1 << 6),
        FLAGS_P_PICTURE = (1 << 5),
        FLAGS_I_PICTURE = (1 << 4),
        FLAGS_B_PICTURE = (3 << 4),
        // Reserved (0).
        // Reserved (0).
        FLAGS_TFF = (1 << 1),
        FLAGS_RFF = 1
    };


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
    AudioFilesMap audio_files;
    FakeFile* fake_file;
    FFMPEG *f;
    AVStream *video_stream;
    ProgressFunction progress_report;
    LoggingFunction log_message;

    MPEGParser parser;

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


static int getStreamType(const char *name) {
    std::unordered_map<std::string, int> stream_types_map = {
        { "mpegvideo", D2V::ELEMENTARY_STREAM },
        { "mpeg", D2V::PROGRAM_STREAM },
        { "mpegts", D2V::TRANSPORT_STREAM },
        { "pva", D2V::PVA_STREAM }
    };

    int stream_type;

    try {
        stream_type = stream_types_map.at(name);
    } catch (std::out_of_range &) {
        stream_type = D2V::UNSUPPORTED_STREAM;
    }

    return stream_type;
}


#endif // D2V_WITCH_D2V_H
