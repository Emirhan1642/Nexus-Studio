#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <any>

namespace Engine::Renderer {

enum class PinType {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Texture
};

enum class PinKind {
    Input,
    Output
};

struct ShaderPin {
    int id;
    int nodeId;
    std::string name;
    PinType type;
    PinKind kind;
};

enum class NodeType {
    Output,
    TextureSample,
    Color,
    Float,
    Multiply,
    Add,
    Lerp,
    DotProduct,
    Time,
    VertexNormal,
    VertexPosition,
    UV
};

struct ShaderNode {
    int id;
    std::string name;
    NodeType type;
    
    std::vector<ShaderPin> inputs;
    std::vector<ShaderPin> outputs;
    
    // For storing custom values (like float or color)
    std::any value;
};

struct ShaderLink {
    int id;
    int startPinId;
    int endPinId;
};

class ShaderGraph {
public:
    ShaderGraph() = default;

    int nextId = 1;
    std::vector<ShaderNode> nodes;
    std::vector<ShaderLink> links;

    int generateId() { return nextId++; }
    
    ShaderNode* getNode(int id) {
        for (auto& n : nodes) {
            if (n.id == id) return &n;
        }
        return nullptr;
    }
    
    ShaderPin* getPin(int id) {
        for (auto& n : nodes) {
            for (auto& p : n.inputs) if (p.id == id) return &p;
            for (auto& p : n.outputs) if (p.id == id) return &p;
        }
        return nullptr;
    }

    void removeLink(int linkId) {
        for (auto it = links.begin(); it != links.end(); ++it) {
            if (it->id == linkId) {
                links.erase(it);
                return;
            }
        }
    }
};

} // namespace Engine::Renderer
