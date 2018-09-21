#pragma once
#include "platform/CCPlatformConfig.h"
#include "../LSTG/LogSystem.h"

#define VINFO(_str, ...) LINFO("[VID] %m: " _str, __func__, ##__VA_ARGS__)
#define VWARN(_str,...) LWARNING("[VID] %m: " _str, __func__, ##__VA_ARGS__)
#define VERRO(_str, ...) LERROR("[VID] %m: " _str, __func__, ##__VA_ARGS__)

#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
#define av_sprintf sprintf_s

#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avdevice.lib")
#pragma comment(lib,"avfilter.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"postproc.lib")
#pragma comment(lib,"swresample.lib")
#pragma comment(lib,"swscale.lib")
#pragma warning( disable : 4996 )

#else
#define av_sprintf snprintf
#endif

#ifdef PixelFormat
#undef   PixelFormat
#endif
