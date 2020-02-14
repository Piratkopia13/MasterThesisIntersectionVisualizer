#pragma once
#include <string>
#include <memory>

class TF_Tensor;
class TensorShape;
class MySession;

class TFPredictor {
public:
	TFPredictor(const std::string& modelPath, const std::string& inputLayerName, const std::string& outputLayerName, unsigned int numTrianglesPerMesh);
	~TFPredictor();

	bool predict(const std::string& inputFilePath);
	bool predict(void* data, unsigned int dataSize);

	float getLastPredictionValue() const;
	long long getLastPredictionTime() const;

private:
	TF_Tensor* createTensor(void* inputData, unsigned int dataSize, TensorShape* inputShape);

private:
	std::unique_ptr<MySession> m_session;
	TensorShape* m_inputShape;
	float m_lastPrediction;
	long long m_lastPredictionTime;

};