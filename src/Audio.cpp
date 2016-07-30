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


extern "C" {
#include <libavutil/opt.h>
}

#include "Audio.h"


AVFormatContext *openWave64(const std::string &path, const AVCodecContext *in_ctx, std::string &error) {
#define ERROR_SIZE 512
    char temp[ERROR_SIZE] = { 0 };

    AVFormatContext *w64_ctx = nullptr;
    int ret = avformat_alloc_output_context2(&w64_ctx, nullptr, "w64", path.c_str());
    if (ret < 0) {
        av_strerror(ret, temp, ERROR_SIZE);
        error = "Failed to allocate AVFormatContext for muxing the audio file '" + path + "': " + temp;

        return nullptr;
    }

    ret = avio_open2(&w64_ctx->pb, path.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
    if (ret < 0) {
        av_strerror(ret, temp, ERROR_SIZE);
        error = "Failed to open AVIOContext for audio file '" + path + "': " + temp;

        avformat_free_context(w64_ctx);

        return nullptr;
    }

    AVCodecID codec_id = av_get_pcm_codec(in_ctx->sample_fmt, 0);

    AVCodec *pcm_codec = avcodec_find_encoder(codec_id);
    if (!pcm_codec) {
        error = "Failed to find encoder for codec '" + std::string(avcodec_get_name(codec_id)) + "' (id " + std::to_string(codec_id) + ").";

        avformat_free_context(w64_ctx);

        return nullptr;
    }

    if (!avformat_new_stream(w64_ctx, pcm_codec)) {
        error = "Failed to create new AVStream for audio file '" + path + "'.";

        avformat_free_context(w64_ctx);

        return nullptr;
    }

    AVCodecContext *out_ctx = w64_ctx->streams[0]->codec;
    out_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    out_ctx->codec_id = codec_id;
    out_ctx->codec_tag = 0x0001;
    out_ctx->sample_rate = in_ctx->sample_rate;
    out_ctx->channels = in_ctx->channels;
    out_ctx->sample_fmt = in_ctx->sample_fmt;
    out_ctx->channel_layout = in_ctx->channel_layout;

    ret = avformat_write_header(w64_ctx, nullptr);
    if (ret < 0) {
        av_strerror(ret, temp, ERROR_SIZE);
#undef ERROR_SIZE
        error = "Failed to write Wave64 header to file '" + path + "': " + temp;

        avformat_free_context(w64_ctx);

        return nullptr;
    }

    return w64_ctx;
}


void closeAudioFiles(D2V::AudioFilesMap &audio_files, const AVFormatContext *fctx) {
    for (unsigned i = 0; i < fctx->nb_streams; i++) {
        try {
            void *pointer = audio_files.at(fctx->streams[i]->index);

            if (codecIDRequiresWave64(fctx->streams[i]->codec->codec_id)) {
                AVFormatContext *w64_ctx = (AVFormatContext *)pointer;

                av_write_trailer(w64_ctx);
                avformat_free_context(w64_ctx);
            } else {
                fclose((FILE *)pointer);
            }
        } catch (std::out_of_range &) {

        }
    }
}


const char *suggestAudioFileExtension(AVCodecID codec_id) {
    const char *extension = avcodec_get_name(codec_id);
    if (codecIDRequiresWave64(codec_id))
        extension = "w64";

    return extension;
}


int64_t getChannelLayout(AVCodecContext *avctx) {
    int64_t channel_layout = 0;

    av_opt_get_int(avctx, "channel_layout", 0, &channel_layout);

    if (channel_layout == 0) {
        int64_t channels = 0;
        av_opt_get_int(avctx, "ac", 0, &channels);
        channel_layout = av_get_default_channel_layout(channels);
    }

    return channel_layout;
}
