#pragma once
#include "cocos2d.h"
#include <functional>
#include "VideoHeader.h"
#include "VideoDecoder.h"

namespace xVideo
{
	//TODO: cache textures
	class SpriteVideo : public cocos2d::Sprite
	{
	public:
		enum PlayMode
		{
			STEP,	   // step a const time per frame
			REALTIME,  // realtime time -> video time, not recommand
			FRAME,	   // game frame -> video frame
			MANUAL,	   // invoke update manually
		};
		static SpriteVideo* create(const char* path, int width = -1, int height = -1);

		bool init(const char* path, int width_ = -1, int height_ = -1);
		void vplay();
		void vstop();
		void vpause();
		void vresume();
		// can only seek key frame, not recommand to set a non-0 value
		void seek(uint32_t frame);
		void draw(cocos2d::Renderer *renderer, const cocos2d::Mat4& transform, uint32_t flags) override;
		// step dt in decoder and get data, will step 1 frame if dt<0
		void update(float dt) override;

		// see PlayMode
		void setPlayMode(PlayMode m);
		// used for STEP mode
		void setStep(double v) { step = v; }
		// auto invoke [removeFromParentAndCleanup] when finished
		void setAutoRemove(bool b) { autoRemove = b; }
		// set callback when finished
		void setVideoEndCallback(std::function<void()> func);
		// save current frame to file
		void saveCurrentFrame(const std::string& path);

	private:
		void seekTime(double sec);
		VideoDecoder* decoder = nullptr;
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
		cocos2d::CustomCommand cmdBeforeDraw;

		SpriteVideo();
		virtual ~SpriteVideo();
	};
}
