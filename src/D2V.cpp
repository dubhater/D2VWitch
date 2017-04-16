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


#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <unordered_set>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include "Audio.h"
#include "D2V.h"


void D2V::clearDataLine() {
    line.info = 0;
    line.matrix = 0;
    line.file = 0;
    line.position = 0;
    line.skip = 0;
    line.vob = 0;
    line.cell = 0;
    line.pictures.clear();
}


bool D2V::isDataLineNull() const {
    return !(line.pictures.size() && (line.info & INFO_BIT11));
}


void D2V::reorderDataLineFlags() {
    if (!line.pictures.size())
        return;

    if (video_stream->codec->codec_id == AV_CODEC_ID_H264) {
        auto cmp = [] (const Picture &p1, const Picture &p2) -> bool {
            return p1.output_picture_number < p2.output_picture_number;
        };

        std::sort(line.pictures.begin(), line.pictures.end(), cmp);
    } else {
        for (size_t i = 1; i < line.pictures.size(); i++) {
            if ((line.pictures[i - 1].flags & FLAGS_B_PICTURE) != FLAGS_B_PICTURE &&
                    (line.pictures[i].flags & FLAGS_B_PICTURE) == FLAGS_B_PICTURE)
                std::swap(line.pictures[i - 1], line.pictures[i]);
        }
    }
}


bool D2V::printHeader() {
    std::string header;

    header += "DGIndexProjectFile";
    header += std::to_string(video_stream->codec->codec_id == AV_CODEC_ID_H264 ? 42 : 16) + "\n";

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
    int64_t ts_packetsize = 0;
    if (stream_type == TRANSPORT_STREAM) {
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
    else if (video_stream->codec->codec_id == AV_CODEC_ID_H264)
        mpeg_type = 264;

    int yuvrgb_scale = input_range == ColourRangeLimited ? 1 : 0;

    int width, height;
    if (av_opt_get_image_size(video_stream->codec, "video_size", 0, &width, &height) < 0)
        width = height = -1;

    AVRational sar;
    if (av_opt_get_q(video_stream->codec, "aspect", 0, &sar) < 0 || sar.num < 1 || sar.den < 1)
        sar = { 1, 1 };
    AVRational dar = av_mul_q(av_make_q(width, height), sar);
    av_reduce(&dar.num, &dar.den, dar.num, dar.den, 1024);

    // No AVOption for framerate?
    AVRational frame_rate = video_stream->codec->framerate;
    if (frame_rate.num <= 0 || frame_rate.den <= 0) {
        if (guessed_frame_rate.num > 0 && guessed_frame_rate.den > 0) {
            frame_rate = guessed_frame_rate;
        } else {
            frame_rate.num = 42;
            frame_rate.den = 1;
        }
    }

    std::string settings;

    settings += "Stream_Type=" + std::to_string(stream_type) + "\n";
    if (stream_type == TRANSPORT_STREAM) {
        char pids[100] = { 0 };
        snprintf(pids, 100, "%x,%x,%x", video_id, audio_id, 0);
        settings += "MPEG2_Transport_PID=";
        settings += pids;
        settings += "\n";

        settings += "Transport_Packet_Size=" + std::to_string(ts_packetsize) + "\n";
    }
    settings += "MPEG_Type=" + std::to_string(mpeg_type) + "\n";
    settings += "iDCT_Algorithm=6\n"; // "32-bit SSEMMX (Skal)". No one cares anyway.
    settings += "YUVRGB_Scale=" + std::to_string(yuvrgb_scale) + "\n";
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


bool D2V::printDataLine(const D2V::DataLine &data_line) {
    if (fprintf(d2v_file, "\n%x %d %d %" PRId64 " %d %d %d",
                data_line.info,
                data_line.matrix,
                data_line.file,
                data_line.position,
                data_line.skip,
                data_line.vob,
                data_line.cell) < 0) {
        error = "Failed to print d2v data line: fprintf() failed.";
        return false;
    }

    for (auto it = data_line.pictures.cbegin(); it != data_line.pictures.cend(); it++) {
        if (fprintf(d2v_file, " %x", (int)it->flags) < 0) {
            error = "Failed to print d2v data line: fprintf() failed.";
            return false;
        }
    }

    return true;
}


bool D2V::handleVideoPacket(AVPacket *packet) {
    Picture picture = { 0, AV_PICTURE_STRUCTURE_UNKNOWN, 0 };

    AVCodecID codec_id = f->fctx->streams[packet->stream_index]->codec->codec_id;

    if (codec_id == AV_CODEC_ID_H264) {
        uint8_t *output_buffer; /// free this?
        int output_buffer_size;

        while (packet->size) {
            int parsed_bytes = av_parser_parse2(f->parser, f->avctx, &output_buffer, &output_buffer_size,
                                                packet->data, packet->size,
                                                packet->pts, packet->dts, packet->pos);

            packet->data += parsed_bytes;
            packet->size -= parsed_bytes;
        }
    } else {
        d2vWitchParseMPEG12Data(f->parser, f->avctx, packet->data, packet->size);
    }

    if (f->parser->width <= 0 || f->parser->height <= 0) {
        if (log_message)
            log_message("Skipping frame with invalid dimensions " + std::to_string(f->parser->width) + "x" + std::to_string(f->parser->height) + ".", log_data);

        return true;
    }

    bool first_gop = lines.size() == 0;
    bool first_picture = line.pictures.size() == 0;

    if (first_gop &&
        first_picture &&
        !f->parser->key_frame) {
        if (log_message)
            log_message("Skipping leading non-keyframe.", log_data);

        return true;
    }

    bool mpeg12 = codec_id == AV_CODEC_ID_MPEG1VIDEO || codec_id == AV_CODEC_ID_MPEG2VIDEO;

    if (mpeg12 &&
        first_gop &&
        !(line.info & INFO_CLOSED_GOP) &&
        line.pictures.size() == 1 &&
        f->parser->pict_type == AV_PICTURE_TYPE_B) {
        if (log_message)
            log_message("Skipping leading B frame. It's probably unusable.", log_data);

        return true;
    }

    picture.output_picture_number = f->parser->output_picture_number;
    picture.picture_structure = f->parser->picture_structure;

    if (f->parser->key_frame) {
        if (!isDataLineNull()) {
            reorderDataLineFlags();
            lines.push_back(line);
            clearDataLine();
        }

        line.info = INFO_BIT11 | INFO_STARTS_NEW_GOP;

        // More evil shit for passing through "closed_gop". MPEG2 only.
        if (f->parser->key_frame >> 16)
            line.info |= INFO_CLOSED_GOP;

        int64_t colorspace;
        if (av_opt_get_int(f->avctx, "colorspace", 0, &colorspace) < 0 ||
            colorspace == AVCOL_SPC_UNSPECIFIED ||
            colorspace == AVCOL_SPC_RESERVED) {
            if (f->parser->width > 720 || f->parser->height > 576)
                colorspace = AVCOL_SPC_BT709;
            else
                colorspace = AVCOL_SPC_BT470BG;
        }
        line.matrix = colorspace;

        line.position = packet->pos;

        picture.flags = FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP;

        if (progress_report)
            progress_report(packet->pos, fake_file->getTotalSize(), progress_data);
    }

    if (f->parser->pict_type == AV_PICTURE_TYPE_I) {
        picture.flags |= FLAGS_I_PICTURE;

        if (mpeg12)
            picture.flags |= FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP;
    } else if (f->parser->pict_type == AV_PICTURE_TYPE_P) {
        picture.flags |= FLAGS_P_PICTURE;

        if (mpeg12)
            picture.flags |= FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP;
    } else if (f->parser->pict_type == AV_PICTURE_TYPE_B) {
        picture.flags |= FLAGS_B_PICTURE;

        if (mpeg12) {
            if (line.info & INFO_CLOSED_GOP) {
                picture.flags |= FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP;
            } else {
                int reference_pictures = 0;

                for (auto it = line.pictures.cbegin(); it != line.pictures.cend(); it++) {
                    uint8_t frame_type = it->flags & FLAGS_B_PICTURE;

                    if (frame_type == FLAGS_I_PICTURE || frame_type == FLAGS_P_PICTURE)
                        reference_pictures++;
                }

                if (reference_pictures >= 2)
                    picture.flags |= FLAGS_DECODABLE_WITHOUT_PREVIOUS_GOP;
            }
        }
    } else {
        if (log_message)
            log_message(std::string("Encountered unknown picture type ") + av_get_picture_type_char((AVPictureType)f->parser->pict_type) + " (" + std::to_string(f->parser->pict_type) + ").", log_data);
    }


    if (mpeg12) {
        // Frame double or tripling can only happen in sequences marked progressive.
        if (f->parser->repeat_pict == 3 || f->parser->repeat_pict == 5)
            line.info |= INFO_PROGRESSIVE_SEQUENCE;

        // Some evil shit done for the sake of passing through both "progressive_frame" and "top_field_first".
        bool progressive_frame = (f->parser->field_order >> 16) == AV_FIELD_PROGRESSIVE;
        f->parser->field_order = (AVFieldOrder)(f->parser->field_order & 0xff);

        if (progressive_frame)
            picture.flags |= FLAGS_PROGRESSIVE;
    }

    if (f->parser->repeat_pict > 1)
        picture.flags |= FLAGS_RFF;

    if (f->parser->picture_structure == AV_PICTURE_STRUCTURE_FRAME &&
        (f->parser->field_order == AV_FIELD_TT || f->parser->repeat_pict == 5))
        picture.flags |= FLAGS_TFF;

    if (f->parser->picture_structure == AV_PICTURE_STRUCTURE_FRAME &&
        f->parser->field_order == AV_FIELD_PROGRESSIVE)
        picture.flags |= FLAGS_PROGRESSIVE;


    if (codec_id == AV_CODEC_ID_H264) {
        // Handle interlaced crap by pretending we have frames in the stream, not fields.
        Picture &previous_picture = line.pictures.back();
        if (line.pictures.size() &&
            f->parser->picture_structure != AV_PICTURE_STRUCTURE_FRAME &&
            previous_picture.picture_structure != AV_PICTURE_STRUCTURE_FRAME &&
            previous_picture.output_picture_number == f->parser->output_picture_number - 1) {

            if (f->parser->picture_structure == AV_PICTURE_STRUCTURE_TOP_FIELD)
                previous_picture.flags &= ~FLAGS_TFF;
            else
                previous_picture.flags |= FLAGS_TFF;

            // No RFF when the coded pictures are fields.
            previous_picture.flags &= ~FLAGS_RFF;

            previous_picture.picture_structure = AV_PICTURE_STRUCTURE_FRAME;

            // Skip adding this picture to the list.
            return true;
        }
    }

    // Try to guess the frame rate from the pts. We use it if ffmpeg reports a nonsense frame rate.
    if (guessed_frame_rate.num == 0 || guessed_frame_rate.den == 0) {
        if (previous_pts == AV_NOPTS_VALUE) {
            previous_pts = f->parser->pts;
        } else {
            AVRational duration = { (int)(f->parser->pts - previous_pts), 1 };

            if (duration.num > 0) {
                AVRational timebase = f->fctx->streams[packet->stream_index]->time_base;

                AVRational frame_rate = av_inv_q(av_mul_q(duration, timebase));

                if (frame_rate.num > 0 && frame_rate.den > 0 && av_q2d(frame_rate) < 130) {
                    av_reduce(&guessed_frame_rate.num, &guessed_frame_rate.den,
                              frame_rate.num, frame_rate.den,
                              INT64_MAX);
                }
            }
        }
    }

    line.pictures.push_back(picture);

    return true;
}


bool D2V::handleAudioPacket(AVPacket *packet) {
    if (codecIDRequiresWave64(f->fctx->streams[packet->stream_index]->codec->codec_id)) {
        AVFormatContext *w64_ctx = (AVFormatContext *)audio_files.at(packet->stream_index);

        AVPacket pkt_in = *packet;

        AVFrame *frame = av_frame_alloc();

        while (pkt_in.size) {
            int got_frame = 0;

            AVCodecContext *codec = f->audio_ctx.at(pkt_in.stream_index);

            // We ignore got_frame because pcm_bluray and pcm_dvd decoders don't have any delay.
            int ret = avcodec_decode_audio4(codec, frame, &got_frame, &pkt_in);
            if (ret < 0) {
                char id[20] = { 0 };
                snprintf(id, 19, "%x", f->fctx->streams[pkt_in.stream_index]->id);
                error = "Failed to decode audio packet from stream id ";
                error += id;
                error += ".";

                return false;
            }

            pkt_in.data += ret;
            pkt_in.size -= ret;

            AVPacket pkt_out;
            av_init_packet(&pkt_out);
            pkt_out.data = frame->data[0];
            pkt_out.size = frame->nb_samples * frame->channels * av_get_bytes_per_sample((AVSampleFormat)frame->format);
            pkt_out.stream_index = 0;
            pkt_out.pts = 0;
            pkt_out.dts = 0;

            av_write_frame(w64_ctx, &pkt_out);
        };

        av_frame_free(&frame);
    } else { // Not PCM, just dump it.
        FILE *file = (FILE *)audio_files.at(packet->stream_index);

        if (fwrite(packet->data, 1, packet->size, file) < (size_t)packet->size) {
            char id[20] = { 0 };
            snprintf(id, 19, "%x", f->fctx->streams[packet->stream_index]->id);
            error = "Failed to write audio packet from stream id ";
            error += id;
            error += ": fwrite() failed.";

            return false;
        }
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


D2V::D2V() {

}


D2V::D2V(const std::string &_d2v_file_name, FILE *_d2v_file, const AudioFilesMap &_audio_files, FakeFile *_fake_file, FFMPEG *_f, AVStream *_video_stream, D2V::ColourRange _input_range, ProgressFunction _progress_report, void *_progress_data, LoggingFunction _log_message, void *_log_data)
    : d2v_file_name(_d2v_file_name)
    , d2v_file(_d2v_file)
    , audio_files(_audio_files)
    , fake_file(_fake_file)
    , f(_f)
    , video_stream(_video_stream)
    , input_range(_input_range)
    , progress_report(_progress_report)
    , progress_data(_progress_data)
    , log_message(_log_message)
    , log_data(_log_data)
    , previous_pts(AV_NOPTS_VALUE)
    , guessed_frame_rate({ 0, 0 })
{ }


const std::string &D2V::getD2VFileName() const {
    return d2v_file_name;
}


const D2V::Stats &D2V::getStats() const {
    return stats;
}


const std::string &D2V::getError() const {
    return error;
}


std::atomic_bool stop_processing(false);


void D2V::index() {
    AVPacket packet;
    av_init_packet(&packet);

    while (av_read_frame(f->fctx, &packet) == 0) {
        if (stop_processing) {
            stop_processing = false;
            result = ProcessingCancelled;
            fclose(d2v_file);
            closeAudioFiles(audio_files, f->fctx);
            return;
        }

        // Apparently we might receive packets from streams with AVDISCARD_ALL set,
        // and also from streams discovered late, probably.
        if (packet.stream_index != video_stream->index &&
            !audio_files.count(packet.stream_index)) {
            av_free_packet(&packet);
            continue;
        }

        bool okay = true;

        if (packet.stream_index == video_stream->index)
            okay = handleVideoPacket(&packet);
        else
            okay = handleAudioPacket(&packet);

        if (!okay) {
            av_free_packet(&packet);
            result = ProcessingError;
            fclose(d2v_file);
            closeAudioFiles(audio_files, f->fctx);
            return;
        }

        av_free_packet(&packet);
    }


    // If the last picture in the stream is an orphan field, discard it. lavc would not like it.
    if (line.pictures.size() &&
        line.pictures.back().picture_structure != AV_PICTURE_STRUCTURE_FRAME)
        line.pictures.pop_back();


    // Handle the very last GOP, I guess.
    if (!isDataLineNull()) {
        reorderDataLineFlags();
        lines.push_back(line);
        clearDataLine();
    }


    // Collect stats.
    for (size_t i = 0; i < lines.size(); i++) {
        stats.video_frames += lines[i].pictures.size();

        for (size_t j = 0; j < lines[i].pictures.size(); j++) {
            const Picture &picture = lines[i].pictures[j];

            if (picture.flags & FLAGS_PROGRESSIVE)
                stats.progressive_frames++;
            if (picture.flags & FLAGS_TFF)
                stats.tff_frames++;
            if (picture.flags & FLAGS_RFF)
                stats.rff_frames++;
        }
    }


    if (!lines.size()) {
        result = ProcessingFinished;
        fclose(d2v_file);
        closeAudioFiles(audio_files, f->fctx);
        return;
    }


    // Here we must make sure d2vsource can actually obtain every keyframe.
    // If it can't, we try to find a better location towards the previous keyframe.
    // If somehow that fails, we move the offending line's frames to the previous line.
    // At least h264 in mpegts requires this.
    FakeFile::seek(fake_file, 0, SEEK_SET);
    FFMPEG f2;
    if (!f2.initFormat(*fake_file)) {
        result = ProcessingError;
        error = "Error while testing keyframe locations: " + f2.getError();
        fclose(d2v_file);
        closeAudioFiles(audio_files, f->fctx);
        return;
    }

    av_init_packet(&packet);

    for (size_t i = 0; i < lines.size(); ) {
        // Report progress because this takes a while. Especially with slow hard drives, probably.
        if (progress_report)
            progress_report((int64_t)i, (int64_t)lines.size(), progress_data);

        // Same reason.
        if (stop_processing) {
            stop_processing = false;
            result = ProcessingCancelled;
            fclose(d2v_file);
            closeAudioFiles(audio_files, f->fctx);
            return;
        }

        int64_t target = lines[i].position;

        fake_file->setOffsetFromRealStart(target);
        FakeFile::seek(fake_file, 0, SEEK_SET);
        avformat_seek_file(f2.fctx, video_stream->index, INT64_MIN, 0, INT64_MAX, AVSEEK_FLAG_BYTE);


        do {
            av_free_packet(&packet);
            av_read_frame(f2.fctx, &packet);
        } while (f2.fctx->streams[packet.stream_index]->id != video_stream->id);

        int64_t position = packet.pos;

        av_free_packet(&packet);

        bool invalid_seek_point = position != 0;

        if (invalid_seek_point) {
            int64_t previous_target = i ? lines[i - 1].position : -1;

            // Binary search, yay.
            int64_t minimum = previous_target;
            int64_t maximum = target;

            while (maximum - minimum > 1) {
                int64_t middle = minimum + (maximum - minimum) / 2;

                fake_file->setOffsetFromRealStart(middle);
                FakeFile::seek(fake_file, 0, SEEK_SET);
                avformat_seek_file(f2.fctx, video_stream->index, INT64_MIN, 0, INT64_MAX, AVSEEK_FLAG_BYTE);


                do {
                    av_free_packet(&packet);
                    av_read_frame(f2.fctx, &packet);
                } while (f2.fctx->streams[packet.stream_index]->id != video_stream->id);

                position = packet.pos;

                av_free_packet(&packet);

                if (position == target - middle) { // middle is good
                    if (log_message)
                        log_message("Moving keyframe location " + std::to_string(fake_file->getPositionInRealFile(target)) + " to " + std::to_string(fake_file->getPositionInRealFile(middle)) + " (" + std::to_string(position) + " bytes).", log_data);

                    lines[i].position = middle;

                    break;
                } else if (position > target - middle) { // middle resulted in packet too far to the right
                    maximum = middle;
                } else { // middle resulted in packet too far to the left
                    minimum = middle;
                }
            }
        }

        bool still_invalid_seek_point = invalid_seek_point && lines[i].position == target;

        if (still_invalid_seek_point && i == 0) {
            std::string message = "Location of first keyframe is unreachable. This should have been impossible.";

            if (target != 0) {
                message += " Moving it from " + std::to_string(target) + " to 0 even though it's probably pointless.";

                lines[i].position = 0;
            }

            if (log_message)
                log_message(message, log_data);
        }

        if (still_invalid_seek_point && i) {
            if (log_message)
                log_message("Fixing unreachable keyframe location " + std::to_string(fake_file->getPositionInRealFile(target)) + ".", log_data);

            for (size_t j = 0; j < lines[i].pictures.size(); j++)
                lines[i - 1].pictures.push_back(lines[i].pictures[j]);

            lines.erase(lines.cbegin() + i);
        } else {
            i++;
        }
    }
    f2.cleanup();
    fake_file->setOffsetFromRealStart(0);
    FakeFile::seek(fake_file, 0, SEEK_SET);


    // Convert positions in the fake file into positions in the real files.
    if (fake_file->size() > 1) {
        for (size_t i = 0; i < lines.size(); i++) {
            int64_t position = lines[i].position;

            lines[i].file = fake_file->getFileIndex(position);
            lines[i].position = fake_file->getPositionInRealFile(position);
        }
    }


    if (!printHeader()) {
        result = ProcessingError;
        fclose(d2v_file);
        closeAudioFiles(audio_files, f->fctx);
        return;
    }

    if (!printSettings()) {
        result = ProcessingError;
        fclose(d2v_file);
        closeAudioFiles(audio_files, f->fctx);
        return;
    }

    for (size_t i = 0; i < lines.size(); i++) {
        if (stop_processing) {
            stop_processing = false;
            result = ProcessingCancelled;
            fclose(d2v_file);
            closeAudioFiles(audio_files, f->fctx);
            return;
        }

        if (!printDataLine(lines[i])) {
            result = ProcessingError;
            fclose(d2v_file);
            closeAudioFiles(audio_files, f->fctx);
            return;
        }
    }

    if (!printStreamEnd()) {
        result = ProcessingError;
        fclose(d2v_file);
        closeAudioFiles(audio_files, f->fctx);
        return;
    }

    result = ProcessingFinished;
    fclose(d2v_file);
    closeAudioFiles(audio_files, f->fctx);

    if (log_message) {
        std::string message;
        message += "Video frames seen:   " + std::to_string(stats.video_frames) + "\n";
        message += "    Progressive:     " + std::to_string(stats.progressive_frames) + "\n";
        message += "    Top field first: " + std::to_string(stats.tff_frames) + "\n";
        message += "    Repeat:          " + std::to_string(stats.rff_frames);

        log_message(message, log_data);
    }
}


void D2V::demuxVideo(FILE *video_file, int64_t start_gop_position, int64_t end_gop_position) {
    result = ProcessingFinished;

    f->deselectAllStreams();

    video_stream->discard = AVDISCARD_DEFAULT;

    f->seek(start_gop_position);

    AVPacket packet;
    av_init_packet(&packet);

    while (av_read_frame(f->fctx, &packet) == 0) {
        if (stop_processing) {
            stop_processing = false;
            result = ProcessingCancelled;
            fclose(video_file);
            return;
        }

        // Apparently we might receive packets from streams with AVDISCARD_ALL set,
        // and also from streams discovered late, probably.
        if (packet.stream_index != video_stream->index ||
            packet.pos < start_gop_position) {
            av_free_packet(&packet);
            continue;
        } else if (packet.pos >= end_gop_position) {
            av_free_packet(&packet);
            break;
        }

        if (progress_report)
            progress_report(packet.pos - start_gop_position, end_gop_position - start_gop_position, progress_data);

        if (fwrite(packet.data, 1, packet.size, video_file) < (size_t)packet.size) {
            char id[20] = { 0 };
            snprintf(id, 19, "%x", video_stream->id);
            error = "Failed to write video packet from stream id ";
            error += id;
            error += ": fwrite() failed.";

            result = ProcessingError;

            av_free_packet(&packet);

            fclose(video_file);

            return;
        }

        av_free_packet(&packet);
    }

    fclose(video_file);
}


D2V::ProcessingResult D2V::getResult() {
    return result;
}


// Terribly inefficient but speed is not really important here.
int D2V::getGOPStartFrame(int frame) {
    int total = 0;

    for (size_t i = 0; i < lines.size(); i++) {
        total += lines[i].pictures.size();

        if (frame < total)
            return total - lines[i].pictures.size();
    }

    return -1;
}


int D2V::getNextGOPStartFrame(int frame) {
    int total = 0;

    for (size_t i = 0; i < lines.size(); i++) {
        total += lines[i].pictures.size();

        if (frame < total)
            return total;
    }

    return -1;
}


int64_t D2V::getGOPStartPosition(int frame) {
    int total = 0;

    for (size_t i = 0; i < lines.size(); i++) {
        total += lines[i].pictures.size();

        if (frame < total)
            return lines[i].position;
    }

    return -1;
}


int64_t D2V::getNextGOPStartPosition(int frame) {
    int total = 0;

    for (size_t i = 0; i < lines.size(); i++) {
        total += lines[i].pictures.size();

        if (frame < total) {
            if (i < lines.size() - 1)
                return lines[i + 1].position;
            else
                return INT64_MAX;
        }
    }

    return -1;
}


bool D2V::isOpenGOP(int frame) {
    int total = 0;

    for (size_t i = 0; i < lines.size(); i++) {
        total += lines[i].pictures.size();

        if (frame < total)
            return !(lines[i].info & INFO_CLOSED_GOP);
    }

    return false;
}


int D2V::getNumFrames() {
    int total = 0;

    for (size_t i = 0; i < lines.size(); i++)
        total += lines[i].pictures.size();

    return total;
}


int D2V::getStreamType(const char *name) {
    std::unordered_map<std::string, int> stream_types_map = {
        { "mpegvideo",  ELEMENTARY_STREAM },
        { "h264",       ELEMENTARY_STREAM },
        { "mpeg",       PROGRAM_STREAM },
        { "mpegts",     TRANSPORT_STREAM },
        { "pva",        PVA_STREAM }
    };

    int stream_type;

    try {
        stream_type = stream_types_map.at(name);
    } catch (std::out_of_range &) {
        stream_type = UNSUPPORTED_STREAM;
    }

    return stream_type;
}


bool D2V::isSupportedVideoCodecID(AVCodecID id) {
    std::unordered_set<int> supported_codec_ids = {
        AV_CODEC_ID_MPEG1VIDEO,
        AV_CODEC_ID_MPEG2VIDEO,
        AV_CODEC_ID_H264
    };

    return supported_codec_ids.count(id);
}
