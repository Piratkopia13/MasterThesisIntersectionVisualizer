#pragma once

#include <string.h>
#include "Sail/resources/TextureData.h"

class Texture {
public:
	enum ADDRESS_MODE {
		WRAP,
		MIRROR,
		CLAMP,
		BORDER,
		MIRROR_ONCE
	};
	enum FILTER {
		MIN_MAG_MIP_POINT,
		MIN_MAG_POINT_MIP_LINEAR,
		MIN_POINT_MAG_LINEAR_MIP_POINT,
		MIN_POINT_MAG_MIP_LINEAR,
		MIN_LINEAR_MAG_MIP_POINT,
		MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		MIN_MAG_LINEAR_MIP_POINT,
		MIN_MAG_MIP_LINEAR,
		ANISOTROPIC
		// TODO: add more filters if needed
	};
	
public:
	static Texture* Create(const std::string& filename, bool useAbsolutePath = false);
	Texture(const std::string& filename);
	virtual ~Texture() {}

	const std::string& getName() const;

protected:
	TextureData& getTextureData(const std::string& filename, bool useAbsolutePath) const;

private:
	std::string m_name;

};