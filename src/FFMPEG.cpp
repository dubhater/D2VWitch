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

    size_t io_buffer_size = 32 * 1024;
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


bool FFMPEG::initCodec(AVCodecID video_codec_id) {
    avcodec = avcodec_find_decoder(video_codec_id);
    if (!avcodec) {
        error = "Couldn't find decoder for ";
        error += avcodec_get_name(video_codec_id);
        return false;
    }

    avctx = avcodec_alloc_context3(avcodec);
    if (!avctx) {
        error = "Couldn't allocate AVCodecContext.";
        return false;
    }

    if (avcodec_open2(avctx, avcodec, nullptr) < 0) {
        error = "Couldn't open AVCodecContext.";
        return false;
    }

    return true;
}


void FFMPEG::cleanup() {
    if (avctx) {
        avcodec_close(avctx);
        avcodec_free_context(&avctx);
    }


    if (fctx)
        avformat_close_input(&fctx);
}
