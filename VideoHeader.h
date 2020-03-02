#pragma once

#include "VideoMacros.h"

extern "C" {
#include "../../external/ffmpeg/include/libavformat/avformat.h"
#include "../../external/ffmpeg/include/libavcodec/avcodec.h"
#include "../../external/ffmpeg/include/libswscale/swscale.h"
#include "../../external/ffmpeg/include/libswresample/swresample.h"

#include "../../external/ffmpeg/include/libavutil/frame.h"
#include "../../external/ffmpeg/include/libavutil/mem.h"
#include "../../external/ffmpeg/include/libavcodec/avcodec.h"
#include "../../external/ffmpeg/include/libavformat/avformat.h"
#include "../../external/ffmpeg/include/libavformat/avio.h"
#include "../../external/ffmpeg/include/libavutil/file.h"
#include "../../external/ffmpeg/include/libavutil/common.h"
#include "../../external/ffmpeg/include/libavutil/rational.h"
#include "../../external/ffmpeg/include/libavutil/avutil.h"
#include "../../external/ffmpeg/include/libavutil/imgutils.h"
#include "../../external/ffmpeg/include/libswresample/swresample.h"
}

#include "VideoStruct.h"
