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


#ifndef D2V_WITCH_FAKEFILE_H
#define D2V_WITCH_FAKEFILE_H


#include <string>
#include <vector>


struct RealFile {
    std::string name;
    FILE *stream;
    off_t size;

    RealFile(const std::string &_name)
        : name(_name)
        , stream(nullptr)
        , size(0)
    { }
};


class FakeFile : public std::vector<RealFile> {
    int64_t total_size;
    int64_t current_position;
    int64_t offset_from_real_start; // Only used (non-zero) when verifying the keyframe locations.
    const_iterator current_file;
    std::string error;


public:
    ~FakeFile();

    bool open();

    void close();

    int64_t getTotalSize() const;

    int64_t getCurrentPosition() const;

    const std::string &getError() const;

    int getFileIndex(int64_t position_in_fake_file) const;

    int64_t getPositionInRealFile(int64_t position_in_fake_file) const;

    int64_t getPositionInFakeFile(int64_t position_in_real_file, int file_index) const;

    void setOffsetFromRealStart(int64_t offset);

    static int64_t seek(void *opaque, int64_t offset, int whence);

    static int readPacket(void *opaque, uint8_t *buf, int bytes_to_read);
};


#endif // D2V_WITCH_FAKEFILE_H
