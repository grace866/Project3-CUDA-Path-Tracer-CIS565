#include "scene.h"

Scene::Scene(std::string filename)
{
    std::cout << "Reading scene from " << filename << " ..." << std::endl;
    std::cout << " " << std::endl;
    auto ext = filename.substr(filename.find_last_of('.'));
    if (ext == ".json")
    {
        loadFromJSON(filename);
        return;
    }
    else
    {
        std::cout << "Couldn't read from " << filename << std::endl;
        exit(-1);
    }
}

void Scene::loadFromJSON(const std::string& jsonName)
{
    std::ifstream f(jsonName);
    json sceneData= json::parse(f);
    const auto& materialsData = sceneData["Materials"];
    std::unordered_map<std::string, uint32_t> MatNameToID;
    for (const auto& item : materialsData.items())
    {
        const auto& name = item.key();
        const auto& p = item.value();
        Material newMaterial{};
        // TODO: handle materials loading differently
        if (p["TYPE"] == "Diffuse")
        {
            const auto& col = p["ALBEDO"];
            newMaterial.color = glm::vec3(col[0], col[1], col[2]);
            newMaterial.hasReflective = 0.0f;
            newMaterial.type = DIFFUSE;
        }
        else if (p["TYPE"] == "Emitting")
        {
            const auto& col = p["ALBEDO"];
            newMaterial.color = glm::vec3(col[0], col[1], col[2]);
            newMaterial.emittance = p["EMITTANCE"];
        }
        else if (p["TYPE"] == "Specular")
        {
            const auto& col = p["ALBEDO"];
            newMaterial.color = glm::vec3(col[0], col[1], col[2]);
            newMaterial.hasReflective = 1.0f;
            newMaterial.type = SPECULAR;
        }
        else if (p["TYPE"] == "MetallicWorkflow") {
            const auto& col = p["ALBEDO"];
            newMaterial.color = glm::vec3(col[0], col[1], col[2]);
            newMaterial.metallic = p["METALLIC"];
            newMaterial.roughness = p["ROUGHNESS"];
            newMaterial.type = METALLICWORKFLOW;
        }
        MatNameToID[name] = materials.size();
        materials.emplace_back(newMaterial);
    }

    std::vector<json> models = {};

    const auto& objectsData = sceneData["Objects"];
    for (const auto& p : objectsData)
    {
        const auto& type = p["TYPE"];

        if (type == "mesh") {
            models.push_back(p);
            continue;
        }

        Geom newGeom;
        if (type == "cube")
        {
            newGeom.type = CUBE;
        }
        else
        {
            newGeom.type = SPHERE;
        }

        newGeom.materialid = MatNameToID[p["MATERIAL"]];
        const auto& trans = p["TRANS"];
        const auto& rotat = p["ROTAT"];
        const auto& scale = p["SCALE"];
        newGeom.translation = glm::vec3(trans[0], trans[1], trans[2]);
        newGeom.rotation = glm::vec3(rotat[0], rotat[1], rotat[2]);
        newGeom.scale = glm::vec3(scale[0], scale[1], scale[2]);
        newGeom.transform = utilityCore::buildTransformationMatrix(
            newGeom.translation, newGeom.rotation, newGeom.scale);
        newGeom.inverseTransform = glm::inverse(newGeom.transform);
        newGeom.invTranspose = glm::inverseTranspose(newGeom.transform);

        geoms.push_back(newGeom);
        // if geom is a light
        if (materials[newGeom.materialid].emittance > 0.0f) {
            lights.push_back(newGeom);
        }
    }

    for (const auto& model : models) {
        // fill with triangle data 
        gltfLoad(model, MatNameToID);
    }

    const auto& cameraData = sceneData["Camera"];
    Camera& camera = state.camera;
    RenderState& state = this->state;
    camera.resolution.x = cameraData["RES"][0];
    camera.resolution.y = cameraData["RES"][1];
    float fovy = cameraData["FOVY"];
    state.iterations = cameraData["ITERATIONS"];
    state.traceDepth = cameraData["DEPTH"];
    state.imageName = cameraData["FILE"];
    const auto& pos = cameraData["EYE"];
    const auto& lookat = cameraData["LOOKAT"];
    const auto& up = cameraData["UP"];
    camera.position = glm::vec3(pos[0], pos[1], pos[2]);
    camera.lookAt = glm::vec3(lookat[0], lookat[1], lookat[2]);
    camera.up = glm::vec3(up[0], up[1], up[2]);

    //calculate fov based on resolution
    float yscaled = tan(fovy * (PI / 180));
    float xscaled = (yscaled * camera.resolution.x) / camera.resolution.y;
    float fovx = (atan(xscaled) * 180) / PI;
    camera.fov = glm::vec2(fovx, fovy);

    camera.right = glm::normalize(glm::cross(camera.view, camera.up));
    camera.pixelLength = glm::vec2(2 * xscaled / (float)camera.resolution.x,
        2 * yscaled / (float)camera.resolution.y);

    camera.view = glm::normalize(camera.lookAt - camera.position);

    //set up render camera stuff
    int arraylen = camera.resolution.x * camera.resolution.y;
    state.image.resize(arraylen);
    std::fill(state.image.begin(), state.image.end(), glm::vec3());
}

void Scene::gltfLoad(const json& modelData, std::unordered_map<std::string, uint32_t> MatNameToID) {
    // first load transforms 
    const auto& trans = modelData["TRANS"];
    const auto& rot = modelData["ROTAT"];
    const auto& scl = modelData["SCALE"];

    glm::vec3 translate = glm::vec3(trans[0], trans[1], trans[2]);
    glm::vec3 rotate = glm::vec3(rot[0], rot[1], rot[2]);
    glm::vec3 scale = glm::vec3(scl[0], scl[1], scl[2]);
    glm::mat4 transform = utilityCore::buildTransformationMatrix(
        translate, rotate, scale);
    glm::mat4 inverse = glm::inverse(transform);
    glm::mat4 inverseTranspose = glm::inverseTranspose(transform);

    // tiny gltf
    tg3_parse_options opts; // configuration
    tg3_error_stack errors; // stores errors/warnings encountered during parsing
    tg3_model model; // parsed mode

    // fill with default settings
    tg3_parse_options_init(&opts);

    // initialize error stack
    tg3_error_stack_init(&errors);

    // parse file
    std::string jsonpath = modelData["FILEPATH"];
    //std::string jsonpath = "C:\\Users\\grttt\\source\\repos\\Project3-CUDA-Path-Tracer-CIS565\\models\\eagle.gltf";
    const char* filepath = jsonpath.c_str();
    uint32_t filelen = jsonpath.size();

    tg3_error_code err = tg3_parse_file(&model, &errors, filepath, filelen, &opts);
    if (err != TG3_OK) {
        // print errors
        for (uint32_t i = 0; i < errors.count; i++) {
            fprintf(stderr, "[%d] %s\n", (int)errors.entries[i].severity,
                errors.entries[i].message ? errors.entries[i].message : "(null)");
        }
    }

    // load model data

    for (int i = 0; i < model.nodes_count; ++i) { // for each node

        const tg3_node& node = model.nodes[i];

        // each node may refer to a mesh or camera; we care about geometry only
        int32_t mesh_i = node.mesh;
        if (node.mesh == -1) {
            continue;
        }


        const tg3_mesh& mesh = model.meshes[mesh_i];

        for (int j = 0; j < mesh.primitives_count; ++j) { // for each primitive
            const tg3_primitive& prim = mesh.primitives[j];

            if (prim.indices == -1) continue;

            // get index info
            const tg3_accessor& indexAccessor = model.accessors[prim.indices];
            const tg3_buffer_view& indexViewBuf = model.buffer_views[indexAccessor.buffer_view];
            const tg3_buffer& indexBuf = model.buffers[indexViewBuf.buffer];

            uint64_t numIndices = indexAccessor.count;

            std::vector<uint32_t> indexData(numIndices);
            switch (indexAccessor.component_type) {
                case TG3_COMPONENT_TYPE_UNSIGNED_BYTE: {
                    const uint8_t* buf = reinterpret_cast<const uint8_t*>(indexBuf.data.data + indexViewBuf.byte_offset + indexAccessor.byte_offset);
                    for (int i = 0; i < numIndices; ++i) indexData[i] = buf[i];
                    break;
                }
                case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
                    const uint16_t* buf = reinterpret_cast<const uint16_t*>(indexBuf.data.data + indexViewBuf.byte_offset + indexAccessor.byte_offset);
                    for (int i = 0; i < numIndices; ++i) indexData[i] = buf[i];
                    break;
                }
                case TG3_COMPONENT_TYPE_UNSIGNED_INT: {
                    const uint32_t* buf = reinterpret_cast<const uint32_t*>(indexBuf.data.data + indexViewBuf.byte_offset + indexAccessor.byte_offset);
                    for (int i = 0; i < numIndices; ++i) indexData[i] = buf[i];
                    break;
                }
                default:
                    throw std::runtime_error("Unsupported index type");
            }
          
            // gather vertex attributes
            for (int k = 0; k < prim.attributes_count; ++k) {
                const tg3_str_int_pair& attr = prim.attributes[k];

                std::string attrib_name(attr.key.data, attr.key.len);
                int attr_i = attr.value;

                if (attrib_name == "POSITION") { // process position buffer

                    // get position info
                    const tg3_accessor& posAccessor = model.accessors[attr_i];
                    const tg3_buffer_view& posViewBuf = model.buffer_views[posAccessor.buffer_view];
                    const tg3_buffer& posBuf = model.buffers[posViewBuf.buffer];
                    // position coodinates are always floats 
                    const glm::vec3* positionData = reinterpret_cast<const glm::vec3*>(posBuf.data.data + posViewBuf.byte_offset + posAccessor.byte_offset);

                    // want to store triangle info that we can iterate through later to test intersections 

                    for (int idx = 0; idx < numIndices; idx += 3) {
                        // 3 indices = 1 triangle

                        Triangle tri;

                        // index data
                        uint32_t i0 = indexData[idx];
                        uint32_t i1 = indexData[idx + 1];
                        uint32_t i2 = indexData[idx + 2];

                        // position data 
                        glm::vec3 pos0 = positionData[i0];
                        glm::vec3 pos1 = positionData[i1];
                        glm::vec3 pos2 = positionData[i2];

                        tri.positions[0] = pos0;
                        tri.positions[1] = pos1;
                        tri.positions[2] = pos2;

                        // calculate normal
                        glm::vec3 normal = glm::normalize(glm::cross(pos1 - pos0, pos2 - pos0));

                        tri.normal = normal;

                        // calculate centroid (in world space for BVH)
                        glm::vec3 centroid = (pos0 + pos1 + pos2) * 0.3333f;
                        tri.centroid = glm::vec3(transform * glm::vec4(centroid, 1.0f));

                        // material id
                        tri.materialid = MatNameToID[modelData["MATERIAL"]];

                        // transforms
                        tri.transform = transform;
                        tri.inverseTransform = inverse;
                        tri.invTranspose = inverseTranspose;

                        triangles.push_back(tri);
                    }
                }
                else { // don't care about other buffers atm
                    continue;
                }
            }
        }
    }
    
    // free resources
    tg3_model_free(&model);
    tg3_error_stack_free(&errors);
}