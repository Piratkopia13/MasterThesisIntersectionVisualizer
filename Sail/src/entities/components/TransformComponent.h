#pragma once

#include "Component.h"
#include "../../graphics/geometry/Transform.h"

class TransformComponent : public Component {
public:
	SAIL_COMPONENT
	TransformComponent() { }
	~TransformComponent() { }

	Transform& getTransform() {
		return m_transform;
	}

private:
	Transform m_transform;
};