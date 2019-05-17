#pragma once

#include <cstdint>
#include "cocos2d.h"
#include "VideoHeader.h"
#include "VideoStream.h"

namespace xVideo
{
	class VideoDecoder : public cocos2d::Ref
	{
	public:
		static VideoDecoder* create(const char* path);

		bool open(const char* path);
		bool open(VideoStream* stream, double loopA = 0, double loopB = -1);

		void setup();
		void setup(cocos2d::Size target_size);

		bool isOpened() const;
		void close();
		uint32_t read(uint8_t** vbuf);
		/**
		 * can only seek key frame, not recommand to set a non-0 value. 
		 * will make next [read] give specified frame.
		 */
		bool seek(uint32_t frameOffset);
		// only used for frame control when playing
		bool seekTime(double sec);
		uint32_t tell() const;
		uint32_t getTotalFrames() const;

		// raw size of the video
		cocos2d::Size getRawSize() const { return cocos2d::Size(raw_width, raw_height); }
		// size after convert
		cocos2d::Size getTargetSize() const { return targetSize; }
		const VideoInfo& getVideoInfo() const { return videoInfo; }
		const std::string& getReadableInfo() const { return readableInfo; }
	protected:
		void setVideoInfo(bool print);

		VideoDecoder();
		~VideoDecoder();
		bool _isOpened = false;
		VideoStream* stream = nullptr;

		uint32_t raw_width = 0;
		uint32_t raw_height = 0;
		cocos2d::Size targetSize;

		AVRational timeBaseV;
		AVRational timeBaseA;

		AVFormatContext *pFormatCtx = nullptr;
		AVCodecContext  *pCodecCtx = nullptr;
		AVCodec			*pCodecV = nullptr;
		int idxVideo = -1;
		int idxAudio = -1;

		SwsContext *img_convert_ctx = nullptr;

		AVPicture *picture = nullptr;
		AVFrame   *pFrame = nullptr;
		AVPacket  *packet = nullptr;
		int64_t lastFrame = -2;
		int64_t currentFrame = -1;

		std::string filePath;
		double frame_dt = 0.0;
		int64_t totalFrames = 0;
		double durationV = 0;

		VideoInfo videoInfo;
		std::string readableInfo;
	};
}
