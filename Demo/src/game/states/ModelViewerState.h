#pragma once

#include "Sail.h"
#include "../editor/EntitiesGui.h"
#include "../editor/EditorGui.h"

class TFPredictor;
class SATIntersection;

class ModelViewerState : public State {
public:
	ModelViewerState(StateStack& stack);
	~ModelViewerState();

	// Process input for the state
	virtual bool processInput(float dt) override;
	// Updates the state
	virtual bool update(float dt) override;
	// Renders the state
	virtual bool render(float dt) override;
	// Renders imgui
	virtual bool renderImgui(float dt) override;

private:
	Application* m_app;

	EditorGui m_editorGui;
	EntitiesGui m_entitiesGui;

	// Camera
	PerspectiveCamera m_cam;
	FlyingCameraController m_camController;
	
	Model::SPtr m_model1;
	Model::SPtr m_model2;

	Entity::SPtr m_mesh1;
	Entity::SPtr m_mesh2;
	std::vector<Entity::SPtr> m_transformTestEntities;

	Scene m_scene;

	TFPredictor* m_predictor;
	SATIntersection* m_satIntersector;
	unsigned int m_trianglesPerMesh;

private:
	std::vector<glm::vec3> convertMeshToVertexVector(const Mesh& mesh, Transform& transform);
	void normalizeMeshes(std::vector<glm::vec3>& mesh1, std::vector<glm::vec3>& mesh2);
};