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


#include "FFMPEG.h"


FFMPEG::FFMPEG()
    : io_buffer(nullptr)
    , fctx(nullptr)
    , avcodec(nullptr)
    , avctx(nullptr)
    , parser(nullptr)
{ }


FFMPEG::~FFMPEG() {
    cleanup();
}


const std::string &FFMPEG::getError() const {
    return error;
}


bool FFMPEG::initFormat(FakeFile &fake_file) {
    fctx = avformat_alloc_context();
    if (!fctx) {
        error = "Couldn't allocate AVFormatContext.";
        return false;
    }

    size_t io_buffer_size = 128 * 2048;
    io_buffer = (uint8_t *)av_malloc(io_buffer_size);
    if (!io_buffer) {
        error = "Couldn't allocate " + std::to_string(io_buffer_size) + " bytes for the AVIOContext.";
        return false;
    }

    fctx->pb = avio_alloc_context(io_buffer, io_buffer_size, 0, &fake_file, FakeFile::readPacket, nullptr, FakeFile::seek);
    if (!fctx->pb) {
        error = "Couldn't allocate AVIOContext.";
        return false;
    }

    // These two are both needed in order to properly detect the
    // details of the audio streams from a certain DVD.
    // Values obtained by trial and error. Increase them if needed.
    // They add a bit of delay, but what can you do?
    fctx->probesize = 10 * 1000 * 1000; // bytes
    fctx->max_analyze_duration = 20 * 1000 * 1000; // microseconds

    int ret = avformat_open_input(&fctx, fake_file[0].name.c_str(), nullptr, nullptr);
    if (ret < 0) {
        error = "avformat_open_input() failed: ";
        char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        if (!av_strerror(ret, av_error, AV_ERROR_MAX_STRING_SIZE))
            error += av_error;
        else
            error += strerror(ret);

        return false;
    }

    ret = avformat_find_stream_info(fctx, nullptr);
    if (ret < 0) {
        error = "avformat_find_stream_info() failed: ";
        char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        if (!av_strerror(ret, av_error, AV_ERROR_MAX_STRING_SIZE))
            error += av_error;
        else
            error += strerror(ret);

        return false;
    }

    return true;
}


bool FFMPEG::initVideoCodec(int stream_index) {
    if (!fctx) {
        error = "Must call initFormat before initVideoCodec.";
        return false;
    }

    deinitVideoCodec();

    AVCodecID video_codec_id = fctx->streams[stream_index]->codecpar->codec_id;

    avcodec = avcodec_find_decoder(video_codec_id);
    if (!avcodec) {
        error = "Couldn't find decoder for ";
        error += avcodec_get_name(video_codec_id);
        return false;
    }

    avctx = avcodec_alloc_context3(avcodec);
    if (!avctx) {
        error = "Couldn't allocate AVCodecContext for the video decoder.";
        return false;
    }

    if (avcodec_parameters_to_context(avctx, fctx->streams[stream_index]->codecpar) < 0) {
        error = "Couldn't copy video codec parameters.";
        return false;
    }

    if (avcodec_open2(avctx, avcodec, nullptr) < 0) {
        error = "Couldn't open AVCodecContext for the video decoder.";
        return false;
    }

    parser = av_parser_init(video_codec_id);
    if (!parser) {
        error = "Couldn't initialise parser for ";
        error += avcodec_get_name(video_codec_id);
        return false;
    }

    parser->flags = PARSER_FLAG_COMPLETE_FRAMES;

    return true;
}


bool FFMPEG::initAudioCodec(int stream_index) {
    if (codecIDRequiresWave64(fctx->streams[stream_index]->codecpar->codec_id)) {
        AVCodecContext *ctx = avcodec_alloc_context3(nullptr);
        if (!ctx) {
            error = "Couldn't allocate AVCodecContext for an audio decoder.";
            return false;
        }

        if (avcodec_parameters_to_context(ctx, fctx->streams[stream_index]->codecpar) < 0) {
            error = "Couldn't copy AVCodecContext for an audio decoder.";
            return false;
        }

        const AVCodec *audio_decoder = avcodec_find_decoder(fctx->streams[stream_index]->codecpar->codec_id);
        if (!audio_decoder) {
            error = "Couldn't find decoder for ";
            error += avcodec_get_name(fctx->streams[stream_index]->codecpar->codec_id);
            return false;
        }

        if (avcodec_open2(ctx, audio_decoder, nullptr) < 0) {
            error = "Couldn't open AVCodecContext for an audio decoder.";
            return false;
        }

        audio_ctx.insert({ fctx->streams[stream_index]->index, ctx });
    }

    return true;
}


bool FFMPEG::initAudioCodecs() {
    for (unsigned i = 0; i < fctx->nb_streams; i++) {
        if (!initAudioCodec(i))
            return false;
    }

    return true;
}


void FFMPEG::deinitVideoCodec() {
    if (parser) {
        av_parser_close(parser);
        parser = nullptr;
    }


    if (avctx) {
        avcodec_close(avctx);
        avcodec_free_context(&avctx);
    }
}


void FFMPEG::cleanup() {
    deinitVideoCodec();


    if (fctx) {
        avio_context_free(&fctx->pb);
        avformat_close_input(&fctx);
    }


    for (auto it = audio_ctx.begin(); it != audio_ctx.end(); it++) {
        avcodec_close(it->second);
        avcodec_free_context(&it->second);
    }
}


AVStream *FFMPEG::selectVideoStreamById(int id) {
    for (unsigned i = 0; i < fctx->nb_streams; i++) {
        if (fctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (fctx->streams[i]->id == id) {
                fctx->streams[i]->discard = AVDISCARD_DEFAULT;
                return fctx->streams[i];
            }
        }
    }

    return nullptr;
}


AVStream *FFMPEG::selectFirstVideoStream() {
    for (unsigned i = 0; i < fctx->nb_streams; i++) {
        if (fctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            fctx->streams[i]->discard = AVDISCARD_DEFAULT;
            return fctx->streams[i];
        }
    }

    return nullptr;
}


bool FFMPEG::selectAudioStreamsById(const std::vector<int> &audio_ids, std::vector<int> &missing_audio_ids) {
    missing_audio_ids = audio_ids;

    for (int j = missing_audio_ids.size() - 1; j >= 0; j--) {
        for (unsigned i = 0; i < fctx->nb_streams; i++) {
            if (fctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                if (fctx->streams[i]->id == missing_audio_ids[j]) {
                    fctx->streams[i]->discard = AVDISCARD_DEFAULT;
                    missing_audio_ids.erase((missing_audio_ids.begin() + j));
                    break;
                }
            }
        }
    }

    return !missing_audio_ids.size();
}


bool FFMPEG::selectAllAudioStreams() {
    bool okay = false;

    for (unsigned i = 0; i < fctx->nb_streams; i++) {
        if (fctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            fctx->streams[i]->discard = AVDISCARD_DEFAULT;

            okay = true;
        }
    }

    return okay;
}


void FFMPEG::deselectAllStreams() {
    for (unsigned i = 0; i < fctx->nb_streams; i++)
        fctx->streams[i]->discard = AVDISCARD_ALL;
}


bool FFMPEG::seek(int64_t position) {
    int ret = avformat_seek_file(fctx, -1, INT64_MIN, position, INT64_MAX, AVSEEK_FLAG_BYTE);
    if (ret < 0) {
        error = "avformat_seek_file() failed to seek to position " + std::to_string(position) + ": ";
        char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        if (!av_strerror(ret, av_error, AV_ERROR_MAX_STRING_SIZE))
            error += av_error;
        else
            error += strerror(ret);

        return false;
    }

    return true;
}
