#include "TFPredictor.h"
#include <sstream>
#include <chrono>

#include "model_run.cpp"

TFPredictor::TFPredictor(const std::string& modelPath, const std::string& inputLayerName, const std::string& outputLayerName, unsigned int numTrianglesPerMesh) {
	m_inputShape = new TensorShape({ {1, 2 * numTrianglesPerMesh * 3 * 3}, 2 }); // Two meshes * trianglePerMesh * three vertices per triangle * three floats per vertex

	// Load the frozen model, the input/output tensors names must be provided.
	m_session = std::unique_ptr<MySession>(my_model_load(modelPath.c_str(), inputLayerName.c_str(), outputLayerName.c_str()));
}
TFPredictor::~TFPredictor() {
	delete m_inputShape;
}

bool TFPredictor::predict(const std::string& inputFilePath) {
	std::vector<float> inputData;
	inputData.reserve(m_inputShape->values[1]);

	std::string line;
	std::ifstream infile(inputFilePath);
	while (std::getline(infile, line)) {
		std::istringstream iss(line);
		float c;
		while (iss >> c) {
			inputData.emplace_back(c);
		}
	};

	return predict(inputData.data(), inputData.size() * sizeof(float));
}

bool TFPredictor::predict(void* data, unsigned int dataSize) {
	double startTime = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() / 1000.0;

	auto tensor = createTensor(data, dataSize, m_inputShape);

	CStatus status;
	TF_Tensor* inputs[] = { tensor };
	TF_Tensor* outputs[1] = {};

	TF_SessionRun(m_session->session.get(), nullptr, &m_session->inputs, inputs, 1, &m_session->outputs, outputs, 1, nullptr, 0, nullptr, status.ptr);
	auto _output_holder = tf_obj_unique_ptr(outputs[0]);

	if (status.failure()) {
		status.dump_error();
		__debugbreak();
	}

	TF_Tensor& output = *outputs[0];
	if (TF_TensorType(&output) != TF_FLOAT) {
		std::cerr << "Error, unexpected output tensor type." << std::endl;
		__debugbreak();
	}

	double endTime = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() / 1000.0;
	m_lastPredictionTime = endTime - startTime;

	m_lastPrediction = ((const float*)TF_TensorData(&output))[0];
	return m_lastPrediction > 0.5f;
}

float TFPredictor::getLastPredictionValue() const {
	return m_lastPrediction;
}

double TFPredictor::getLastPredictionTime() const {
	return m_lastPredictionTime;
}

TF_Tensor* TFPredictor::createTensor(void* inputData, unsigned int dataSize, TensorShape* inputShape) {
	auto tensor = TF_NewTensor(TF_FLOAT,
		inputShape->values, inputShape->dim,
		inputData, dataSize, none_deallocator, nullptr);

	if (!tensor) {
		std::cerr << "Tensor creation failure." << std::endl;
		return nullptr;
	}
	return tensor;
}
