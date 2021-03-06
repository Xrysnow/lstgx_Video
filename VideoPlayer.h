﻿#pragma once
#include "cocos2d.h"
#include "VideoCommon.h"
#include "VideoDecoder.h"
#include <functional>

namespace video
{
	//TODO: cache
	class Player : public cocos2d::Sprite
	{
	public:
		enum PlayMode
		{
			STEP,	   // step a const time per frame
			REALTIME,  // realtime time -> video time, not recommand
			FRAME,	   // game frame -> video frame
			MANUAL,	   // invoke update manually
		};
		static Player* create(const std::string& path, int width = -1, int height = -1);
	protected:
		bool init_(const std::string& path, int width = -1, int height = -1);
	public:
		void vplay();
		void vstop();
		void vpause();
		void vresume();
		// can only seek key frame, not recommand to set a non-0 value
		void seek(uint32_t frame);
		// step dt in decoder and get data, will step 1 frame if dt<0
		void update(float dt) override;

		// see PlayMode
		void setPlayMode(PlayMode m);
		// used for STEP mode
		void setStep(double v) { step = v; }
		// auto invoke [removeFromParentAndCleanup] when finished
		void setAutoRemove(bool b) { autoRemove = b; }
		// set callback when finished
		void setVideoEndCallback(const std::function<void()>& func);
		// save current frame to file
		void saveCurrentFrame(const std::string& path);

	protected:
		void draw(cocos2d::Renderer* renderer, const cocos2d::Mat4& transform, uint32_t flags) override;
		void playerSeekTime(double sec);
		Decoder* decoder = nullptr;
		VideoInfo videoInfo;
		uint8_t* vbuf = nullptr;

		double currentTime = 0.0;
		double step = 1.0 / 60;
		bool autoUpdate = false;
		bool autoRemove = false;
		bool isPlaying = false;
		bool texureDirty = false;
		PlayMode mode = STEP;

		std::function<void()> videoEndCallback;
		cocos2d::CallbackCommand cmdBeforeDraw;

		Player();
		virtual ~Player();
	};
}
