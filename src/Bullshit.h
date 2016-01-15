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


#ifndef D2V_WITCH_BULLSHIT_H
#define D2V_WITCH_BULLSHIT_H


#ifndef _WIN32
#include <dirent.h>
#endif

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>


#ifdef _MSC_VER
#undef fseeko
#undef ftello
#define fseeko _fseeki64
#define ftello _ftelli64

#define snprintf _snprintf
#endif


#ifdef _WIN32
#include <windows.h>

// Because wstring_convert is broken in gcc 5.3.0 (works in gcc 5.1.0).
//    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf16;

struct UTF16 {
    std::string to_bytes(const std::wstring &wstr) {
        int required_size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::vector<char> buffer;
        buffer.resize(required_size);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer.data(), required_size, NULL, NULL);
        return std::string(buffer.data());
    }

    std::wstring from_bytes(const std::string &str) {
        int required_size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        std::vector<wchar_t> wbuffer;
        wbuffer.resize(required_size);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wbuffer.data(), required_size);
        return std::wstring(wbuffer.data());
    }
};
#endif


static void makeAbsolute(std::string &path, std::string &error) {
#ifdef _WIN32
    UTF16 utf16;

    std::vector<wchar_t> buffer(_MAX_PATH);

    if (_wfullpath(buffer.data(), utf16.from_bytes(path).c_str(), _MAX_PATH)) {
        error.clear();
        path = utf16.to_bytes(buffer.data());
    } else {
        error = "_wfullpath() failed.";
    }
#else
    std::vector<char> buffer(PATH_MAX);

    if (realpath(path.c_str(), buffer.data())) {
        error.clear();
        path = buffer.data();
    } else {
        error = strerror(errno);
    }
#endif
}


static FILE *openFile(const char *path, const char *mode) {
#ifdef _WIN32
    UTF16 utf16;

    return _wfopen(utf16.from_bytes(path).c_str(), utf16.from_bytes(mode).c_str());
#else
    return fopen(path, mode);
#endif
}

#endif // D2V_WITCH_BULLSHIT_H

