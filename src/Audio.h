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


#ifndef D2V_WITCH_AUDIO_H
#define D2V_WITCH_AUDIO_H

#include <unordered_map>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "FakeFile.h"


// Key: audio stream index. Value: AVFormatContext* if the stream is LPCM, otherwise FILE*.
typedef std::unordered_map<int, void *> AudioFilesMap;

// Key: audio stream id. Value: delay in milliseconds.
typedef std::unordered_map<int, int64_t> AudioDelayMap;


AVFormatContext *openWave64(const std::string &path, const AVCodecParameters *in_par, std::string &error);

void closeAudioFiles(AudioFilesMap &audio_files, const AVFormatContext *fctx);

const char *suggestAudioFileExtension(AVCodecID codec_id);

int64_t getChannelLayout(AVCodecParameters *avctx);

bool calculateAudioDelays(FakeFile &fake_file, int video_stream_id, AudioDelayMap &audio_delay_map, int64_t *first_video_keyframe_pos, std::string &error);

#endif // D2V_WITCH_AUDIO_H
