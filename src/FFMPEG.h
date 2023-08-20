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


#ifndef D2V_WITCH_FFMPEG_H
#define D2V_WITCH_FFMPEG_H


#include <string>
#include <unordered_map>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "FakeFile.h"


class FFMPEG {
    uint8_t *io_buffer;

    std::string error;

    void deinitVideoCodec();

public:
    AVFormatContext *fctx;
    const AVCodec *avcodec;
    AVCodecContext *avctx;
    AVCodecParserContext *parser;
    std::unordered_map<int, AVCodecContext *> audio_ctx;

    FFMPEG();
    ~FFMPEG();

    const std::string &getError() const;

    bool initFormat(FakeFile &fake_file);

    bool initVideoCodec(int stream_index);

    bool initAudioCodec(int stream_index);

    bool initAudioCodecs();

    void cleanup();

    AVStream *selectVideoStreamById(int id);

    AVStream *selectFirstVideoStream();

    bool selectAudioStreamsById(const std::vector<int> &audio_ids, std::vector<int> &missing_audio_ids);

    bool selectAllAudioStreams();

    void deselectAllStreams();

    bool seek(int64_t position);
};


static inline bool codecIDRequiresWave64(AVCodecID codec_id) {
    return codec_id == AV_CODEC_ID_PCM_BLURAY ||
           codec_id == AV_CODEC_ID_PCM_DVD;
}


#endif // D2V_WITCH_FFMPEG_H

