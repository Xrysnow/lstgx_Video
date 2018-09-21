#include "VideoDecoder.h"
#include "cocos2d.h"
#include "VideoMacros.h"
#include "VideoHeader.h"

using namespace std;
using namespace cocos2d;
using namespace xVideo;

static void lazyInit()
{
	static bool inited = false;
	if (!inited)
	{
		av_register_all();
		inited = true;
		auto c_temp =
			av_codec_next(nullptr);
			//av_hwaccel_next(nullptr);
		string inf;
		while (c_temp != nullptr)
		{
			if (c_temp->decode != nullptr)
			{
				inf += "[Decode]";
			}
			else
			{
				inf += "[Encode]";
			}
			switch (c_temp->type)
			{
			case AVMEDIA_TYPE_VIDEO:
				inf += "[Video] ";
				break;
			case AVMEDIA_TYPE_AUDIO:
				inf += "[Audio] ";
				break;
			default:
				inf += "[Other] ";
				break;
			}
			if(c_temp->name)
				inf += c_temp->name;
			inf += " | ";
			if (c_temp->long_name)
				inf += c_temp->long_name;
			inf += "\n";
			c_temp = c_temp->next;
		}
		//VINFO("supported codec:\n%m", inf.c_str());
		auto ch_temp = av_hwaccel_next(nullptr);
		inf = "";
		while (ch_temp != nullptr)
		{
			//if (ch_temp->name != nullptr)
			//{
			//	inf += "[Decode]";
			//}
			//else
			//{
			//	inf += "[Encode]";
			//}
			switch (ch_temp->type)
			{
			case AVMEDIA_TYPE_VIDEO:
				inf += "[Video] ";
				break;
			case AVMEDIA_TYPE_AUDIO:
				inf += "[Audio] ";
				break;
			default:
				inf += "[Other] ";
				break;
			}
			if (ch_temp->name)
				inf += ch_temp->name;
			inf += "\n";
			ch_temp = ch_temp->next;
		}
		VINFO("supported hardware codec:\n%m", inf.c_str());
	}
}

VideoDecoder* VideoDecoder::create(const char* path)
{
	auto ret = new (std::nothrow) VideoDecoder;
	if (ret&&ret->open(path))
	{
		ret->autorelease();
		return ret;
	}
	CC_SAFE_DELETE(ret);
	return nullptr;
}

bool VideoDecoder::open(const char* path)
{
	if (_isOpened)
		return false;
	filePath = FileUtils::getInstance()->fullPathForFilename(path);
	if (filePath.empty())
	{
		VERRO("no file: %m", path);
		return false;
	}
	lazyInit();
	pFormatCtx = avformat_alloc_context();

	// open_input
	if (avformat_open_input(&pFormatCtx, filePath.c_str(), nullptr, nullptr) != 0) {
		VERRO("avformat_open_input failed");
		return false;
	}

	// find_stream_info
	if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {
		VERRO("avformat_find_stream_info failed");
		return false;
	}

	// find first stream
	idxVideo = -1;
	idxAudio = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++) {
		auto type = pFormatCtx->streams[i]->codec->codec_type;
		if (idxVideo == -1 && type == AVMEDIA_TYPE_VIDEO) {
			idxVideo = i;
		}
		if (idxAudio == -1 && type == AVMEDIA_TYPE_AUDIO) {
			idxAudio = i;
		}
		if (idxVideo != -1 && idxAudio != -1)
			break;
	}

	// no video stream
	if (idxVideo == -1) {
		VERRO("no video stream");
		return false;
	}
	// Get a pointer to the codec context for the video stream
	pCodecCtx = pFormatCtx->streams[idxVideo]->codec;
	//auto codec_id = pFormatCtx->streams[idxVideo]->codecpar->codec_id;

	// Find the decoder for the video stream
	pCodecV = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodecCtx->codec_id == AV_CODEC_ID_H264)
	{
		pCodecV = avcodec_find_decoder_by_name("h264_cuvid");
		//pCodecV = avcodec_find_decoder_by_name("h264_qsv");
	}
	if (pCodecV == nullptr) {
		VERRO("can't get video decoder");
		return false;
	}
	// open codec
	if (avcodec_open2(pCodecCtx, pCodecV, nullptr)) {
		VERRO("can't open video codec [%m]", pCodecV->name);
		return false;
	}
	// get rational
	AVRational rational = pFormatCtx->streams[idxVideo]->avg_frame_rate;
	if (!rational.num || !rational.den)
		rational = pFormatCtx->streams[idxVideo]->r_frame_rate;

	// calc duration
	int64_t duration = 0;
	if (pFormatCtx->duration != AV_NOPTS_VALUE) {
		/*int hours, mins, secs, us;*/
		duration = pFormatCtx->duration;// +5000;
		auto secs = duration / AV_TIME_BASE;
		auto us = duration % AV_TIME_BASE;
		//VINFO("duration = %d.%d", secs, (100 * us) / AV_TIME_BASE);
	}
	else {
		VERRO("can't get duration");
		return false;
	}

	double fps = av_q2d(rational);
	frame_dt = 1.0 / fps;
	totalFrames = pFormatCtx->streams[idxVideo]->nb_frames;
	
	// Allocate video frame
	pFrame = av_frame_alloc();

	packet = (AVPacket*)av_malloc(sizeof(AVPacket));

	// File Information
	//av_dump_format(pFormatCtx, 0, m_filePath.c_str(), 0);

	// raw size
	raw_width = pCodecCtx->width;
	raw_height = pCodecCtx->height;
	timeBaseV = pFormatCtx->streams[idxVideo]->time_base;
	//timeBaseA = pFormatCtx->streams[idxAudio]->time_base;
	durationV = pFormatCtx->streams[idxVideo]->duration*av_q2d(timeBaseV);
	//avcodec_find_encoder_by_name("");
	_isOpened = true;
	setVideoInfo(true);
	return true;
}

bool VideoDecoder::open(fcyStream* src, double loopA, double loopB)
{
	if (_isOpened || !src)
		return false;
	return true;
}

void VideoDecoder::setup()
{
	setup(Size(raw_width, raw_height));
}

void VideoDecoder::setup(Size target_size)
{
	targetSize = target_size;
	// picture data
	picture = (AVPicture*)av_malloc(sizeof(AVPicture));
	// TODO: sws to YUV420 and convert in shader
	auto fmt = AVPixelFormat::AV_PIX_FMT_RGB24;
	avpicture_alloc(picture, fmt, targetSize.width, targetSize.height);
	// scale/convert
	img_convert_ctx = sws_getContext(
		pCodecCtx->width,
		pCodecCtx->height,
		pCodecCtx->pix_fmt,
		targetSize.width,
		targetSize.height,
		fmt,
		1,//SWS_FAST_BILINEAR
		nullptr, nullptr, nullptr);
}

bool VideoDecoder::isOpened() const
{
	return _isOpened;
}

void VideoDecoder::close()
{
	if (_isOpened)
	{
		sws_freeContext(img_convert_ctx);
		img_convert_ctx = nullptr;

		// Free RGB picture
		if (picture)
		{
			avpicture_free(picture);
			av_freep(&picture);
		}

		if(pFrame)
			av_freep(&pFrame);

		if(packet)
			av_freep(&packet);

		// Close the codec
		if (pCodecCtx)
			avcodec_close(pCodecCtx);

		if (pFormatCtx)
			avformat_close_input(&pFormatCtx);

		if (stream)
		{
			VINFO("stream ref = %d", stream->getReferenceCount());
			stream->SetPosition(BEG, 0);
			stream->release();
			stream = nullptr;
		}
		_isOpened = false;
	}
}

uint32_t VideoDecoder::read(uint8_t** vbuf)
{
	*vbuf = nullptr;
	if (currentFrame == (totalFrames - 1) || !_isOpened)
		return 0;
	if (lastFrame == currentFrame)
	{
		*vbuf = picture->data[idxVideo];
		//VINFO("return last");
		return 0;
	}
	int frameFinished = 0;
	while (!frameFinished && av_read_frame(pFormatCtx, packet) >= 0) {
		// Is this a packet from the video stream?
		if (packet->stream_index == idxVideo) {
			// Decode video frame
			auto ret = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, packet);
			if (ret < 0) {
				char str[64] = { 0 };
				av_strerror(ret, str, 64);
				VERRO("decode error: %m", str);
				av_free_packet(packet);
				//break;
				return 0;
			}
		}
		// Free the packet that was allocated by av_read_frame
		av_free_packet(packet);
	}
	// get frames remained in codec
	if (!frameFinished)
	{
		auto ret = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, packet);
		if (ret < 0 || !frameFinished)
		{
			return 0;
		}
		av_free_packet(packet);
	}
	sws_scale(img_convert_ctx,
		pFrame->data, pFrame->linesize,
		0, pCodecCtx->height,
		picture->data, picture->linesize);
	lastFrame = currentFrame;
	currentFrame++;
	*vbuf = picture->data[idxVideo];
	return 1;
}

bool VideoDecoder::seek(uint32_t frameOffset)
{
	if (!_isOpened || frameOffset >= totalFrames)
		return false;
	if (frameOffset == currentFrame)
	{
		lastFrame = currentFrame - 1;
		return true;
	}
	if (frameOffset == currentFrame - 1)
	{
		lastFrame = currentFrame;
		return true;
	}
	int64_t targetFrame = frameOffset - 1;
	auto ret = avformat_seek_file(pFormatCtx, idxVideo,
		targetFrame, targetFrame, targetFrame, AVSEEK_FLAG_FRAME);
	avcodec_flush_buffers(pCodecCtx);
	currentFrame = targetFrame;
	return ret >= 0;
}

bool VideoDecoder::seekTime(double sec)
{
	auto target = sec / durationV * totalFrames;
	int64_t targetFrame = (int64_t)target;
	if (!_isOpened|| targetFrame >= totalFrames)
	{
		VINFO("failed");
		return false;
	}
	double df = target - currentFrame;
	if (targetFrame == currentFrame || (-0.3 < df&&df < 0))
	{
		lastFrame = currentFrame - 1;
		//VINFO("pass");
		return true;
	}
	if (targetFrame == currentFrame - 1)
	{
		lastFrame = currentFrame;
		//VINFO("wait");
		return true;
	}
	if (df <= 0)
	{
		// note: wait once
		//VINFO("wait %f from %d to %f", -df, (int32_t)currentFrame, target);
		lastFrame = currentFrame;
		return true;
	}
	else
	{
		// have problem here:
		//auto cost = int(df) - 1;
		//if (cost > 1)
		//{
		//	uint8_t* tmp;
		//	for (size_t i = 0; i < cost; i++)
		//	{
		//		lastFrame = -2;
		//		read(&tmp);
		//	}
		//	VINFO("cost %f to %f", df-1, target);
		//}
		// note: assert fps <= 60
		lastFrame = currentFrame - 1;
	}
	return true;
}

uint32_t VideoDecoder::tell() const
{
	return currentFrame;
}

uint32_t VideoDecoder::getTotalFrames() const
{
	return totalFrames;
}

void VideoDecoder::setVideoInfo(bool print)
{
	if (!_isOpened)
		return;
	// AVFormatContext
	videoInfo.filename = pFormatCtx->filename;
	videoInfo.duration = pFormatCtx->duration;
	videoInfo.bitRate = pFormatCtx->bit_rate;
	videoInfo.nb_streams = pFormatCtx->nb_streams;

	// AVInputFormat
	auto iformat = pFormatCtx->iformat;
	if (iformat)
	{
		videoInfo.iformatName = iformat->name;
		videoInfo.iformatLongName = iformat->long_name;
		videoInfo.iformatExtensions = iformat->extensions;
		videoInfo.iformatID = iformat->raw_codec_id;		
	}

	// AVStream
	auto stV = (idxVideo >= 0) ? pFormatCtx->streams[idxVideo] : nullptr;
	auto stA = (idxAudio >= 0) ? pFormatCtx->streams[idxAudio] : nullptr;
	if (stV)
	{
		videoInfo.nFramesV = stV->nb_frames;
		videoInfo.durationV = stV->duration;
		AVRational rational = stV->avg_frame_rate;
		if (!rational.num || !rational.den)
			rational = stV->r_frame_rate;
		videoInfo.frameRateV = av_q2d(rational);
		videoInfo.timeBaseV = av_q2d(stV->time_base);
	}
	if (stA)
	{
		videoInfo.nFramesA = stA->nb_frames;
		videoInfo.durationA = stA->duration;
		AVRational rational = stA->avg_frame_rate;
		if (!rational.num || !rational.den)
			rational = stA->r_frame_rate;
		videoInfo.frameRateA = av_q2d(rational);
		videoInfo.timeBaseA = av_q2d(stA->time_base);
	}

	// AVCodecParameters
	auto cparV = stV ? stV->codecpar : nullptr;
	auto cparA = stA ? stA->codecpar : nullptr;
	if (cparV)
	{
		videoInfo.width = cparV->width;
		videoInfo.height = cparV->height;
		videoInfo.bitRateV = cparV->bit_rate;		
	}
	if (cparA)
	{
		videoInfo.bitRateA = cparA->bit_rate;
		videoInfo.sampleRateA = cparA->sample_rate;
		videoInfo.channelsA = cparA->channels;		
	}

	// AVCodec
	videoInfo.codecNameV = pCodecV->name;
	videoInfo.codecLongNameV = pCodecV->long_name;


	string inf;
	inf += StringUtils::format("filename = %s\n", videoInfo.filename.c_str());
	inf += StringUtils::format("duration = %.2f\n", videoInfo.duration / 1e6);

	inf += StringUtils::format("iformatName = %s\n", videoInfo.iformatName.c_str());
	inf += StringUtils::format("iformatLongName = %s\n", videoInfo.iformatLongName.c_str());

	inf += StringUtils::format("nFramesV = %d\n", videoInfo.nFramesV);
	inf += StringUtils::format("nFramesA = %d\n", videoInfo.nFramesA);
	inf += StringUtils::format("frameRateV = %.2f\n", videoInfo.frameRateV);
	inf += StringUtils::format("frameRateA = %.2f\n", videoInfo.frameRateA);

	inf += StringUtils::format("width = %d\n", videoInfo.width);
	inf += StringUtils::format("height = %d\n", videoInfo.height);
	inf += StringUtils::format("bitRateV = %.1fkbps\n", videoInfo.bitRateV / 1000.0);
	inf += StringUtils::format("bitRateA = %.1fkbps\n", videoInfo.bitRateA / 1000.0);
	inf += StringUtils::format("sampleRateA = %d\n", videoInfo.sampleRateA);
	inf += StringUtils::format("channelsA = %d\n", videoInfo.channelsA);

	inf += StringUtils::format("codecNameV = %s\n", videoInfo.codecNameV.c_str());
	inf += StringUtils::format("codecLongNameV = %s\n", videoInfo.codecLongNameV.c_str());

	readableInfo = inf;
	if (print)
	{
		VINFO("videoInfo:\n%m", inf.c_str());
	}
}

VideoDecoder::VideoDecoder()
{
}

VideoDecoder::~VideoDecoder()
{
	close();
}
