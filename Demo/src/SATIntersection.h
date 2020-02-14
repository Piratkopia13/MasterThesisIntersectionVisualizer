#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

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

private:
	void convertToMeshes(void* data, unsigned int byteSize);

	// ----SAT functions----
	std::vector<glm::vec3> getEdges(const glm::vec3 tri[3]);
	std::vector<glm::vec3> getAxes(const glm::vec3 tri1[3], const glm::vec3 tri2[3]);
	bool projectionOverlapTest(glm::vec3& testVec, const glm::vec3 tri1[3], const glm::vec3 tri2[3]);
	bool SAT(const glm::vec3 tri1[3], const glm::vec3 tri2[3], int earlyExitLevel);
	bool meshVsMeshIntersection(std::vector<glm::vec3> mesh1, std::vector<glm::vec3> mesh2, int earlyExitLevel);
	// ---------------------
};