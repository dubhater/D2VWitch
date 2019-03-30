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

    enum ProcessingResult {
        ProcessingFinished,
        ProcessingCancelled,
        ProcessingError
    };

    enum ColourRange {
        ColourRangeLimited = 0,
        ColourRangeFull
    };


    typedef void (*ProgressFunction)(int64_t current_position, int64_t total_size, void *progress_data);
    typedef void (*LoggingFunction)(const std::string &message, void *log_data);
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

    D2V();

    D2V(const std::string &_d2v_file_name, FILE *_d2v_file, const AudioFilesMap &_audio_files, FakeFile *_fake_file, FFMPEG *_f, AVStream *_video_stream, ColourRange _input_range, bool _use_relative_paths, ProgressFunction _progress_report, void *_progress_data, LoggingFunction _log_message, void *_log_data);

    const std::string &getD2VFileName() const;

    const Stats &getStats() const;

    const std::string &getError() const;

    void index();

    void demuxVideo(FILE *video_file, int64_t start_gop_position, int64_t end_gop_position);

    ProcessingResult getResult() const;

    int getGOPStartFrame(int frame) const;
    int getNextGOPStartFrame(int frame) const;

    int64_t getGOPStartPosition(int frame) const;
    int64_t getNextGOPStartPosition(int frame) const;

    bool isOpenGOP(int frame) const;

    int getNumFrames() const;

    static int getStreamType(const char *name);

    static bool isSupportedVideoCodecID(AVCodecID id);

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


    struct Picture {
        int output_picture_number; // Only used for h264.
        AVPictureStructure picture_structure; // Only used for h264.
        uint8_t flags;
    };


    struct DataLine {
        int info;
        int matrix;
        int file;
        int64_t position;
        int skip;
        int vob;
        int cell;
        std::vector<Picture> pictures;

        DataLine()
            : info(0)
            , matrix(0)
            , file(0)
            , position(0)
            , skip(0)
            , vob(0)
            , cell(0)
            , pictures{ }
        { }
    };

    std::string d2v_file_name;
    FILE *d2v_file;
    AudioFilesMap audio_files;
    FakeFile* fake_file;
    FFMPEG *f;
    AVStream *video_stream;
    ColourRange input_range;
    bool use_relative_paths;
    ProgressFunction progress_report;
    void *progress_data;
    LoggingFunction log_message;
    void *log_data;

    DataLine line;

    int64_t previous_pts; // For frame rate guessing.
    AVRational guessed_frame_rate;

    Stats stats;

    std::string error;

    ProcessingResult result;

    std::vector<DataLine> lines;


    void clearDataLine();

    bool isDataLineNull() const;

    void reorderDataLineFlags();

    bool printHeader();

    bool printSettings();

    bool printDataLine(const DataLine &data_line);

    bool handleVideoPacket(AVPacket *packet);

    bool handleAudioPacket(AVPacket *packet);

    bool printStreamEnd();
};


std::string suggestD2VName(const std::string &video_name);
std::string suggestAudioTrackSuffix(const AVStream *stream);

#endif // D2V_WITCH_D2V_H
