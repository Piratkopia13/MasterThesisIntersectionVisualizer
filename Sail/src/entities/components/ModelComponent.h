#pragma once

#include "Component.h"
class Model;

class ModelComponent : public Component {
public:
	SAIL_COMPONENT
	ModelComponent(Model* model)
		: m_model(model)
	{ }
	~ModelComponent() { }

	Model* getModel() {
		return m_model;
	}

private:
	Model* m_model;
};