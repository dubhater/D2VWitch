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


#include <cinttypes>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include "D2V.h"


void D2V::clearDataLine() {
    line.info = 0;
    line.matrix = 0;
    line.file = 0;
    line.position = 0;
    line.skip = 0;
    line.vob = 0;
    line.cell = 0;
    line.flags.clear();
}


bool D2V::isDataLineNull() const {
    return !line.flags.size();
}


void D2V::reorderDataLineFlags() {
    if (!line.flags.size())
        return;

    for (size_t i = 1; i < line.flags.size(); i++) {
        if ((line.flags[i - 1] & D2V_FLAGS_B_PICTURE) != D2V_FLAGS_B_PICTURE &&
                (line.flags[i] & D2V_FLAGS_B_PICTURE) == D2V_FLAGS_B_PICTURE)
            std::swap(line.flags[i - 1], line.flags[i]);
    }
}


bool D2V::printHeader() {
    std::string header;

    header += "DGIndexProjectFile16\n";
    header += std::to_string(fake_file->size()) + "\n";

    for (auto it = fake_file->cbegin(); it != fake_file->cend(); it++)
        header += it->name + "\n";

    header += "\n";

    if (fprintf(d2v_file, "%s", header.c_str()) < 0) {
        error = "Failed to print d2v header section: fprintf() failed.";
        return false;
    }

    return true;
}


bool D2V::printSettings() {
    int stream_type = getStreamType(f->fctx->iformat->name);

    int video_id = video_stream->id;
    int audio_id = 0;
    int64_t ts_packetsize;
    if (stream_type == D2V_STREAM_TRANSPORT) {
        const AVStream *audio_stream = nullptr;
        for (unsigned i = 0; i < f->fctx->nb_streams; i++) {
            if (f->fctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream = f->fctx->streams[i];
                break;
            }
        }

        if (audio_stream)
            audio_id = audio_stream->id;

        if (av_opt_get_int(f->fctx, "ts_packetsize", AV_OPT_SEARCH_CHILDREN, &ts_packetsize) < 0)
            ts_packetsize = 0;
    }

    int mpeg_type = 0;
    if (video_stream->codec->codec_id == AV_CODEC_ID_MPEG1VIDEO)
        mpeg_type = 1;
    else if (video_stream->codec->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        mpeg_type = 2;

    int width, height;
    if (av_opt_get_image_size(video_stream->codec, "video_size", 0, &width, &height) < 0)
        width = height = -1;

    AVRational sar;
    if (av_opt_get_q(video_stream->codec, "aspect", 0, &sar) < 0)
        sar = { 1, 1 };
    AVRational dar = av_mul_q(av_make_q(width, height), sar);
    av_reduce(&dar.num, &dar.den, dar.num, dar.den, 1024);

    // No AVOption for framerate?
    AVRational frame_rate = video_stream->codec->framerate;

    std::string settings;

    settings += "Stream_Type=" + std::to_string(stream_type) + "\n";
    if (stream_type == D2V_STREAM_TRANSPORT) {
        char pids[100] = { 0 };
        snprintf(pids, 100, "%x,%x,%x", video_id, audio_id, 0);
        settings += "MPEG2_Transport_PID=";
        settings += pids;
        settings += "\n";

        settings += "Transport_Packet_Size=" + std::to_string(ts_packetsize) + "\n";
    }
    settings += "MPEG_Type=" + std::to_string(mpeg_type) + "\n";
    settings += "iDCT_Algorithm=6\n"; // "32-bit SSEMMX (Skal)". No one cares anyway.
    settings += "YUVRGB_Scale=0\n"; // "TV Scale".
    settings += "Luminance_Filter=0,0\n"; // We don't care.
    settings += "Clipping=0,0,0,0\n"; // We don't crop here.
    settings += "Aspect_Ratio=" + std::to_string(dar.num) + ":" + std::to_string(dar.den) + "\n";
    settings += "Picture_Size=" + std::to_string(width) + "x" + std::to_string(height) + "\n";
    settings += "Field_Operation=0\n"; // Always tell them honor the pulldown flags.
    settings += "Frame_Rate=" + std::to_string((int)((float)frame_rate.num * 1000 / frame_rate.den)) + " (" + std::to_string(frame_rate.num) + "/" + std::to_string(frame_rate.den) + ")\n";
    settings += "Location=0,0,0,0\n"; // Whatever.

    if (fprintf(d2v_file, "%s", settings.c_str()) < 0) {
        error = "Failed to print d2v settings section: fprintf() failed.";
        return false;
    }

    return true;
}


bool D2V::printDataLine() {
    if (fprintf(d2v_file, "\n%x %d %d %" PRId64 " %d %d %d",
                line.info,
                line.matrix,
                line.file,
                line.position,
                line.skip,
                line.vob,
                line.cell) < 0) {
        error = "Failed to print d2v data line: fprintf() failed.";
        return false;
    }

    for (auto it = line.flags.cbegin(); it != line.flags.cend(); it++) {
        if (fprintf(d2v_file, " %x", (int)*it) < 0) {
            error = "Failed to print d2v data line: fprintf() failed.";
            return false;
        }
    }

    return true;
}


bool D2V::handleVideoPacket(AVPacket *packet) {
    uint8_t *output_buffer;
    int output_buffer_size;

    while (packet->size) {
        int parsed_bytes = av_parser_parse2(f->parser, f->avctx, &output_buffer, &output_buffer_size,
                                            packet->data, packet->size, packet->pts, packet->dts, packet->pos);

        packet->data += parsed_bytes;
        packet->size -= parsed_bytes;
    }

    uint8_t flags = 0;

    if (!f->parser->width || !f->parser->height) {
        if (log_message)
            log_message("Encountered frame with invalid dimensions " + std::to_string(f->parser->width) + "x" + std::to_string(f->parser->height) + ".");

        return true;
    }

    if (f->parser->pict_type == AV_PICTURE_TYPE_I) {
        if (!isDataLineNull()) {
            reorderDataLineFlags();
            if (!printDataLine())
                return false;
            clearDataLine();
        }

        line.info = D2V_INFO_BIT11 | D2V_INFO_STARTS_NEW_GOP | D2V_INFO_CLOSED_GOP;

        int64_t colorspace;
        // Pointless, it's always AVCOL_SPC_UNSPECIFIED.
        if (av_opt_get_int(f->avctx, "colorspace", 0, &colorspace) < 0)
            colorspace = AVCOL_SPC_UNSPECIFIED;
        line.matrix = colorspace;

        line.file = fake_file->getFileIndex(f->parser->pos);
        line.position = fake_file->getPositionInRealFile(f->parser->pos);

        flags = D2V_FLAGS_I_PICTURE | D2V_FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP;

        if (progress_report)
            progress_report(packet->pos, fake_file->getTotalSize());
    } else if (f->parser->pict_type == AV_PICTURE_TYPE_P) {
        flags = D2V_FLAGS_P_PICTURE | D2V_FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP;
    } else if (f->parser->pict_type == AV_PICTURE_TYPE_B) {
        flags = D2V_FLAGS_B_PICTURE;

        int reference_pictures = 0;
        for (auto it = line.flags.cbegin(); it != line.flags.cend(); it++) {
            uint8_t frame_type = *it & D2V_FLAGS_B_PICTURE;

            if (frame_type == D2V_FLAGS_I_PICTURE || frame_type == D2V_FLAGS_P_PICTURE)
                reference_pictures++;
        }

        // av_read_frame returns *frames*, so this works fine even when picture_structure is "field",
        // i.e. when the pictures in the mpeg2 stream are individual fields.
        if (reference_pictures >= 2) {
            flags |= D2V_FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP;
        } else {
            line.info &= ~D2V_INFO_CLOSED_GOP;
        }
    } else {
        if (log_message)
            log_message(std::string("Encountered unknown picture type ") +  av_get_picture_type_char((AVPictureType)f->parser->pict_type) + " (" + std::to_string(f->parser->pict_type) + ").");

        return true;
    }

    if (f->parser->repeat_pict == PARSER_MAGIC_RFF_3_FIELDS ||
        f->parser->repeat_pict == PARSER_MAGIC_RFF_2_FRAMES ||
        f->parser->repeat_pict == PARSER_MAGIC_RFF_3_FRAMES) {
        flags |= D2V_FLAGS_RFF;
        flags |= D2V_FLAGS_PROGRESSIVE;
    }

    if (f->parser->field_order == AV_FIELD_TT ||
        f->parser->repeat_pict == PARSER_MAGIC_RFF_3_FRAMES)
        flags |= D2V_FLAGS_TFF;

    if (f->parser->field_order == AV_FIELD_PROGRESSIVE)
        flags |= D2V_FLAGS_PROGRESSIVE;

    line.flags.push_back(flags);

    stats.video_frames++;
    if (flags & D2V_FLAGS_PROGRESSIVE)
        stats.progressive_frames++;
    if (flags & D2V_FLAGS_TFF)
        stats.tff_frames++;
    if (flags & D2V_FLAGS_RFF)
        stats.rff_frames++;

    return true;
}


bool D2V::handleAudioPacket(AVPacket *packet) {
    FILE *file = audio_files.at(packet->stream_index);

    if (fwrite(packet->data, 1, packet->size, file) < (size_t)packet->size) {
        char id[20] = { 0 };
        snprintf(id, 19, "%x", f->fctx->streams[packet->stream_index]->id);
        error = "Failed to write audio packet from stream id ";
        error += id;
        error += ": fwrite() failed.";

        return false;
    }

    return true;
}


bool D2V::printStreamEnd() {
    if (fprintf(d2v_file, " ff\n") < 0) {
        error = "Failed to print the d2v stream end flag: fprintf() failed.";
        return false;
    }

    return true;
}


D2V::D2V(FILE *_d2v_file, const std::unordered_map<int, FILE *> &_audio_files, FakeFile *_fake_file, FFMPEG *_f, AVStream *_video_stream, ProgressFunction _progress_report, LoggingFunction _log_message)
    : d2v_file(_d2v_file)
    , audio_files(_audio_files)
    , fake_file(_fake_file)
    , f(_f)
    , video_stream(_video_stream)
    , progress_report(_progress_report)
    , log_message(_log_message)
{ }


const D2V::Stats &D2V::getStats() const {
    return stats;
}


const std::string &D2V::getError() const {
    return error;
}


bool D2V::engage() {
    if (!printHeader())
        return false;

    if (!printSettings())
        return false;

    AVPacket packet;
    av_init_packet(&packet);

    while (av_read_frame(f->fctx, &packet) == 0) {
        // Apparently we might receive packets from streams with AVDISCARD_ALL set,
        // and also from streams discovered late, probably.
        if (packet.stream_index != video_stream->index &&
            !audio_files.count(packet.stream_index))
            continue;

        bool okay = true;

        if (packet.stream_index == video_stream->index)
            okay = handleVideoPacket(&packet);
        else
            okay = handleAudioPacket(&packet);

        if (!okay)
            return false;

        av_free_packet(&packet);
    }

    if (!isDataLineNull()) {
        reorderDataLineFlags();
        if (!printDataLine())
            return false;
        clearDataLine();
    }

    if (!printStreamEnd())
        return false;

    return true;
}
