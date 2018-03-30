#include "ForwardRenderer.h"
#include "../shader/ShaderSet.h"
#include "../light/LightSetup.h"

using namespace DirectX::SimpleMath;

ForwardRenderer::ForwardRenderer() {

}

ForwardRenderer::~ForwardRenderer() {

}

void ForwardRenderer::begin(Camera* camera) {
	m_camera = camera;

	commandQueue.clear();
}

void ForwardRenderer::submit(Model* model, const DirectX::SimpleMath::Matrix& modelMatrix) {
	RenderCommand cmd;
	cmd.model = model;
	cmd.modelMatrix = modelMatrix.Transpose();
	commandQueue.push_back(cmd);
	//Logger::Log("Submitted model at: " + Utils::vec3ToStr(commandQueue.back().modelMatrix.Transpose().Translation()));

}

void ForwardRenderer::setLightSetup(LightSetup* lightSetup) {
	m_lightSetup = lightSetup;
}

void ForwardRenderer::end() {
	/*for (RenderCommand& command : commandQueue) {
		Logger::Log("Preparing model at: " + Utils::vec3ToStr(command.modelMatrix.Transpose().Translation()));
	}*/
}

void ForwardRenderer::present() {

	for (RenderCommand& command : commandQueue) {
		ShaderSet* shader = command.model->getShader();
		shader->bind();

		shader->setCBufferVar("sys_mWorld", &command.modelMatrix, sizeof(Matrix));
		shader->setCBufferVar("sys_mVP", &m_camera->getViewProjection().Transpose(), sizeof(Matrix));
		shader->setCBufferVar("sys_cameraPos", &m_camera->getPosition(), sizeof(Vector3));

		if (m_lightSetup) {
			auto& dlData = m_lightSetup->getDirLightData();
			auto& plData = m_lightSetup->getPointLightsData();
			shader->setCBufferVar("dirLight", &dlData, sizeof(dlData));
			shader->setCBufferVar("pointLights", &plData, sizeof(plData));
		}

		command.model->draw(*this);
	}
}
