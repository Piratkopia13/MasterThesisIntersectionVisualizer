#include "ModelViewerState.h"
#include "imgui.h"

#if defined(_SAIL_DX12) && defined(_DEBUG)
#include "API/DX12/DX12API.h"
#endif

// Command line parsing
#include <shellapi.h>
#include <atlstr.h>

#include <tensorflow/c/c_api.h>
//#include <cppflow/src/Model.cpp>
//#include <cppflow/src/Tensor.cpp>

#include "../../model_run.cpp"

ModelViewerState::ModelViewerState(StateStack& stack)
: State(stack)
, m_cam(90.f, 1280.f / 720.f, 0.1f, 5000.f)
, m_camController(&m_cam)
{
	SAIL_PROFILE_FUNCTION();
	{
		Logger::Log("TF version " + std::string(TF_Version()));

		///*
		// * Load the frozen model, the input/output tensors names must be provided.
		// * input_layer_name=conv2d_input:0
		// * output_layer_name=dense_1/Softmax
		// */
		//auto session = std::unique_ptr<MySession>(my_model_load("frozen_model.pb", "input_meshes_input:0", "result/Sigmoid"));
		//TensorShape inputShape = { {2, 600, 3}, 4 };    // python equivalent: input_shape = (1, 28, 28, 1)

		//std::vector<std::vector<std::vector<float>>> inputData;
		//// TODO: fill inputData
		//auto output = tf_obj_unique_ptr(TF_NewTensor(TF_FLOAT,
		//	inputShape.values, inputShape.dim,
		//	(void*)inputData.data(), size * sizeof(float), cpp_array_deallocator<float>, nullptr));

		//auto input_values = tf_obj_unique_ptr(ascii2tensor(str, input_shape));
		//if (!input_values) {
		//	std::cerr << "Tensor creation failure." << std::endl;
		//	__debugbreak();
		//}

		//CStatus status;
		//TF_Tensor* inputs[] = { input_values.get() };
		//TF_Tensor* outputs[1] = {};
		//TF_SessionRun(session->session.get(), nullptr,
		//	&session->inputs, inputs, 1,
		//	&session->outputs, outputs, 1,
		//	nullptr, 0, nullptr, status.ptr);
		//auto _output_holder = tf_obj_unique_ptr(outputs[0]);

		//if (status.failure()) {
		//	status.dump_error();
		//	__debugbreak();
		//}

		//TF_Tensor& output = *outputs[0];
		//if (TF_TensorType(&output) != TF_FLOAT) {
		//	std::cerr << "Error, unexpected output tensor type." << std::endl;
		//	__debugbreak();
		//}

		//{
		//	std::cout << "Prediction output: " << std::endl;
		//	size_t output_size = TF_TensorByteSize(&output) / sizeof(float);
		//	auto output_array = (const float*)TF_TensorData(&output);
		//	for (int i = 0; i < output_size; i++) {
		//		std::cout << '[' << i << "]=" << output_array[i] << ' ';
		//		if ((i + 1) % 10 == 0) std::cout << std::endl;
		//	}
		//	std::cout << std::endl;
		//}

		/*CppFlow::Model model("satnet.pb");
		model.init();

		auto inputMeshes = new CppFlow::Tensor(model, "input_meshes");
		auto output = new CppFlow::Tensor(model, "result");

		std::vector<float> data(100);
		std::iota(data.begin(), data.end(), 0);

		inputMeshes->set_data(data);

		model.run(inputMeshes, output);
		for (float f : output->get_data<float>()) {
			std::cout << f << " ";
		}
		std::cout << std::endl;*/

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
	auto* pbrShader = &m_app->getResourceManager().getShaderSet<PBRMaterialShader>();
	auto* outlineShader = &m_app->getResourceManager().getShaderSet<OutlineShader>();

	// Create/load models
	auto planeModel = ModelFactory::PlaneModel::Create(glm::vec2(50.f), pbrShader, glm::vec2(30.0f));
	auto cubeModel = ModelFactory::CubeModel::Create(glm::vec3(0.5f), pbrShader);

	// Create entities
	{
		m_mesh1 = Entity::Create("Mesh1");
		m_mesh1->addComponent<ModelComponent>(cubeModel);
		auto transformComp = m_mesh1->addComponent<TransformComponent>();
		transformComp->setTranslation(2.f, 1.f, 0.f);
		auto mat = m_mesh1->addComponent<MaterialComponent<PBRMaterial>>();
		mat->get()->setColor(glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));
		m_scene.addEntity(m_mesh1);
	}
	{
		m_mesh2 = Entity::Create("Mesh2");
		m_mesh2->addComponent<ModelComponent>(cubeModel);
		auto transformComp = m_mesh2->addComponent<TransformComponent>();
		transformComp->setTranslation(-2.f, 1.f, 0.f);
		auto mat = m_mesh2->addComponent<MaterialComponent<PBRMaterial>>();
		mat->get()->setColor(glm::vec4(0.2f, 0.2f, 0.8f, 1.0f));
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
	
	return false;
}
