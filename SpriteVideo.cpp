#include "SpriteVideo.h"
#include "AppFrame.h"
#include "VideoHeader.h"

#define INFO(_str, ...) LINFO("[VID] %s: " _str, __func__, ##__VA_ARGS__)
#define WARN(_str,...) LWARNING("[VID] %s: " _str, __func__, ##__VA_ARGS__)
#define ERRO(_str, ...) LERROR("[VID] %s: " _str, __func__, ##__VA_ARGS__)

using namespace cocos2d;
using namespace video;

SpriteVideo* SpriteVideo::create(const char* path, int width, int height)
{
	SpriteVideo* ret = new (std::nothrow) SpriteVideo();
	if (ret&&ret->init(path, width, height)) {
		ret->autorelease();
		return ret;
	}
	CC_SAFE_DELETE(ret);
	return nullptr;
}

SpriteVideo::SpriteVideo()
{
}

SpriteVideo::~SpriteVideo()
{
	if (decoder) {
		decoder->close();
		decoder->release();
	}
}

bool SpriteVideo::init(const char* path, int width_, int height_)
{
	decoder = xVideo::VideoDecoder::create(path);
	if (!decoder)
		return false;
	decoder->retain();
	if (width_ > 0 && height_ > 0)
		decoder->setup(Size(width_, height_));
	else
		decoder->setup();
	auto size = decoder->getTargetSize();
	auto length = size.width*size.height;
	auto texture = new Texture2D();
	if (!texture)
		return false;
	auto buf = (uint8_t*)malloc(length * 3);
	if (!texture->initWithData(buf, length,
		Texture2D::PixelFormat::RGB888, size.width, size.height, size))
	{
		free(buf);
		return false;
	}
	free(buf);
	initWithTexture(texture);
	setContentSize(size);
	_running = true;
	update(0);

	videoInfo = decoder->getVideoInfo();
	return true;
}

void SpriteVideo::vplay()
{
	if (mode == MANUAL)
		return;
	seek(0);
	if (!isPlaying)
	{
		if (mode == REALTIME)
			scheduleUpdate();
		isPlaying = true;
	}
	autoUpdate = true;
	INFO("start play");
}

void SpriteVideo::vstop()
{
	if (mode == MANUAL)
		return;
	if (isPlaying)
	{
		if (mode == REALTIME)
			unscheduleUpdate();
		isPlaying = false;
		seek(0);
	}
	autoUpdate = false;
}

void SpriteVideo::vpause()
{
	if (mode == MANUAL)
		return;
	if (isPlaying&&autoUpdate)
	{
		if (mode == REALTIME)
			unscheduleUpdate();
		isPlaying = false;
	}
}

void SpriteVideo::vresume()
{
	if (mode == MANUAL)
		return;
	if (!isPlaying&&autoUpdate)
	{
		if (mode == REALTIME)
			scheduleUpdate();
		isPlaying = true;
	}
}

void SpriteVideo::seekTime(double sec)
{
	if (decoder->seekTime(sec))
	{
		currentTime = sec;
	}
}

void SpriteVideo::seek(uint32_t frame)
{
	if (decoder->seek(frame))
	{
		currentTime = frame / videoInfo.frameRateV;
	}
}

void SpriteVideo::update(float dt)
{
	Sprite::update(dt);
	if (dt >= 0)
	{
		seekTime(currentTime);
		currentTime += dt;
	}
	else
	{
		currentTime += 1.0 / videoInfo.frameRateV;
	}
	auto ret = decoder->read(&vbuf);
	texureDirty = ret != 0 && vbuf;
	if (decoder->tell() == decoder->getTotalFrames() - 1 || !vbuf) {
		vstop();
		if (videoEndCallback) {
			videoEndCallback();
		}
		if (autoRemove)
		{
			removeFromParentAndCleanup(true);
		}
		//INFO("SpriteVideo: stopped");
	}
}

void SpriteVideo::setPlayMode(PlayMode m)
{
	if (m != REALTIME)
	{
		unscheduleUpdate();
	}
	mode = m;
}

void SpriteVideo::draw(Renderer *renderer, const Mat4 &transform, uint32_t flags)
{
	if (!_texture)
		return;
	if (isPlaying)
	{
		if (mode == STEP)
		{
			update(step);
		}
		else if (mode == FRAME)
		{
			update(-1);
		}
		// TODO: use PBO
		if (texureDirty&&vbuf)
		{
			cmdBeforeDraw.func = [this]()
			{
				GL::bindTexture2D(_texture->getName());
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _contentSize.width, _contentSize.height,
					GL_RGB, GL_UNSIGNED_BYTE, vbuf);
			};
			renderer->addCommand(&cmdBeforeDraw);		
		}		
	}

	Sprite::draw(renderer, transform, flags);
}

void SpriteVideo::setVideoEndCallback(std::function<void()> func)
{
	videoEndCallback = func;
}

void SpriteVideo::saveCurrentFrame(const std::string& path)
{
	if (!vbuf)
		return;
	auto im = new Image();
	auto dat = vbuf;
	auto length = _contentSize.width * _contentSize.height;
	auto buf = (uint8_t*)malloc(length * 4);
	for (int i = 0; i < length; ++i)
	{
		buf[i * 4] = dat[i * 3];
		buf[i * 4 + 1] = dat[i * 3 + 1];
		buf[i * 4 + 2] = dat[i * 3 + 2];
		buf[i * 4 + 3] = 255;
	}
	im->initWithRawData(buf, length * 4,
		_contentSize.width, _contentSize.height, 32);
	im->saveToFile(path);
	delete im;
	free(buf);
}
