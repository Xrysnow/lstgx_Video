﻿#include "VideoPlayer.h"

using namespace video;

Player* Player::create(const std::string& path, int width, int height)
{
	auto ret = new (std::nothrow) Player();
	if (ret&&ret->init_(path, width, height))
	{
		ret->autorelease();
		return ret;
	}
	CC_SAFE_DELETE(ret);
	return nullptr;
}

Player::Player()
{
}

Player::~Player()
{
	if (decoder)
	{
		decoder->close();
		decoder->release();
	}
}

bool Player::init_(const std::string& path, int width, int height)
{
	decoder = Decoder::create(path);
	if (!decoder)
		return false;
	decoder->retain();
	if (width > 0 && height > 0)
	{
		if(!decoder->setup(cocos2d::Size(width, height)))
			return false;
	}
	else if (!decoder->setup())
		return false;
	const auto size = decoder->getTargetSize();
	const int length = size.width * size.height;
	auto texture = new cocos2d::Texture2D();
	if (!texture)
		return false;
	auto buf = new uint8_t[length * 3];
	const auto pixFormat = cocos2d::backend::PixelFormat::RGB888;
	if (!texture->initWithData(buf, length,
		pixFormat, size.width, size.height, size))
	{
		delete[] buf;
		return false;
	}
	delete[] buf;
	initWithTexture(texture);
	setContentSize(size);
	_running = true;
	update(0);

	videoInfo = decoder->getVideoInfo();
	return true;
}

void Player::vplay()
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
	//INFO("start play");
}

void Player::vstop()
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

void Player::vpause()
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

void Player::vresume()
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

void Player::playerSeekTime(double sec)
{
	if (decoder->playerSeekTime(sec))
	{
		currentTime = sec;
	}
}

void Player::seek(uint32_t frame)
{
	if (decoder->seek(frame))
	{
		currentTime = frame / videoInfo.frameRateV;
	}
}

void Player::update(float dt)
{
	cocos2d::Sprite::update(dt);
	if (dt >= 0)
	{
		// note: it assumes that framerate of the video is lower than the game
		playerSeekTime(currentTime);
		currentTime += dt;
	}
	else
	{
		currentTime += 1.0 / videoInfo.frameRateV;
	}
	auto ret = decoder->read(&vbuf);
	//if(!vbuf)
	//	VINFO("read null from decoder");
	texureDirty = ret != 0 && vbuf;
	if (decoder->tell() == decoder->getTotalFrames() - 1/* || !vbuf*/) {
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

void Player::setPlayMode(PlayMode m)
{
	if (m != REALTIME)
	{
		unscheduleUpdate();
	}
	mode = m;
}

void Player::draw(cocos2d::Renderer* renderer, const cocos2d::Mat4 &transform, uint32_t flags)
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
		if (texureDirty && vbuf)
		{
			_texture->updateWithData(vbuf, 0, 0, _texture->getPixelsWide(), _texture->getPixelsHigh());
		}
	}

	cocos2d::Sprite::draw(renderer, transform, flags);
}

void Player::setVideoEndCallback(const std::function<void()>& func)
{
	videoEndCallback = func;
}

void Player::saveCurrentFrame(const std::string& path)
{
	if (!vbuf)
		return;
	auto im = new cocos2d::Image();
	auto dat = vbuf;
	const int length = _contentSize.width * _contentSize.height;
	auto buf = new uint8_t[length * 4];
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
	delete[] buf;
}
