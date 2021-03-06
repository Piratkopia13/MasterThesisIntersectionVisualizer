#pragma once

#include "PostProcessStage.h"
#include "../../shader/ShaderPipeline.h"

class HGaussianBlurStage : public PostProcessStage {
public:
	HGaussianBlurStage(const Renderer& renderer, UINT width, UINT height, Mesh* fullscreenQuad);
	virtual ~HGaussianBlurStage();

	bool onResize(WindowResizeEvent& event) override;;

};