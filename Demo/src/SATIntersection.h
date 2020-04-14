#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <ctpl_stl.h>

class SATIntersection {
public:
	SATIntersection(unsigned int trianglesPerMesh, int earlyExitLevel = 2);
	virtual ~SATIntersection();

	void setEarlyExitLevel(int earlyExitLevel);

	bool testIntersection(const std::string& inputFilePath);
	bool testIntersection(void* data, unsigned int byteSize);

	bool getLastIntersectionResult() const;
	double getLastIntersectionTime() const;

private:
	unsigned int m_trianglesPerMesh;
	std::vector<glm::vec3> m_lastMesh1;
	std::vector<glm::vec3> m_lastMesh2;
	int m_earlyExitLevel;

	bool m_lastIntersectionResult;
	double m_lastIntersectionTime;

	int m_numThreads;
	std::unique_ptr<ctpl::thread_pool> m_threadPool;

private:
	void convertToMeshes(void* data, unsigned int byteSize);

	// ----SAT functions----
	static std::vector<glm::vec3> getEdges(const glm::vec3 tri[3]);
	static std::vector<glm::vec3> getAxes(const glm::vec3 tri1[3], const glm::vec3 tri2[3]);
	static bool projectionOverlapTest(glm::vec3& testVec, const glm::vec3 tri1[3], const glm::vec3 tri2[3]);
	static bool SAT(const glm::vec3 tri1[3], const glm::vec3 tri2[3], int earlyExitLevel);
	bool meshVsMeshIntersection(std::vector<glm::vec3> &mesh1, std::vector<glm::vec3> &mesh2, int earlyExitLevel);
	static bool meshVsMeshThread(int id, std::vector<glm::vec3> &mesh1, std::vector<glm::vec3> &mesh2, size_t startTriangle, size_t stopTriangle, int earlyExitLevel);
	// ---------------------
};