#pragma once
#include "cocos2d.h"
#include "VideoCommon.h"
#include "VideoStream.h"
#include <cstdint>

namespace video
{
	class Decoder : public cocos2d::Ref
	{
	public:
		static Decoder* create(const std::string& path);

		bool open(const std::string& path);
		bool open(VideoStream* stream, double loopA = 0, double loopB = -1);

		bool setup();
		bool setup(const cocos2d::Size& target_size);

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
		bool send_packet(AVPacket* packet);
		bool receive_frame(AVFrame* frame);

		AVPixelFormat getHWPixelFormat(AVHWDeviceType type);
		bool createHWDevice(AVHWDeviceType type);
		bool usingHardware();
		
		void setVideoInfo(bool print);

		Decoder();
		~Decoder();
		bool _isOpened = false;
		VideoStream* stream = nullptr;

		uint32_t raw_width = 0;
		uint32_t raw_height = 0;
		cocos2d::Size targetSize;

		AVRational timeBaseV;
		AVRational timeBaseA;

		AVFormatContext* pFormatCtx = nullptr;
		AVCodecContext*  pCodecCtx = nullptr;
		AVCodec*         pCodecV = nullptr;
		int idxVideo = -1;
		int idxAudio = -1;

		AVHWDeviceType hw_device_type;
		AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;
		AVPixelFormat sw_pix_fmt = AV_PIX_FMT_NONE;
		AVBufferRef* hw_device_ctx = nullptr;

		SwsContext* img_convert_ctx = nullptr;
		uint8_t* sws_pointers[4] = { nullptr };
		int sws_linesizes[4] = { 0 };

		AVFrame* pFrame = nullptr;
		AVFrame* pFrameSW = nullptr;
		int64_t lastFrame = -2;
		int64_t currentFrame = -1;
		
		std::string filePath;
		double frame_dt = 0.0;
		int64_t totalFrames = 0;
		double durationV = 0.0;
		double durationCtx = 0.0;

		VideoInfo videoInfo;
		std::string readableInfo;
	};
}
