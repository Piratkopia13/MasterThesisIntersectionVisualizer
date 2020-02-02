#pragma once

#include "../entities/Entity.h"
#include "camera/Camera.h"
//#include "postprocessing/PostProcessPipeline.h"

class LightSetup;
class Renderer;
// TODO: make this class virtual and have the actual scene in the demo/game project
class Scene {
public:
	Scene();
	~Scene();

	// Adds an entity to later be drawn
	// This takes shared ownership of the entity
	void addEntity(Entity::SPtr entity);
	void setLightSetup(LightSetup* lights);
	void draw(Camera& camera);

private:
	std::vector<Entity::SPtr> m_entities;
	std::unique_ptr<Renderer> m_renderer;
	//DeferredRenderer m_renderer;
	//std::unique_ptr<DX11RenderableTexture> m_deferredOutputTex;
	//PostProcessPipeline m_postProcessPipeline;

};