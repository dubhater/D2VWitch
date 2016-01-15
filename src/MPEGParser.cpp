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

#include "MPEGParser.h"


MPEGParser::MPEGParser()
    : width(-1)
    , height(-1)
    , progressive_sequence(false)
{
    clear();
}


void MPEGParser::clear() {
    picture_coding_type = 0;
    top_field_first = false;
    repeat_first_field = false;
    progressive_frame = false;
    group_of_pictures_header = false;
    closed_gop = false;
    matrix_coefficients = MATRIX_UNSPECIFIED;
}


const uint8_t *MPEGParser::findStartCode(const uint8_t *data, const uint8_t *data_end, uint32_t *start_code) {
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


void MPEGParser::parseData(const uint8_t *data, int data_size) {
    clear();

    const uint8_t *data_end = data + data_size;

    while (data < data_end) {
        uint32_t start_code = 0xffffffff;

        data = findStartCode(data, data_end, &start_code);

        int bytes_left = data_end - data;

        if (start_code == PICTURE_START_CODE) {
            if (bytes_left >= 2)
                picture_coding_type = (data[1] >> 3) & 7;
        } else if (start_code == SEQUENCE_HEADER_CODE) {
            if (bytes_left >= 3) {
                width = (((int)data[0]) << 4) | (data[1] >> 4);
                height = ((data[1] & 0xf) << 8) | data[2];
            }
        } else if (start_code == EXTENSION_START_CODE) {
            if (bytes_left >= 1) {
                int extension_type = data[0] >> 4;

                if (extension_type == SEQUENCE_EXTENSION) {
                    if (bytes_left >= 3) {
                        if (width > 0 && height > 0) {
                            int horizontal_size_extension = ((data[1] & 1) << 1) | (data[2] >> 7);
                            int vertical_size_extension = (data[2] >> 5) & 3;
                            width |= horizontal_size_extension << 12;
                            height |= vertical_size_extension << 12;
                        }
                        progressive_sequence = data[1] & (1 << 3);
                    }
                } else if (extension_type == SEQUENCE_DISPLAY_EXTENSION) {
                    if (bytes_left >= 1) {
                        bool colour_description = data[0] & 1;
                        if (colour_description && bytes_left >= 4) {
                            // colour_primaries = data[1];
                            // transfer_characteristics = data[2];
                            matrix_coefficients = data[3];
                        }
                    }
                } else if (extension_type == PICTURE_CODING_EXTENSION) {
                    if (bytes_left >= 5) {
                        top_field_first = data[3] & (1 << 7);
                        repeat_first_field = data[3] & (1 << 1);
                        progressive_frame = data[4] & (1 << 7);
                    }
                }
            }
        } else if (start_code == GROUP_START_CODE) {
            if (bytes_left >= 4) {
                group_of_pictures_header = true;
                closed_gop = data[3] & (1 << 6);
            }
        } else if (start_code == 0xffffffff ||
                   (start_code >= SLICE_START_CODE_MIN && start_code <= SLICE_START_CODE_MAX)) {
            break;
        }
    }
}
