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


#include <stdexcept>

extern "C" {
#include <libavutil/opt.h>
}

#include "Audio.h"
#include "FFMPEG.h"
#include "MPEGParser.h"


AVFormatContext *openWave64(const std::string &path, const AVCodecParameters *in_par, std::string &error) {
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

    AVCodecID codec_id = av_get_pcm_codec(static_cast<AVSampleFormat>(in_par->format), 0);

    const AVCodec *pcm_codec = avcodec_find_encoder(codec_id);
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

    AVCodecContext *out_ctx = avcodec_alloc_context3(pcm_codec);
    if (!out_ctx) {
        error = "Failed to create new AVCodecContext for audio file '" + path + "'.";

        avformat_free_context(w64_ctx);

        return nullptr;
    }

    out_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    out_ctx->codec_id = codec_id;
    out_ctx->codec_tag = 0x0001;
    out_ctx->sample_rate = in_par->sample_rate;
    out_ctx->channels = in_par->channels;
    out_ctx->sample_fmt = static_cast<AVSampleFormat>(in_par->format);
    out_ctx->channel_layout = in_par->channel_layout;

    ret = avcodec_open2(out_ctx, pcm_codec, nullptr);
    if (ret < 0) {
        error = "Failed to open codec for audio file '" + path + "'";

        avformat_free_context(w64_ctx);
        avcodec_free_context(&out_ctx);

        return nullptr;
    }
    avcodec_parameters_from_context(w64_ctx->streams[0]->codecpar, out_ctx);
    if (ret < 0) {
        error = "Failed to copy codec parameters for audio file '" + path + "'";

        avformat_free_context(w64_ctx);
        avcodec_free_context(&out_ctx);

        return nullptr;
    }

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


void closeAudioFiles(AudioFilesMap &audio_files, const AVFormatContext *fctx) {
    for (unsigned i = 0; i < fctx->nb_streams; i++) {
        try {
            void *pointer = audio_files.at(fctx->streams[i]->index);

            if (codecIDRequiresWave64(fctx->streams[i]->codecpar->codec_id)) {
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


int64_t getChannelLayout(AVCodecParameters *avpar) {
    int64_t channel_layout = avpar->channel_layout;

    if (channel_layout == 0) {
        int64_t channels = avpar->channels;
        channel_layout = av_get_default_channel_layout(channels);
    }

    return channel_layout;
}


bool calculateAudioDelays(FakeFile &fake_file, int video_stream_id, AudioDelayMap &audio_delay_map, int64_t *first_video_keyframe_pos, std::string &error) {
    const char *error_prefix = "Failed to calculate audio delays: ";

    int64_t original_position = fake_file.getCurrentPosition();

    FakeFile::seek(&fake_file, 0, SEEK_SET);

    FFMPEG f;

    if (!f.initFormat(fake_file)) {
        error = error_prefix + f.getError();

        f.cleanup();
        FakeFile::seek(&fake_file, original_position, SEEK_SET);

        return false;
    }

    int video_stream_index = 0;

    for (unsigned i = 0; i < f.fctx->nb_streams; i++) {
        if (f.fctx->streams[i]->id == video_stream_id) {
            video_stream_index = i;
        } else if (f.fctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_delay_map[f.fctx->streams[i]->id] = AV_NOPTS_VALUE;
        }
    }

    size_t audio_streams_left = audio_delay_map.size();

    // Quit early if there are no audio streams.
    if (audio_streams_left == 0) {
        f.cleanup();
        FakeFile::seek(&fake_file, original_position, SEEK_SET);

        return true;
    }

    if (!f.initVideoCodec(video_stream_index)) {
        error = error_prefix + f.getError();

        f.cleanup();
        FakeFile::seek(&fake_file, original_position, SEEK_SET);

        return false;
    }

    // Smallest video timestamp found in between (in coded order) the first two keyframes.
    int64_t first_video_pts = AV_NOPTS_VALUE;


    bool second_keyframe_reached = false;

    AVPacket packet;
    av_init_packet(&packet);

    struct AudioPacketDetails {
         int64_t pos;
         int64_t pts;
    };

    std::unordered_map<int, std::vector<AudioPacketDetails> > audio_packet_details_map;

    // av_read_frame may not return packets from different streams in order (packet.pos always increasing)
    while ((audio_streams_left != 0 || !second_keyframe_reached) && av_read_frame(f.fctx, &packet) == 0) {
        if (packet.stream_index == video_stream_index) {
            AVCodecID codec_id = f.fctx->streams[packet.stream_index]->codecpar->codec_id;

            if (codec_id == AV_CODEC_ID_H264) {
                uint8_t *output_buffer; /// free this?
                int output_buffer_size;

                while (packet.size) {
                    int parsed_bytes = av_parser_parse2(f.parser, f.avctx, &output_buffer, &output_buffer_size,
                                                        packet.data, packet.size,
                                                        packet.pts, packet.dts, packet.pos);

                    packet.data += parsed_bytes;
                    packet.size -= parsed_bytes;
                }
            } else {
                d2vWitchParseMPEG12Data(f.parser, f.avctx, packet.data, packet.size);
            }

            if (f.parser->width > 0 && f.parser->height > 0) {
                if (f.parser->key_frame) {
                    if (first_video_pts == AV_NOPTS_VALUE) {
                        first_video_pts = packet.pts;
                        *first_video_keyframe_pos = packet.pos;
                    } else {
                        second_keyframe_reached = true;
                    }
                } else if (first_video_pts != AV_NOPTS_VALUE) {
                    if (packet.pts < first_video_pts && packet.pts != AV_NOPTS_VALUE) {
                        first_video_pts = packet.pts;
                    }
                }
            }
        } else {
            if (f.fctx->streams[packet.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                int id = f.fctx->streams[packet.stream_index]->id;

                AVRational time_base = f.fctx->streams[packet.stream_index]->time_base;
                int64_t pts = packet.pts * 1000 * time_base.num / time_base.den;

                if (first_video_pts == AV_NOPTS_VALUE) {
                    // We haven't reached the first video keyframe yet, so just buffer the audio packet details.
                    audio_packet_details_map[id].push_back({ packet.pos, pts });
                } else {
                    auto latest_packet = audio_packet_details_map[id].crbegin();

                    if (latest_packet != audio_packet_details_map[id].crend() && latest_packet->pos >= *first_video_keyframe_pos) {
                        if (audio_streams_left > 0)
                            audio_streams_left--;
                    } else {
                        audio_packet_details_map[id].push_back({ packet.pos, pts });
                    }
                }
            }
        }

        av_packet_unref(&packet);
    }

    AVRational time_base = f.fctx->streams[video_stream_index]->time_base;

    first_video_pts = first_video_pts * 1000 * time_base.num / time_base.den;

    for (auto it = audio_packet_details_map.cbegin(); it != audio_packet_details_map.cend(); it++) {
        for (size_t i = 0; i < it->second.size(); i++) {
            if (it->second[i].pos >= *first_video_keyframe_pos) {
                audio_delay_map[it->first] = it->second[i].pts - first_video_pts;
                break;
            }
        }
    }

    f.cleanup();

    FakeFile::seek(&fake_file, original_position, SEEK_SET);

    return true;
}
