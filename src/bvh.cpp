#include "bvh.h"

void BVH::buildBVH(Scene* scene) {
	int numTris = scene->triangles.size();
	int rootNodeIdx = 0; 
	nodesUsed = 1;

	bvhNodePool.resize(numTris * 2 - 1);
	triIdx.resize(numTris);

	for (int i = 0; i < numTris; ++i) triIdx[i] = i;

	// set up root node
	BVHNode& bvhRoot = bvhNodePool[rootNodeIdx];
	bvhRoot.leftFirst = 0;
	bvhRoot.triCount = numTris;

	// find bounds + keep splitting
	UpdateNodeBounds(scene, rootNodeIdx);
	Subdivide(scene, rootNodeIdx);
}

// helpers 
glm::vec3 minVec3(glm::vec3 a, glm::vec3 b) {
	float minX = fminf(a[0], b[0]);
	float minY = fminf(a[1], b[1]);
	float minZ = fminf(a[2], b[2]);

	return glm::vec3(minX, minY, minZ);
}

glm::vec3 maxVec3(glm::vec3 a, glm::vec3 b) {
	float maxX = fmaxf(a[0], b[0]);
	float maxY = fmaxf(a[1], b[1]);
	float maxZ = fmaxf(a[2], b[2]);

	return glm::vec3(maxX, maxY, maxZ);
}

void BVH::UpdateNodeBounds(Scene* scene, int nodeIdx) {
	BVHNode& node = bvhNodePool[nodeIdx];
	node.aabbMin = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	node.aabbMax = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	int first = node.leftFirst;

	for (int i = 0; i < node.triCount; ++i) {
		int leafTriIndex = triIdx[first + i];
		Triangle& leafTri = scene->triangles[leafTriIndex];

		// evaluate bounds using world space triangle positions 
		glm::vec3 worldPos0 = glm::vec3(leafTri.transform * glm::vec4(leafTri.positions[0], 1.0f));
		glm::vec3 worldPos1 = glm::vec3(leafTri.transform * glm::vec4(leafTri.positions[1], 1.0f));
		glm::vec3 worldPos2 = glm::vec3(leafTri.transform * glm::vec4(leafTri.positions[2], 1.0f));

		node.aabbMin = minVec3(node.aabbMin, worldPos0);
		node.aabbMin = minVec3(node.aabbMin, worldPos1);
		node.aabbMin = minVec3(node.aabbMin, worldPos2);
		node.aabbMax = maxVec3(node.aabbMax, worldPos0);
		node.aabbMax = maxVec3(node.aabbMax, worldPos1);
		node.aabbMax = maxVec3(node.aabbMax, worldPos2);
	}
}

void BVH::Subdivide(Scene* scene, int nodeIdx) {

	BVHNode& node = bvhNodePool[nodeIdx]; 
	if (node.triCount <= 2) return;

	// split plane axis and position 
	glm::vec3 extent = node.aabbMax - node.aabbMin;
	int axis = 0;
	if (extent.y > extent.x) axis = 1;
	if (extent.z > extent[axis]) axis = 2;
	float splitPos = node.aabbMin[axis] + extent[axis] * 0.5f;

	// split the group into 2 halves
	int i = node.leftFirst;
	int j = i + node.triCount - 1;
	while (i < j) {
		if (scene->triangles[triIdx[i]].centroid[axis] < splitPos) {
			i++;
		}
		else {
			std::swap(triIdx[i], triIdx[j--]);
		}
	}

	// creating child nodes for each half 

	// abort split if one of the sides is empty 
	// some situation where > 2 triangles but all centroids are clustered on one side; want to prevent leaf w/ 0 prims
	int leftCount = i - node.leftFirst;
	if (leftCount == 0 || leftCount == node.triCount) return;

	// indices of left and right child
	int leftChildIdx = nodesUsed++;
	int rightChildIdx = nodesUsed++;
	
	// left child starts where node began
	bvhNodePool[leftChildIdx].leftFirst = node.leftFirst;
	bvhNodePool[leftChildIdx].triCount = leftCount;

	// right child starts at partition
	bvhNodePool[rightChildIdx].leftFirst = i;
	bvhNodePool[rightChildIdx].triCount = node.triCount - leftCount;

	// update (now node is an internal node; leftFirst refers to nodes in bvhNodePool isntead of beginning of primitives in triangles)
	node.leftFirst = leftChildIdx;
	node.triCount = 0;
	UpdateNodeBounds(scene, leftChildIdx);
	UpdateNodeBounds(scene, rightChildIdx);

	// recurse 
	Subdivide(scene, leftChildIdx);
	Subdivide(scene, rightChildIdx);
}