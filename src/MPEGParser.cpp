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



This file is inspired by libavcodec/mpegvideo_parser.c. It may be
considered to have the same license as that file, if anyone cares about
such things.

*/


#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include "MPEGParser.h"


enum StartCodes {
    PICTURE_START_CODE = 0x00,
    SEQUENCE_HEADER_CODE = 0xb3,
    EXTENSION_START_CODE = 0xb5,
    GROUP_START_CODE = 0xb8,
    SLICE_START_CODE_MIN = 0x01,
    SLICE_START_CODE_MAX = 0xaf
};

enum ExtensionStartCodeIdentifier {
    SEQUENCE_EXTENSION = 0x1,
    SEQUENCE_DISPLAY_EXTENSION = 0x2,
    PICTURE_CODING_EXTENSION = 0x8
};


static void clear(AVCodecParserContext *parser, AVCodecContext *avctx) {
    parser->pict_type = AV_PICTURE_TYPE_NONE;
    parser->field_order = AV_FIELD_UNKNOWN;
    parser->repeat_pict = 0;
    parser->picture_structure = AV_PICTURE_STRUCTURE_UNKNOWN;
    parser->key_frame = 0;
    av_opt_set_int(avctx, "colorspace", AVCOL_SPC_UNSPECIFIED, 0);
}


static const uint8_t *findStartCode(const uint8_t *data, const uint8_t *data_end, uint32_t *start_code) {
    while (data <= data_end - 4) {
        uint32_t bits;
        memcpy(&bits, data, 4);

        if ((bits << 8) == (1 << 24)) {
            *start_code = bits >> 24;
            return data + 4;
        }

        data++;
    }

    return data_end;
}


void d2vWitchParseMPEG12Data(AVCodecParserContext *parser, AVCodecContext *avctx, const uint8_t *data, int data_size) {
    clear(parser, avctx);

    parser->picture_structure = AV_PICTURE_STRUCTURE_FRAME;

    // Maybe not the best idea, but it's really the only thing that needs to be remembered between calls
    // and not stored in parser or avctx.
    static int progressive_sequence = 0;

    int sequence_header = 0;
    int group_of_pictures_header = 0;
    int closed_gop = 0;


    const uint8_t *data_end = data + data_size;

    while (data < data_end) {
        uint32_t start_code = 0xffffffff;

        data = findStartCode(data, data_end, &start_code);

        int bytes_left = data_end - data;

        if (start_code == PICTURE_START_CODE) {
            if (bytes_left >= 2) {
                parser->pict_type = (data[1] >> 3) & 7;

                parser->key_frame = (parser->pict_type == AV_PICTURE_TYPE_I && sequence_header) || group_of_pictures_header;

                if (closed_gop)
                    parser->key_frame |= 1 << 16;
            }
        } else if (start_code == SEQUENCE_HEADER_CODE) {
            if (bytes_left >= 3) {
                sequence_header = 1;

                parser->width = (((int)data[0]) << 4) | (data[1] >> 4);
                parser->height = ((data[1] & 0xf) << 8) | data[2];
            }
        } else if (start_code == EXTENSION_START_CODE) {
            if (bytes_left >= 1) {
                int extension_type = data[0] >> 4;

                if (extension_type == SEQUENCE_EXTENSION) {
                    if (bytes_left >= 3) {
                        if (parser->width > 0 && parser->height > 0) {
                            int horizontal_size_extension = ((data[1] & 1) << 1) | (data[2] >> 7);
                            int vertical_size_extension = (data[2] >> 5) & 3;
                            parser->width |= horizontal_size_extension << 12;
                            parser->height |= vertical_size_extension << 12;
                        }
                        progressive_sequence = data[1] & (1 << 3);
                    }
                } else if (extension_type == SEQUENCE_DISPLAY_EXTENSION) {
                    if (bytes_left >= 1) {
                        bool colour_description = data[0] & 1;
                        if (colour_description && bytes_left >= 4) {
                            // colour_primaries = data[1];
                            // transfer_characteristics = data[2];
                            av_opt_set_int(avctx, "colorspace", data[3], 0);
                        }
                    }
                } else if (extension_type == PICTURE_CODING_EXTENSION) {
                    if (bytes_left >= 5) {
                        int top_field_first = data[3] & (1 << 7);
                        int repeat_first_field = data[3] & (1 << 1);
                        int progressive_frame = data[4] & (1 << 7);

                        parser->repeat_pict = 1;

                        if (repeat_first_field) {
                            if (progressive_sequence) {
                                if (top_field_first)
                                    parser->repeat_pict = 5;
                                else
                                    parser->repeat_pict = 3;
                            } else if (progressive_frame){
                                parser->repeat_pict = 2;
                            }
                        }

                        if (progressive_sequence) {
                            parser->field_order = AV_FIELD_PROGRESSIVE;
                        } else {
                            if (top_field_first)
                                parser->field_order = AV_FIELD_TT;
                            else
                                parser->field_order = AV_FIELD_BB;

                            if (progressive_frame)
                                parser->field_order = (AVFieldOrder)(parser->field_order | (AV_FIELD_PROGRESSIVE << 16));
                        }
                    }
                }
            }
        } else if (start_code == GROUP_START_CODE) {
            if (bytes_left >= 4) {
                group_of_pictures_header = 1;
                closed_gop = data[3] & (1 << 6);
            }
        } else if (start_code == 0xffffffff ||
                   (start_code >= SLICE_START_CODE_MIN && start_code <= SLICE_START_CODE_MAX)) {
            break;
        }
    }
}
