#pragma once

#include "utilities.h"
#include "sceneStructs.h"
#define TINYGLTF3_ENABLE_FS 1
#include "tiny_gltf_v3.h"

#include <glm/gtc/matrix_inverse.hpp>
//#include <glm/gtx/string_cast.hpp>
#include "json.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <cstring>
#include <vector>

//using namespace std;
using json = nlohmann::json;


struct Triangle {
    int materialid;
    glm::vec3 positions[3];
    glm::vec3 normal;
};

class Scene
{
private:
    void loadFromJSON(const std::string& jsonName);
public:
    Scene(std::string filename);
    void gltfLoad(const json& modelData, std::unordered_map<std::string, uint32_t> MatNameToID);

    std::vector<Geom> geoms;
    std::vector<Geom> lights;
    std::vector<Material> materials;
    std::vector<Triangle> triangles;

    RenderState state;
};


