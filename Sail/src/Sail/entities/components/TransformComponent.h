#pragma once

#include "Component.h"
#include "../../graphics/geometry/Transform.h"

class SailGuiWindow;

class TransformComponent : public Component, public Transform {
public:
	SAIL_COMPONENT

	explicit TransformComponent(TransformComponent* parent)
		: Transform(parent) { }
	TransformComponent(const glm::vec3& translation, TransformComponent* parent) 
		: Transform(translation, parent){ }
	TransformComponent(const glm::vec3& translation = { 0.0f, 0.0f, 0.0f }, const glm::vec3& rotation = { 0.0f, 0.0f, 0.0f }, const glm::vec3& scale = { 1.0f, 1.0f, 1.0f }, TransformComponent* parent = nullptr)
		: Transform(translation, rotation, scale, parent) { }
	~TransformComponent() { }

	virtual void renderEditorGui(SailGuiWindow* window) override;

};
