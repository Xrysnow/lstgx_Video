#pragma once

namespace video
{
	struct VideoInfo
	{

		// AVFormatContext

		std::string filename; // 文件名
		uint64_t duration;	  // 持续时间 微秒
		double bitRate;		  // 平均混合码率 b/s
		uint32_t nb_streams;  // 视音频个数

		// AVInputFormat

		std::string iformatName;        // 封装格式名称
		std::string iformatLongName;	// 封装格式长名称
		std::string iformatExtensions;	// 封装格式扩展名
		int iformatID;					// 封装格式ID

		// AVStream

		uint32_t nFramesV;	   //
		uint32_t nFramesA;	   //
		uint64_t durationV;      // 视频流长度 微秒
		uint64_t durationA;      // 音频流长度 微秒
		double frameRateV;     // 视频fps
		double frameRateA;     // 音频fps
		double timeBaseV;      //
		double timeBaseA;      //

		// AVCodecParameters

		uint32_t width;	      // 视频宽度
		uint32_t height;      // 视频高度
		uint32_t bitRateV;    // 视频码率 b/s

		uint32_t bitRateA;    // 音频码率 b/s
		uint32_t sampleRateA; // 音频采样率
		uint32_t channelsA;   // 音频通道数

		// AVCodec

		std::string codecNameV;      // 视频编码格式名称
		std::string codecLongNameV;	 // 视频编码格式长名称

	};
}
