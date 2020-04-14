#include "ModelViewerState.h"
#include "imgui.h"
#include <iomanip>

#if defined(_SAIL_DX12) && defined(_DEBUG)
#include "API/DX12/DX12API.h"
#endif

// Command line parsing
#include <shellapi.h>
#include <atlstr.h>

#include "../../TFPredictor.h"
#include "../../SATIntersection.h"

ModelViewerState::ModelViewerState(StateStack& stack)
	: State(stack)
	, m_cam(90.f, 1280.f / 720.f, 0.1f, 5000.f)
	, m_camController(&m_cam) {
	SAIL_PROFILE_FUNCTION();

	std::string inputFilePath = "networks/planets_50_tri_true.txt";
	m_trianglesPerMesh = 50;
	m_predictor = SAIL_NEW TFPredictor("networks/g10.pb", "dense_input_1", "result_1/Sigmoid", m_trianglesPerMesh);
	//TFPredictor predictor("networks/frozen_model_10k.pb", "input_meshes", "result/Sigmoid", trianglesPerMesh); // asd
	//TFPredictor predictor("networks/frozen_model_big_boy.pb", "input_meshes_4", "result_4/Sigmoid", trianglesPerMesh); // Great accuracy, terrible speed (1024 nodes first layer)
	//TFPredictor predictor("networks/frozen_model_less_big_boy.pb", "input_meshes_5", "result_5/Sigmoid", trianglesPerMesh); // (256 nodes first layer)		

	//m_predictor = SAIL_NEW TFPredictor("networks/frozen_model_big_boy.pb", "input_meshes_4", "result_4/Sigmoid", m_trianglesPerMesh); // Great accuracy, terrible speed (1024 nodes first layer)
	//m_predictor = SAIL_NEW TFPredictor("networks/frozen_model_less_big_boy.pb", "input_meshes_5", "result_5/Sigmoid", m_trianglesPerMesh); // (256 nodes first layer)		

	m_satIntersector = SAIL_NEW SATIntersection(m_trianglesPerMesh, 2);

	// Create model from input file
	Mesh::Data mesh1Data;
	Mesh::Data mesh2Data;
	{
		Mesh::vec3* vertices1 = SAIL_NEW Mesh::vec3[m_trianglesPerMesh * 3];
		Mesh::vec3* normals1 = SAIL_NEW Mesh::vec3[m_trianglesPerMesh * 3];
		Mesh::vec3* vertices2 = SAIL_NEW Mesh::vec3[m_trianglesPerMesh * 3];
		Mesh::vec3* normals2 = SAIL_NEW Mesh::vec3[m_trianglesPerMesh * 3];

		std::string line;
		std::ifstream infile(inputFilePath);
		while (std::getline(infile, line)) {
			std::istringstream iss(line);
			unsigned int numVertices = 0;
			float x;
			while (iss >> x) {
				Mesh::vec3 vert;
				vert.vec.x = x;
				iss >> vert.vec.y;
				iss >> vert.vec.z;
				if (numVertices < m_trianglesPerMesh * 3) {
					vertices1[numVertices] = vert;
				}
				else {
					vertices2[numVertices - m_trianglesPerMesh * 3] = vert;
				}
				numVertices++;
			}
		};

		for (unsigned int tri = 0; tri < m_trianglesPerMesh; tri++) {
			// Vertices 1
			{
				Mesh::vec3& vert1 = vertices1[tri * 3 + 0];
				Mesh::vec3& vert2 = vertices1[tri * 3 + 1];
				Mesh::vec3& vert3 = vertices1[tri * 3 + 2];

				glm::vec3 normal = glm::cross(vert2.vec - vert1.vec, vert3.vec - vert1.vec);
				normal = glm::normalize(normal);

				normals1[tri * 3 + 0] = Mesh::vec3(normal.x, normal.y, normal.z);
				normals1[tri * 3 + 1] = Mesh::vec3(normal.x, normal.y, normal.z);
				normals1[tri * 3 + 2] = Mesh::vec3(normal.x, normal.y, normal.z);
			}
			// Vertices 2
			{
				Mesh::vec3& vert1 = vertices2[tri * 3 + 0];
				Mesh::vec3& vert2 = vertices2[tri * 3 + 1];
				Mesh::vec3& vert3 = vertices2[tri * 3 + 2];

				glm::vec3 normal = glm::cross(vert2.vec - vert1.vec, vert3.vec - vert1.vec);
				normal = glm::normalize(normal);

				normals2[tri * 3 + 0] = Mesh::vec3(normal.x, normal.y, normal.z);
				normals2[tri * 3 + 1] = Mesh::vec3(normal.x, normal.y, normal.z);
				normals2[tri * 3 + 2] = Mesh::vec3(normal.x, normal.y, normal.z);
			}
		}

		mesh1Data.numVertices = m_trianglesPerMesh * 3;
		mesh1Data.positions = vertices1;
		mesh1Data.normals = normals1;

		mesh2Data.numVertices = m_trianglesPerMesh * 3;
		mesh2Data.positions = vertices2;
		mesh2Data.normals = normals2;
	}
	
	// Get the Application instance
	m_app = Application::getInstance();
	//m_scene = std::make_unique<Scene>(AABB(glm::vec3(-100.f, -100.f, -100.f), glm::vec3(100.f, 100.f, 100.f)));

	// Textures needs to be loaded before they can be used
	// TODO: automatically load textures when needed so the following can be removed
	Application::getInstance()->getResourceManager().loadTexture("sponza/textures/spnza_bricks_a_ddn.tga");
	Application::getInstance()->getResourceManager().loadTexture("sponza/textures/spnza_bricks_a_diff.tga");
	Application::getInstance()->getResourceManager().loadTexture("sponza/textures/spnza_bricks_a_spec.tga");

	// Set up camera with controllers
	m_cam.setPosition(glm::vec3(1.6f, 4.7f, 7.4f));
	m_camController.lookAt(glm::vec3(0.f));
	
	// Disable culling for testing purposes
	m_app->getAPI()->setFaceCulling(GraphicsAPI::NO_CULLING);

	auto* phongShader = &m_app->getResourceManager().getShaderSet<PhongMaterialShader>();
	auto* outlineShader = &m_app->getResourceManager().getShaderSet<OutlineShader>();
	auto* pbrShader = &m_app->getResourceManager().getShaderSet<PBRMaterialShader>();
	
	m_model1 = std::make_shared<Model>(mesh1Data, phongShader, "Model1");
	m_model2 = std::make_shared<Model>(mesh2Data, phongShader, "Model1");

	// Create/load models
	auto planeModel = ModelFactory::PlaneModel::Create(glm::vec2(50.f), pbrShader, glm::vec2(30.0f));
	auto cubeModel = ModelFactory::CubeModel::Create(glm::vec3(0.5f), pbrShader);

	// Create entities
	{
		m_mesh1 = Entity::Create("Mesh1");
		m_mesh1->addComponent<ModelComponent>(m_model1);
		auto transformComp = m_mesh1->addComponent<TransformComponent>();
		transformComp->setScale(10.f);
		auto mat = m_mesh1->addComponent<MaterialComponent<PhongMaterial>>();
		mat->get()->setColor(glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));
		mat->get()->getPhongSettings().ka = 2.0f;
		m_scene.addEntity(m_mesh1);
	}
	{
		m_mesh2 = Entity::Create("Mesh2");
		m_mesh2->addComponent<ModelComponent>(m_model2);
		auto transformComp = m_mesh2->addComponent<TransformComponent>();
		transformComp->setScale(10.f);
		auto mat = m_mesh2->addComponent<MaterialComponent<PhongMaterial>>();
		mat->get()->setColor(glm::vec4(0.2f, 0.2f, 0.8f, 1.0f));
		mat->get()->getPhongSettings().ka = 2.0f;
		m_scene.addEntity(m_mesh2);
	}
	// Lights
	{
		// Add a directional light
		auto e = Entity::Create("Directional light");
		glm::vec3 color(1.0f, 1.0f, 1.0f);
		glm::vec3 direction(-0.8f, -0.5f, -0.4f);
		direction = glm::normalize(direction);
		auto dirLightComp = e->addComponent<DirectionalLightComponent>(color, direction);
		dirLightComp->setIntensity(3.0f);
		m_scene.addEntity(e);
	}
}

ModelViewerState::~ModelViewerState() {
	delete m_predictor;
	delete m_satIntersector;
}

// Process input for the state
bool ModelViewerState::processInput(float dt) {
	SAIL_PROFILE_FUNCTION();

	// Update the camera controller from input devices
	m_camController.update(dt);

	// Reload shaders
	if (Input::WasKeyJustPressed(SAIL_KEY_R)) {
		m_app->getResourceManager().reloadShader<PhongMaterialShader>();
		m_app->getResourceManager().reloadShader<PBRMaterialShader>();
		m_app->getResourceManager().reloadShader<CubemapShader>();
		m_app->getResourceManager().reloadShader<OutlineShader>();
	}

	if (Input::WasKeyJustPressed(SAIL_KEY_B)) {
		{
			std::vector<glm::vec3> mesh1Data = convertMeshToVertexVector(*m_mesh1->getComponent<ModelComponent>()->getModel()->getMesh(0), *m_mesh1->getComponent<TransformComponent>());
			std::vector<glm::vec3> mesh2Data = convertMeshToVertexVector(*m_mesh2->getComponent<ModelComponent>()->getModel()->getMesh(0), *m_mesh2->getComponent<TransformComponent>());

			normalizeMeshes(mesh1Data, mesh2Data);

			std::cout << "Mesh 1 data balance ok? " << testDataBalance(mesh1Data) << '\n';
			std::cout << "Mesh 2 data balance ok? " << testDataBalance(mesh2Data) << '\n';

			std::vector<glm::vec3> vertexData = mesh1Data;
			vertexData.insert(vertexData.end(), mesh2Data.begin(), mesh2Data.end());

			bool prediction = m_predictor->predict(vertexData.data(), vertexData.size() * sizeof(glm::vec3));
			Logger::Log("Prediction: " + std::to_string(prediction));
			std::cout << "\tValue " << std::setprecision(15) << m_predictor->getLastPredictionValue() << std::endl;
			Logger::Log("\tTime " + std::to_string(m_predictor->getLastPredictionTime()) + "ms");

			
			bool actualIntersection = m_satIntersector->testIntersection(vertexData.data(), vertexData.size() * sizeof(glm::vec3));
			Logger::Log("Actual intersection: " + std::to_string(actualIntersection));
			Logger::Log("\tTime " + std::to_string(m_satIntersector->getLastIntersectionTime()) + "ms\n");

		}
	}

	return true;
}

bool ModelViewerState::update(float dt) {
	SAIL_PROFILE_FUNCTION();

	std::wstring fpsStr = std::to_wstring(m_app->getFPS());

#ifdef _DEBUG
	std::string config = "Debug";
#else
	std::string config = "Release";
#endif
	m_app->getWindow()->setWindowTitle("Sail | Game Engine Demo | " + Application::getPlatformName() + " " + config + " | FPS: " + std::to_string(m_app->getFPS()));

	return true;
}

// Renders the state
bool ModelViewerState::render(float dt) {
	SAIL_PROFILE_FUNCTION();

#if defined(_SAIL_DX12) && defined(_DEBUG)
	static int framesToCapture = 0;
	static int frameCounter = 0;

	int numArgs;
	LPWSTR* args = CommandLineToArgvW(GetCommandLineW(), &numArgs);
	if (numArgs > 2) {
		std::string arg = std::string(CW2A(args[1]));
		if (arg.find("pixCaptureStartupFrames")) {
			framesToCapture = std::atoi(CW2A(args[2]));
		}
	}
	
	if (frameCounter < framesToCapture) {
		m_app->getAPI<DX12API>()->beginPIXCapture();
	}
#endif

	// Clear back buffer
	m_app->getAPI()->clear({0.1f, 0.2f, 0.3f, 1.0f});

	// Draw the scene
	m_scene.draw(m_cam);

#if defined(_SAIL_DX12) && defined(_DEBUG)
	if (frameCounter < framesToCapture) {
		m_app->getAPI<DX12API>()->endPIXCapture();
		frameCounter++;
	}
#endif

	return true;
}

bool ModelViewerState::renderImgui(float dt) {
	SAIL_PROFILE_FUNCTION();

	static auto callback = [&](EditorGui::CallbackType type, const std::string& path) {
		switch (type) {
		case EditorGui::CHANGE_STATE:
			requestStackPop();
			requestStackPush(States::Game);

			break;
		case EditorGui::ENVIRONMENT_CHANGED:
			m_scene.getEnvironment()->changeTo(path);

			break;
		default:
			break;
		}
	};
	m_editorGui.render(dt, callback);
	m_entitiesGui.render(m_scene.getEntites());

	static bool continuous = false;
	static float timeSinceLastPrediction = 0.f;
	static bool prediction = false;
	static bool actualIntersection = false;
	static bool hasRun = false;
	static float interval = 0.2f;
	ImGui::Begin("Intersection predictor");
	ImGui::Checkbox("Continuous", &continuous);
	ImGui::SliderFloat("Interval (s)", &interval, 0.f, 2.f);
	bool btn = ImGui::Button("Do et");
	if (continuous && timeSinceLastPrediction > interval || btn) {

		std::vector<glm::vec3> mesh1Data = convertMeshToVertexVector(*m_mesh1->getComponent<ModelComponent>()->getModel()->getMesh(0), *m_mesh1->getComponent<TransformComponent>());
		std::vector<glm::vec3> mesh2Data = convertMeshToVertexVector(*m_mesh2->getComponent<ModelComponent>()->getModel()->getMesh(0), *m_mesh2->getComponent<TransformComponent>());

		normalizeMeshes(mesh1Data, mesh2Data);

		std::vector<glm::vec3> vertexData = mesh1Data;
		vertexData.insert(vertexData.end(), mesh2Data.begin(), mesh2Data.end());
		prediction = m_predictor->predict(vertexData.data(), vertexData.size() * sizeof(glm::vec3));
		actualIntersection = m_satIntersector->testIntersection(vertexData.data(), vertexData.size() * sizeof(glm::vec3));

		timeSinceLastPrediction = 0.f;
		hasRun = true;
	}
	if (hasRun) {
		bool truePositive = prediction && actualIntersection;
		bool falsePositive = !prediction && actualIntersection;
		bool falseNegative = prediction && !actualIntersection;
		bool trueNegative = !prediction && !actualIntersection;

		glm::vec4 color1 = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f);
		glm::vec4 color2 = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f);
		if (truePositive || trueNegative) {
			color1 = glm::vec4(0.2f, 0.8f, 0.2f, 1.0f);
			color2 = glm::vec4(0.2f, 0.5f, 0.2f, 1.0f);
		} else {
			color1 = glm::vec4(0.8f, 0.8f, 0.2f, 1.0f);
			color2 = glm::vec4(0.5f, 0.5f, 0.2f, 1.0f);
		}

		m_mesh1->getComponent<MaterialComponent<PhongMaterial>>()->get()->setColor(color1);
		m_mesh2->getComponent<MaterialComponent<PhongMaterial>>()->get()->setColor(color2);

		ImGui::Text("Prediction: %i", prediction);
		ImGui::Text("Value: %.15f", m_predictor->getLastPredictionValue());
		ImGui::Text("Time: %.3f ms", m_predictor->getLastPredictionTime());
		ImGui::Separator();

		ImGui::Text("Actual intersection: %i", actualIntersection);
		ImGui::Text("Time: %.3f ms", m_satIntersector->getLastIntersectionTime());
		ImGui::Separator();
		ImGui::Text((truePositive) ? "True positive" : (falsePositive) ? "False positive" : (falseNegative) ? "False negative" : "True negative");
	}
	timeSinceLastPrediction += dt;
	ImGui::End();
	
	return false;
}

std::vector<glm::vec3> ModelViewerState::convertMeshToVertexVector(const Mesh& mesh, Transform& transform) {
	std::vector<glm::vec3> returnVector;
	const Mesh::Data& meshData = mesh.getData();

	if (meshData.indices) {
		returnVector.resize(meshData.numIndices);

		for (unsigned int i = 0; i < meshData.numIndices; i++) {
			returnVector[i] = glm::vec3(transform.getMatrix() * glm::vec4(meshData.positions[meshData.indices[i]].vec, 1.0));
		}
	}
	else {
		returnVector.resize(meshData.numVertices);

		for (unsigned int i = 0; i < meshData.numVertices; i++) {
			returnVector[i] = glm::vec3(transform.getMatrix() * glm::vec4(meshData.positions[i].vec, 1.0));
		}
	}
	return returnVector;
}

void ModelViewerState::normalizeMeshes(std::vector<glm::vec3>& mesh1, std::vector<glm::vec3>& mesh2) {
	// Find biggestand smallest values along each axis
	glm::vec3 minVals = mesh1[0];
	glm::vec3 maxVals = mesh1[0];

	size_t mesh1Size = mesh1.size();

	for (size_t i = 0; i < mesh1Size; i++) {
		for (unsigned int j = 0; j < 3; j++) {
			if (mesh1[i][j] < minVals[j]) {
				minVals[j] = mesh1[i][j];
			}
			if (mesh1[i][j] > maxVals[j]) {
				maxVals[j] = mesh1[i][j];
			}
		}
	}
		
	size_t mesh2Size = mesh2.size();

	for (size_t i = 0; i < mesh2Size; i++) {
		for (unsigned int j = 0; j < 3; j++) {
			if (mesh2[i][j] < minVals[j]) {
				minVals[j] = mesh2[i][j];
			}
			if (mesh2[i][j] > maxVals[j]) {
				maxVals[j] = mesh2[i][j];
			}
		}
	}

	bool negativeNormalization = true;
	// Use min and max values to normalize the data
	for (size_t i = 0; i < mesh1Size; i++) {
		mesh1[i] = { (mesh1[i][0] - minVals[0]) / (maxVals[0] - minVals[0]), (mesh1[i][1] - minVals[1]) / (maxVals[1] - minVals[1]), (mesh1[i][2] - minVals[2]) / (maxVals[2] - minVals[2]) };
		if (negativeNormalization) {
			mesh1[i][0] = mesh1[i][0] * 2.f - 1.f;
			mesh1[i][1] = mesh1[i][1] * 2.f - 1.f;
			mesh1[i][2] = mesh1[i][2] * 2.f - 1.f;
		}
	}

	for (size_t i = 0; i < mesh2Size; i++) {
		mesh2[i] = { (mesh2[i][0] - minVals[0]) / (maxVals[0] - minVals[0]), (mesh2[i][1] - minVals[1]) / (maxVals[1] - minVals[1]), (mesh2[i][2] - minVals[2]) / (maxVals[2] - minVals[2]) };
		if (negativeNormalization) {
			mesh2[i][0] = mesh2[i][0] * 2.f - 1.f;
			mesh2[i][1] = mesh2[i][1] * 2.f - 1.f;
			mesh2[i][2] = mesh2[i][2] * 2.f - 1.f;
		}
	}
}

bool ModelViewerState::testDataBalance(std::vector<glm::vec3>& mesh) {
	for (auto& v : mesh) {
		for (unsigned int i = 0; i < 3; i++) {
			if (v[i] < -1.f || v[i] > 1.f) {
				assert(false);
				return false;
			}
		}
	}
	return true;
}
