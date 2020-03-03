#pragma once
#include "platform/CCPlatformConfig.h"
#include <functional>
#include <string>

namespace video
{
	void setLoggingFunction(const std::function<void(const std::string&)>& callback);
	void logging(const char* format, ...);
	std::string getErrorString(int errorCode);
}

#define VINFO(_str, ...) video::logging("[VID] %s: " _str, __func__, ##__VA_ARGS__)
#define VWARN(_str,...) video::logging("[VID] %s: " _str, __func__, ##__VA_ARGS__)
#define VERRO(_str, ...) video::logging("[VID] %s: " _str, __func__, ##__VA_ARGS__)

#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
#define av_sprintf sprintf_s
#else
#define av_sprintf snprintf
#endif

extern "C"
{
	#include "../../external/ffmpeg/include/libavformat/avformat.h"
	#include "../../external/ffmpeg/include/libavformat/avio.h"
	#include "../../external/ffmpeg/include/libavcodec/avcodec.h"
	#include "../../external/ffmpeg/include/libswscale/swscale.h"
	#include "../../external/ffmpeg/include/libswresample/swresample.h"

	#include "../../external/ffmpeg/include/libavutil/frame.h"
	#include "../../external/ffmpeg/include/libavutil/mem.h"
	#include "../../external/ffmpeg/include/libavutil/file.h"
	#include "../../external/ffmpeg/include/libavutil/common.h"
	#include "../../external/ffmpeg/include/libavutil/rational.h"
	#include "../../external/ffmpeg/include/libavutil/avutil.h"
	#include "../../external/ffmpeg/include/libavutil/imgutils.h"
}

#include "VideoStruct.h"
