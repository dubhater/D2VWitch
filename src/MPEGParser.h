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


#ifndef D2V_WITCH_MPEGPARSER_H
#define D2V_WITCH_MPEGPARSER_H


#include <cstdint>


struct MPEGParser {
    enum PictureCodingType {
        I_PICTURE = 1,
        P_PICTURE = 2,
        B_PICTURE = 3
    };

    enum MatrixCoefficients {
        MATRIX_FORBIDDEN = 0,
        MATRIX_BT709 = 1,
        MATRIX_UNSPECIFIED = 2,
        MATRIX_RESERVED = 3,
        MATRIX_FCC = 4,
        MATRIX_BT470BG = 5,
        MATRIX_SMPTE170M = 6,
        MATRIX_SMPTE240M = 7
    };


    int width;
    int height;
    int picture_coding_type;
    bool progressive_sequence;
    bool top_field_first;
    bool repeat_first_field;
    bool progressive_frame;
    bool group_of_pictures_header;
    bool closed_gop;
    uint8_t matrix_coefficients;


    MPEGParser();

    void parseData(const uint8_t *data, int data_size);


private:

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


    void clear();

    const uint8_t *findStartCode(const uint8_t *data, const uint8_t *data_end, uint32_t *start_code);
};

#endif // D2V_WITCH_MPEGPARSER_H
