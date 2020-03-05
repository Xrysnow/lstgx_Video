#include "VideoDecoder.h"
#include "cocos2d.h"

using namespace std;
using namespace cocos2d;
using namespace video;

namespace
{
	AVPixelFormat HW_PIX_FMT = AV_PIX_FMT_NONE;
	enum AVPixelFormat get_hw_format(AVCodecContext*,
		const enum AVPixelFormat* pix_fmts)
	{
		const enum AVPixelFormat *p;
		for (p = pix_fmts; *p != -1; p++) {
			if (*p == HW_PIX_FMT)
				return *p;
		}
		VERRO("failed to get HW surface format");
		return AV_PIX_FMT_NONE;
	}

	std::unique_ptr<AVPacket, std::function<void(AVPacket*)>> newPacket()
	{
		return std::unique_ptr<AVPacket, std::function<void(AVPacket*)>>{
			new AVPacket, [](AVPacket* p) { av_packet_unref(p); delete p; } };
	}
	std::unique_ptr<AVFrame, std::function<void(AVFrame*)>> newFrame()
	{
		return std::unique_ptr<AVFrame, std::function<void(AVFrame*)>>{
			av_frame_alloc(), [](AVFrame* p) { av_frame_free(&p); } };
	}
}

static void lazyInit()
{
	static bool inited = false;
	if (!inited)
	{
		inited = true;
		void* opaque = nullptr;

		std::string protocol_in = "input protocol:";
		auto pro_in = avio_enum_protocols(&opaque, 0);
		while (pro_in != nullptr) {
			protocol_in += " ";
			protocol_in += pro_in;
			pro_in = avio_enum_protocols(&opaque, 0);
		}
		opaque = nullptr;
		std::string protocol_out = "output protocol:";
		auto pro_out = avio_enum_protocols(&opaque, 1);
		while (pro_out != nullptr) {
			protocol_out += " ";
			protocol_out += pro_out;
			pro_out = avio_enum_protocols(&opaque, 1);
		}
		opaque = nullptr;
		std::string demuxer = "demuxer:";
		auto inputFormat = av_demuxer_iterate(&opaque);
		while (inputFormat != nullptr) {
			demuxer += " ";
			demuxer += inputFormat->name;
			inputFormat = av_demuxer_iterate(&opaque);
		}
		opaque = nullptr;
		std::string muxer = "muxer:";
		auto outputFormat = av_muxer_iterate(&opaque);
		while (outputFormat != nullptr) {
			muxer += " ";
			muxer += outputFormat->name;
			outputFormat = av_muxer_iterate(&opaque);
		}
		opaque = nullptr;
		std::string encoder = "encoder:";
		std::string decoder = "decoder:";
		std::string encoder_hw = "encoder_hw:";
		std::string decoder_hw = "decoder_hw:";
		auto avCodec = av_codec_iterate(&opaque);
		while (avCodec != nullptr) {
			std::string name;
			if (avCodec->name)
				name += avCodec->name;
			if (avCodec->long_name)
				name += "(" + std::string(avCodec->long_name) + ")";
			if (name.empty())
				name = "UNKNOWN";
			name = " " + name;
			if (av_codec_is_encoder(avCodec)) {
				if (avcodec_get_hw_config(avCodec, 0))
					encoder_hw += name;
				else
					encoder += name;
			}
			if (av_codec_is_decoder(avCodec)) {
				if (avcodec_get_hw_config(avCodec, 0))
					decoder_hw += name;
				else
					decoder += name;
			}
			avCodec = av_codec_iterate(&opaque);
		}
		opaque = nullptr;
		std::string bsf = "bsf:";
		auto bsf_ = av_bsf_iterate(&opaque);
		while (bsf_ != nullptr) {
			bsf += " ";
			bsf += bsf_->name;
			bsf_ = av_bsf_iterate(&opaque);
		}

		//VINFO("%s", protocol_in.c_str());
		//VINFO("%s", protocol_out.c_str());
		//VINFO("%s", demuxer.c_str());
		//VINFO("%s", muxer.c_str());
		//VINFO("%s", encoder.c_str());
		//VINFO("%s", encoder_hw.c_str());
		//VINFO("%s", decoder.c_str());
		//VINFO("%s", decoder_hw.c_str());
		//VINFO("%s", bsf.c_str());
	}
}

Decoder* Decoder::create(const std::string& path)
{
	auto ret = new (std::nothrow) Decoder;
	if (ret&&ret->open(path))
	{
		ret->autorelease();
		return ret;
	}
	CC_SAFE_DELETE(ret);
	return nullptr;
}

bool Decoder::open(const std::string& path)
{
	if (_isOpened)
		return false;
	filePath = FileUtils::getInstance()->fullPathForFilename(path);
	if (filePath.empty())
	{
		VERRO("no file: %s", path.c_str());
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
		auto pAVCodecContext = avcodec_alloc_context3(nullptr);
		if (!pAVCodecContext) {
			VERRO("avcodec_alloc_context3 failed");
			break;
		}
		avcodec_parameters_to_context(pAVCodecContext, pFormatCtx->streams[i]->codecpar);
		const auto type = pAVCodecContext->codec_type;
		avcodec_free_context(&pAVCodecContext);
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
	pCodecCtx = avcodec_alloc_context3(nullptr);
	if (!pCodecCtx) {
		VERRO("avcodec_alloc_context3 failed");
		return false;
	}
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[idxVideo]->codecpar);
	//auto codec_id = pFormatCtx->streams[idxVideo]->codecpar->codec_id;

	// Find the decoder for the video stream
	pCodecV = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodecCtx->codec_id == AV_CODEC_ID_H264)
	{
		AVCodec* codec_hw = nullptr;
		do
		{
			if (createHWDevice(AV_HWDEVICE_TYPE_QSV))
			{
				codec_hw = avcodec_find_decoder_by_name("h264_qsv");
				if(codec_hw)
					break;
			}
			if (createHWDevice(AV_HWDEVICE_TYPE_CUDA))
			{
				codec_hw = avcodec_find_decoder_by_name("h264_cuvid");
				if(codec_hw)
					break;
			}
		} while (false);
		if (codec_hw)
		{
			VINFO("hardware device: %s", av_hwdevice_get_type_name(hw_device_type));
			pCodecV = codec_hw;
			HW_PIX_FMT = hw_pix_fmt;
			pCodecCtx->get_format = get_hw_format;
		}
	}
	if (!pCodecV) {
		VERRO("can't get video decoder");
		return false;
	}
	// open codec
	if (avcodec_open2(pCodecCtx, pCodecV, nullptr)) {
		VERRO("can't open video codec [%s]", pCodecV->name);
		return false;
	}
	// get rational
	AVRational rational = pFormatCtx->streams[idxVideo]->avg_frame_rate;
	if (!rational.num || !rational.den)
		rational = pFormatCtx->streams[idxVideo]->r_frame_rate;

	if (pFormatCtx->duration != AV_NOPTS_VALUE) {
		const auto duration = pFormatCtx->duration;// +5000;
		const auto secs = duration / AV_TIME_BASE;
		const auto us = duration % AV_TIME_BASE;
		durationCtx = secs + (double)us / AV_TIME_BASE;
	}
	else {
		VERRO("can't get duration");
		return false;
	}

	const double fps = av_q2d(rational);
	frame_dt = 1.0 / fps;
	totalFrames = pFormatCtx->streams[idxVideo]->nb_frames;
	
	// Allocate video frame
	pFrame = av_frame_alloc();
	pFrameSW = av_frame_alloc();
	if (!pFrame || !pFrameSW)
	{
		VERRO("can't alloc frame");
		return false;
	}

	// File Information
	//av_dump_format(pFormatCtx, 0, m_filePath.c_str(), 0);

	// raw size
	raw_width = pCodecCtx->width;
	raw_height = pCodecCtx->height;
	timeBaseV = pFormatCtx->streams[idxVideo]->time_base;
	//timeBaseA = pFormatCtx->streams[idxAudio]->time_base;
	durationV = pFormatCtx->streams[idxVideo]->duration * av_q2d(timeBaseV);
	_isOpened = true;
	setVideoInfo(true);
	return true;
}

bool Decoder::open(VideoStream* stream, double loopA, double loopB)
{
	if (_isOpened || !stream)
		return false;
	//TODO:
	return true;
}

bool Decoder::setup()
{
	return setup(Size(raw_width, raw_height));
}

bool Decoder::setup(const Size& target_size)
{
	const int width = (int)target_size.width;
	const int height = (int)target_size.height;
	if (width <= 0 || height <= 0)
	{
		return false;
	}
	targetSize = Size(width, height);
	// TODO: sws to YUV420 and convert in shader
	const auto fmt = AVPixelFormat::AV_PIX_FMT_RGB24;
	auto ret = av_image_alloc(sws_pointers, sws_linesizes,
		width, height, fmt, 1);
	if (ret < 0)
	{
		VERRO("can't alloc image");
		return false;
	}
	auto srcFormat = pCodecCtx->pix_fmt;
	if (usingHardware() && srcFormat == hw_pix_fmt)
		srcFormat = sw_pix_fmt;
	assert(srcFormat >= 0);
	// scale/convert
	img_convert_ctx = sws_getContext(
		pCodecCtx->width,
		pCodecCtx->height,
		srcFormat,
		width,
		height,
		fmt,
		SWS_FAST_BILINEAR,
		nullptr, nullptr, nullptr);
	if (!img_convert_ctx)
	{
		VERRO("can't create sws context");
		return false;
	}
	return true;
}

bool Decoder::isOpened() const
{
	return _isOpened;
}

void Decoder::close()
{
	if(img_convert_ctx)
	{
		sws_freeContext(img_convert_ctx);
		img_convert_ctx = nullptr;
	}
	if (sws_pointers[0])
	{
		av_freep(&sws_pointers[0]);
	}
	if(pFrame)
	{
		av_frame_free(&pFrame);
		pFrame = nullptr;
	}
	if(pFrameSW)
	{
		av_frame_free(&pFrameSW);
		pFrameSW = nullptr;
	}
	if (pCodecCtx)
	{
		avcodec_free_context(&pCodecCtx);
		pCodecCtx = nullptr;
	}
	if (pFormatCtx)
	{
		avformat_close_input(&pFormatCtx);
		pFormatCtx = nullptr;
	}
	if (hw_device_ctx)
	{
		av_buffer_unref(&hw_device_ctx);
		hw_device_ctx = nullptr;
	}
	if (stream)
	{
		VINFO("stream ref = %d", stream->getReferenceCount());
		stream->seek(VideoStream::SeekOrigin::BEGINNING, 0);
		stream->release();
		stream = nullptr;
	}
	_isOpened = false;
}

uint32_t Decoder::read(uint8_t** vbuf)
{
	*vbuf = nullptr;
	if (currentFrame == (totalFrames - 1) || !_isOpened)
		return 0;
	if (lastFrame == currentFrame)
	{
		*vbuf = usingHardware() ? pFrameSW->data[idxVideo] : pFrame->data[idxVideo];
		return 0;
	}
	int result = 0;
	int frameFinished = 0;
	auto packet_ = newPacket();
	auto packet = packet_.get();
	av_init_packet(packet);
	packet->data = nullptr;
	while (!frameFinished && av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == idxVideo) {
			bool sent = false;
			while (!sent)
			{
				sent = send_packet(packet);
				if (receive_frame(pFrame))
				{
					result = 1;
					frameFinished = 1;
					break;
				}
			}
		}
	}
	if (result == 0)
		return 0;
	auto frame = pFrame;
	if (usingHardware() && pFrame->format == hw_pix_fmt)
	{
		auto ret = av_hwframe_transfer_data(pFrameSW, pFrame, 0);
		if (ret < 0)
		{
			VERRO("failed to transfer hw data: %s", getErrorString(ret).c_str());
			return 0;
		}
		frame = pFrameSW;
	}
	sws_scale(img_convert_ctx,
		frame->data, frame->linesize,
		0, pCodecCtx->height,
		sws_pointers, sws_linesizes);
	lastFrame = currentFrame;
	currentFrame++;
	*vbuf = sws_pointers[0];
	return result;
}

bool Decoder::seek(uint32_t frameOffset)
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

bool Decoder::playerSeekTime(double sec)
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

uint32_t Decoder::tell() const
{
	return currentFrame;
}

uint32_t Decoder::getTotalFrames() const
{
	return totalFrames;
}

bool Decoder::send_packet(AVPacket* packet)
{
	const auto ret = avcodec_send_packet(pCodecCtx, packet);
	if (ret < 0) {
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			return false;
		}
		else
		{
			// error
		}
	}
	return true;
}

bool Decoder::receive_frame(AVFrame* frame)
{
	const auto ret = avcodec_receive_frame(pCodecCtx, frame);
	if (ret < 0) {
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			return false;
		}
		else
		{
			// error
		}
	}
	return true;
}

AVPixelFormat Decoder::getHWPixelFormat(AVHWDeviceType type)
{
	for (auto i = 0;;i++) {
		auto config = avcodec_get_hw_config(pCodecV, i);
		if (!config) {
			// not supported
			return AV_PIX_FMT_NONE;
		}
		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
			config->device_type == type) {
			return config->pix_fmt;
		}
	}
}

bool Decoder::createHWDevice(AVHWDeviceType type)
{
	if (hw_device_ctx)
	{
		av_buffer_unref(&hw_device_ctx);
		hw_device_ctx = nullptr;
	}
	auto ret = av_hwdevice_ctx_create(
		&hw_device_ctx, type, nullptr, nullptr, 0);
	if (ret < 0 || !hw_device_ctx)
		return false;
	pCodecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
	hw_device_type = type;
	hw_pix_fmt = getHWPixelFormat(type);
	auto hw_frames_const = av_hwdevice_get_hwframe_constraints(hw_device_ctx, nullptr);
	if (!hw_frames_const || !hw_frames_const->valid_sw_formats)
	{
		return false;
	}
	for (auto p = hw_frames_const->valid_sw_formats; *p != AV_PIX_FMT_NONE; p++)
	{
		// check if we can convert to the desired format
		if (sws_isSupportedInput(*p))
		{
			// this format can be used with swscale
			sw_pix_fmt = *p;
			break;
		}
	}
	return true;
}

bool Decoder::usingHardware()
{
	return hw_device_type != AV_HWDEVICE_TYPE_NONE;
}

void Decoder::setVideoInfo(bool print)
{
	if (!_isOpened)
		return;
	// AVFormatContext
	if(pFormatCtx->url)
		videoInfo.filename = pFormatCtx->url;
	videoInfo.duration = pFormatCtx->duration;
	videoInfo.bitRate = pFormatCtx->bit_rate;
	videoInfo.nb_streams = pFormatCtx->nb_streams;

	// AVInputFormat
	auto iformat = pFormatCtx->iformat;
	if (iformat)
	{
		if(iformat->name)
			videoInfo.iformatName = iformat->name;
		if(iformat->long_name)
			videoInfo.iformatLongName = iformat->long_name;
		if(iformat->extensions)
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
	if(pCodecV->name)
		videoInfo.codecNameV = pCodecV->name;
	if(pCodecV->long_name)
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
		VINFO("videoInfo:\n%s", inf.c_str());
	}
}

Decoder::Decoder()
{
}

Decoder::~Decoder()
{
	close();
}
