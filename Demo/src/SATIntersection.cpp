#include "SATIntersection.h"

#include "Sail.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>

SATIntersection::SATIntersection(unsigned int trianglesPerMesh, int earlyExitLevel) {
	m_trianglesPerMesh = trianglesPerMesh;
	m_earlyExitLevel = earlyExitLevel;

	m_lastIntersectionTime = 0.f;
	m_lastIntersectionResult = false;


	m_numThreads = std::ceil(m_trianglesPerMesh / 10.0f);
	m_threadPool = std::unique_ptr<ctpl::thread_pool>(SAIL_NEW ctpl::thread_pool(m_numThreads));
}

SATIntersection::~SATIntersection() {

}

void SATIntersection::setEarlyExitLevel(int earlyExitLevel) {
	m_earlyExitLevel = earlyExitLevel;
}

bool SATIntersection::testIntersection(const std::string& inputFilePath) {
	std::vector<float> inputData;
	inputData.reserve((size_t)(m_trianglesPerMesh) * 3 * 3 * 2);

	std::string line;
	std::ifstream infile(inputFilePath);
	while (std::getline(infile, line)) {
		std::istringstream iss(line);
		float c;
		while (iss >> c) {
			inputData.emplace_back(c);
		}
	};

	return testIntersection(inputData.data(), inputData.size() * sizeof(float));
}

bool SATIntersection::testIntersection(void* data, unsigned int byteSize) {
	convertToMeshes(data, byteSize);

	auto startTime = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();

	bool result = meshVsMeshIntersection(m_lastMesh1, m_lastMesh2, m_earlyExitLevel);

	auto endTime = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();

	m_lastIntersectionTime = (endTime - startTime) / 1000.0;
	m_lastIntersectionResult = result;

	return result;
}

bool SATIntersection::getLastIntersectionResult() const {
	return m_lastIntersectionResult;
}

double SATIntersection::getLastIntersectionTime() const {
	return m_lastIntersectionTime;
}

void SATIntersection::convertToMeshes(void* data, unsigned int byteSize) {
	m_lastMesh1.clear();
	m_lastMesh2.clear();

	m_lastMesh1.resize(m_trianglesPerMesh * 3);
	m_lastMesh2.resize(m_trianglesPerMesh * 3);

	memcpy(m_lastMesh1.data(), data, byteSize / 2);

	data = static_cast<void*>(static_cast<char*>(data) + (byteSize / 2));

	memcpy(m_lastMesh2.data(), data, byteSize / 2);
}

std::vector<glm::vec3> SATIntersection::getEdges(const glm::vec3 tri[3]) {
	std::vector<glm::vec3> edges = { tri[1] - tri[0], tri[2] - tri[0], tri[2] - tri[1] };
	return edges;
}

std::vector<glm::vec3> SATIntersection::getAxes(const glm::vec3 tri1[3], const glm::vec3 tri2[3]) {
	std::vector<glm::vec3> axes(11);

	std::vector<glm::vec3> edges1 = getEdges(tri1);
	std::vector<glm::vec3> edges2 = getEdges(tri2);

	axes[0] = glm::cross(edges1[0], edges1[1]);
	axes[1] = glm::cross(edges2[0], edges2[1]);

	for (size_t i = 0; i < edges1.size(); i++) {
		for (size_t j = 0; j < edges2.size(); j++) {
			axes[2 + i * 3 + j] = glm::cross(edges1[i], edges2[j]);
		}
	}
	return axes;
}

bool SATIntersection::projectionOverlapTest(glm::vec3& testVec, const glm::vec3 tri1[3], const glm::vec3 tri2[3]) {
	testVec = glm::normalize(testVec);
	float min1 = 9999.0f, min2 = 9999.0f;
	float max1 = -9999.0f, max2 = -9999.0f;

	for (int i = 0; i < 3; i++) {
		float tempDot1 = glm::dot(tri1[i], testVec);

		if (tempDot1 < min1) {
			min1 = tempDot1;
		}
		if (tempDot1 > max1) {
			max1 = tempDot1;
		}

		float tempDot2 = glm::dot(tri2[i], testVec);

		if (tempDot2 < min2) {
			min2 = tempDot2;
		}
		if (tempDot2 > max2) {
			max2 = tempDot2;
		}
	}

	return max2 >= min1 && max1 >= min2;
}

bool SATIntersection::SAT(const glm::vec3 tri1[3], const glm::vec3 tri2[3], int earlyExitLevel) {
	bool returnValue = true;
	std::vector<glm::vec3> axes = getAxes(tri1, tri2);

	for (size_t i = 0; i < axes.size(); i++) {
		if (!projectionOverlapTest(axes[i], tri1, tri2)) {
			if (earlyExitLevel >= 2) {
				return false;
			}
			else {
				returnValue = false;
			}
		}
	}

	return returnValue;
}

bool SATIntersection::meshVsMeshIntersection(std::vector<glm::vec3> &mesh1, std::vector<glm::vec3> &mesh2, int earlyExitLevel) {
	bool returnValue = false;

	unsigned int trianglesPerThread = std::ceil(m_trianglesPerMesh / m_numThreads);
	std::vector<std::future<bool>> futures;
	for (int i = 0; i < m_numThreads; i++) {
		futures.push_back(m_threadPool->push(meshVsMeshThread, mesh1, mesh2, trianglesPerThread * i, glm::min(trianglesPerThread * (i + 1), m_trianglesPerMesh), earlyExitLevel));
	}

	for (int i = 0; i < m_numThreads; i++) {
		if (futures[i].get()) {
			returnValue = true;
		}
	}

	return returnValue;
}

bool SATIntersection::meshVsMeshThread(int id, std::vector<glm::vec3> &mesh1, std::vector<glm::vec3> &mesh2, size_t startTriangle, size_t stopTriangle, int earlyExitLevel) {
	bool returnValue = false;

	for (size_t i = startTriangle * 3; i < stopTriangle * 3; i += 3) {
		glm::vec3 tri1[3] = { mesh1[i], mesh1[i + 1], mesh1[i + 2] };
		for (size_t j = 0; j < mesh2.size(); j += 3) {
			glm::vec3 tri2[3] = { mesh2[j], mesh2[j + 1], mesh2[j + 2] };
			if (SAT(tri1, tri2, earlyExitLevel)) {
				if (earlyExitLevel >= 1) {
					return true;
				}
				else {
					returnValue = true;
				}
			}
		}
	}
	return returnValue;
}


