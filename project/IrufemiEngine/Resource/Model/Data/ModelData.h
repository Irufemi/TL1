#pragma once

#include "../../../Engine/Graphics/Data/VertexData.h"
#include "MaterialData.h"
#include "Node.h"
#include "JointWeightData.h"
#include <vector>
#include <map>
#include <string>

struct ModelData {
    std::map<std::string, JointWeightData> skinClusterData;
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    MaterialData material;
    Node rootNode;
};