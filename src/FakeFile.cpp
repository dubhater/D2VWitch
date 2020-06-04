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
#include <libavformat/avformat.h>
}

#include "FakeFile.h"

#include "Bullshit.h"


FakeFile::~FakeFile() {
    close();
}


bool FakeFile::open() {
    total_size = 0;
    current_position = 0;
    offset_from_real_start = 0;
    current_file = cbegin();

    auto it = begin();
    for ( ; it != end(); it++) {
        it->stream = openFile(it->name.c_str(), "rb");
        if (!it->stream) {
            error += "fopen() failed: ";
            error += strerror(errno);
            break;
        }

        if (fseeko(it->stream, 0, SEEK_END)) {
            error += "fseeko(stream, 0, SEEK_END) failed: ";
            error += strerror(errno);
            break;
        }

        it->size = ftello(it->stream);
        if (it->size == -1) {
            error += "ftello() failed: ";
            error += strerror(errno);
            break;
        }

        if (fseeko(it->stream, 0, SEEK_SET)) {
            error += "fseeko(stream, 0, SEEK_SET) failed: ";
            error += strerror(errno);
            break;
        }

        total_size += it->size;
    }

    if (error.size()) {
        error = "Failed to open input file '" + it->name + "': " + error;
        return false;
    }

    return true;
}


void FakeFile::close() {
    for (auto it = begin(); it != end(); it++) {
        if (it->stream) {
            fclose(it->stream);
            it->stream = nullptr;
        }
    }
}


int64_t FakeFile::getTotalSize() const {
    return total_size;
}


int64_t FakeFile::getCurrentPosition() const {
    return current_position;
}


const std::string &FakeFile::getError() const {
    return error;
}


int FakeFile::getFileIndex(int64_t position_in_fake_file) const {
    for (size_t i = 0; i < size(); i++) {
        if (position_in_fake_file < at(i).size)
            return i;
        else
            position_in_fake_file -= at(i).size;
    }

    return -1;
}


int64_t FakeFile::getPositionInRealFile(int64_t position_in_fake_file) const {
    for (size_t i = 0; i < size(); i++) {
        if (position_in_fake_file < at(i).size)
            return position_in_fake_file;
        else
            position_in_fake_file -= at(i).size;
    }

    return -1;
}


int64_t FakeFile::getPositionInFakeFile(int64_t position_in_real_file, int file_index) const {
    for (int i = 0; i < file_index; i++)
        position_in_real_file += at(i).size;

    return position_in_real_file;
}


void FakeFile::setOffsetFromRealStart(int64_t offset) {
    offset_from_real_start = offset;
}


int64_t FakeFile::seek(void *opaque, int64_t offset, int whence) {
    if (whence & AVSEEK_FORCE)
        whence &= ~AVSEEK_FORCE;

    FakeFile *ff = (FakeFile *)opaque;

    if (whence == AVSEEK_SIZE) {
        return ff->total_size - ff->offset_from_real_start;
    } else if (whence == SEEK_SET) {
        offset += ff->offset_from_real_start;
    } else if (whence == SEEK_CUR) {
        offset += ff->current_position;
    } else if (whence == SEEK_END) {
        offset += ff->total_size;
    } else {
        ff->error = "unknown 'whence' value " + std::to_string(whence);
        return -1;
    }

    int64_t offset_in_current_file = offset;

    if (offset_in_current_file >= ff->total_size) {
        ff->current_file = ff->cend();
        ff->current_file--;
        offset_in_current_file = offset - ff->total_size + ff->crbegin()->size;
    } else {
        for (auto it = ff->cbegin(); it != ff->cend(); it++) {
            if (offset_in_current_file < it->size) {
                ff->current_file = it;
                break;
            } else {
                offset_in_current_file -= it->size;
            }
        }
    }

    if (fseeko(ff->current_file->stream, offset_in_current_file, SEEK_SET)) {
        ff->error = strerror(errno);
        return -1;
    }

    ff->current_position = offset;
    return ff->current_position - ff->offset_from_real_start;
}


int FakeFile::readPacket(void *opaque, uint8_t *buf, int bytes_to_read) {
    FakeFile *ff = (FakeFile *)opaque;

    size_t bytes_read = fread(buf, 1, bytes_to_read, ff->current_file->stream);

    if (bytes_read < (size_t)bytes_to_read) {
        if (ferror(ff->current_file->stream)) {
            ff->error = "fread() failed.";
            return -1;
        }

        ff->current_file++;
        if (ff->current_file == ff->cend()) {
            ff->current_file--;
        } else {
            if (fseeko(ff->current_file->stream, 0, SEEK_SET)) {
                ff->error = strerror(errno);
                return -1;
            }

            size_t leftover = bytes_to_read - bytes_read;
            size_t bytes_read2 = fread(buf + bytes_read, 1, leftover, ff->current_file->stream);

            if (bytes_read2 < leftover && ferror(ff->current_file->stream)) {
                ff->error = "fread() failed.";
                return -1;
            }

            bytes_read += bytes_read2;
        }
    }

    ff->current_position += bytes_read;

    return (int)bytes_read;
}
