#pragma once
#include "ca4G.h"

using namespace CA4G;

class ScreenShotTechnique : public Technique {
	
public:

	gObj<Texture2D> TextureToSave;

	const char* FileName;

	void Startup() {
		TextureToSave = __create DrawableTexture2D<RGBA>(render_target->Width, render_target->Height);
	}
	
	void Frame() {
		perform(Copy);

		wait_for(signal(flush_all_to_gpu));

		perform(Save);
	}
	void Copy(gObj<GraphicsManager> manager) {
		manager _copy All(TextureToSave, render_target);
	}

	void Save(gObj<GraphicsManager> manager) {
		manager _load ToFile(TextureToSave, FileName);
	}
};