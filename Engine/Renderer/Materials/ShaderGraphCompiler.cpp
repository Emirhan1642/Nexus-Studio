#include "ShaderGraphCompiler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstdlib>
#include <bgfx/bgfx.h>

namespace Engine::Renderer {

ShaderGraphCompiler::ShaderGraphCompiler() {
}

bool ShaderGraphCompiler::compileGraph(ShaderGraph* graph, uint16_t& outProgramHandle) {
    if (!graph) return false;

    std::string vsCode = generateVertexShader();
    std::string fsCode = generateFragmentShader(graph);

    // Save temporary .sc files
    std::string vsPath = "temp_vs.sc";
    std::string fsPath = "temp_fs.sc";
    std::string vsOutPath = "temp_vs.bin";
    std::string fsOutPath = "temp_fs.bin";

    std::ofstream vsFile(vsPath);
    vsFile << vsCode;
    vsFile.close();

    std::ofstream fsFile(fsPath);
    fsFile << fsCode;
    fsFile.close();

    // Run shaderc (assuming it is in PATH or current dir, we'll try to use bin/Debug/shaderc.exe)
    bool vsOk = runShaderc(vsPath, "vertex", vsOutPath);
    bool fsOk = runShaderc(fsPath, "fragment", fsOutPath);

    if (vsOk && fsOk) {
        outProgramHandle = loadShaderProgram(vsOutPath, fsOutPath);
        return bgfx::isValid(bgfx::ProgramHandle{outProgramHandle});
    }

    return false;
}

std::string ShaderGraphCompiler::generateVertexShader() {
    std::stringstream ss;
    ss << "$input a_position, a_normal, a_texcoord0\n";
    ss << "$output v_normal, v_texcoord0\n";
    ss << "#include <bgfx_shader.sh>\n";
    ss << "void main() {\n";
    ss << "    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));\n";
    ss << "    v_normal = mul(u_modelView, vec4(a_normal, 0.0)).xyz;\n";
    ss << "    v_texcoord0 = a_texcoord0;\n";
    ss << "}\n";
    return ss.str();
}

std::string ShaderGraphCompiler::generateFragmentShader(ShaderGraph* graph) {
    std::stringstream ss;
    ss << "$input v_normal, v_texcoord0\n";
    ss << "#include <bgfx_shader.sh>\n";
    
    // Find Output node
    ShaderNode* outputNode = nullptr;
    for (auto& node : graph->nodes) {
        if (node.type == NodeType::Output) {
            outputNode = &node;
            break;
        }
    }

    if (!outputNode) {
        ss << "void main() { gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0); }\n";
        return ss.str();
    }

    // 1. Topological Sort
    std::vector<int> sortedNodes;
    std::unordered_set<int> visited;
    std::unordered_set<int> visiting;
    
    // DFS function
    std::function<void(int)> dfs = [&](int nodeId) {
        if (visited.count(nodeId)) return;
        visiting.insert(nodeId);
        
        ShaderNode* node = graph->getNode(nodeId);
        if (node) {
            for (auto& pin : node->inputs) {
                for (auto& link : graph->links) {
                    if (link.endPinId == pin.id) {
                        ShaderPin* sourcePin = graph->getPin(link.startPinId);
                        if (sourcePin && visiting.count(sourcePin->nodeId) == 0) {
                            dfs(sourcePin->nodeId);
                        }
                    }
                }
            }
        }
        
        visiting.erase(nodeId);
        visited.insert(nodeId);
        sortedNodes.push_back(nodeId);
    };
    
    dfs(outputNode->id);
    
    // 2. Generate Code
    ss << "void main() {\n";
    
    for (int nodeId : sortedNodes) {
        ShaderNode* node = graph->getNode(nodeId);
        if (!node) continue;
        
        // Helper to get input value string
        auto getInputValue = [&](int pinIndex) -> std::string {
            if (pinIndex >= node->inputs.size()) return "0.0";
            ShaderPin& pin = node->inputs[pinIndex];
            
            // Check if connected
            for (auto& link : graph->links) {
                if (link.endPinId == pin.id) {
                    return getPinVariableName(link.startPinId);
                }
            }
            return getDefaultValueString(pin.type);
        };
        
        // Declare outputs
        for (auto& pin : node->outputs) {
            std::string typeStr = "float";
            if (pin.type == PinType::Vec2) typeStr = "vec2";
            else if (pin.type == PinType::Vec3) typeStr = "vec3";
            else if (pin.type == PinType::Vec4) typeStr = "vec4";
            
            ss << "    " << typeStr << " " << getPinVariableName(pin.id) << " = " << getDefaultValueString(pin.type) << ";\n";
        }
        
        // Node Logic
        if (node->type == NodeType::Color) {
            if (!node->outputs.empty()) {
                ss << "    " << getPinVariableName(node->outputs[0].id) << " = vec3(1.0, 0.0, 0.0); // Hardcoded Color\n";
            }
        }
        else if (node->type == NodeType::Multiply) {
            if (!node->outputs.empty()) {
                std::string in1 = getInputValue(0);
                std::string in2 = getInputValue(1);
                ss << "    " << getPinVariableName(node->outputs[0].id) << " = " << in1 << " * " << in2 << ";\n";
            }
        }
        else if (node->type == NodeType::Output) {
            std::string albedo = getInputValue(0);
            ss << "    gl_FragColor = vec4(" << albedo << ", 1.0);\n";
        }
    }
    
    ss << "}\n";
    return ss.str();
}

std::string ShaderGraphCompiler::getPinVariableName(int pinId) {
    return "pin_" + std::to_string(pinId);
}

std::string ShaderGraphCompiler::getDefaultValueString(PinType type) {
    switch (type) {
        case PinType::Float: return "0.0";
        case PinType::Vec2: return "vec2(0.0, 0.0)";
        case PinType::Vec3: return "vec3(0.0, 0.0, 0.0)";
        case PinType::Vec4: return "vec4(0.0, 0.0, 0.0, 0.0)";
        default: return "0.0";
    }
}

bool ShaderGraphCompiler::runShaderc(const std::string& inputPath, const std::string& type, const std::string& outputPath) {
    std::string bgfxSrcPath = "C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/build/_deps/bgfx-src/src"; 
    std::string shadercCmd = "bin\\Debug\\shaderc.exe -f " + inputPath + " -o " + outputPath + " --type " + type + " --platform windows -p ";
    
    if (type == "vertex") {
        shadercCmd += "vs_5_0";
    } else {
        shadercCmd += "ps_5_0";
    }
    
    shadercCmd += " -i \"" + bgfxSrcPath + "\"";
    
    std::cout << "Running: " << shadercCmd << "\n";
    int result = std::system(shadercCmd.c_str());
    return result == 0;
}

uint16_t ShaderGraphCompiler::loadShaderProgram(const std::string& vsPath, const std::string& fsPath) {
    auto loadMem = [](const std::string& path) -> const bgfx::Memory* {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::string fallback = std::string("../../../") + path;
            file.open(fallback, std::ios::binary | std::ios::ate);
        }
        if (!file.is_open()) return nullptr;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        const bgfx::Memory* mem = bgfx::alloc((uint32_t)size + 1);
        if (file.read((char*)mem->data, size)) {
            mem->data[size] = '\0';
            return mem;
        }
        return nullptr;
    };

    const bgfx::Memory* vsMem = loadMem(vsPath);
    const bgfx::Memory* fsMem = loadMem(fsPath);
    
    if (vsMem && fsMem) {
        bgfx::ShaderHandle vsh = bgfx::createShader(vsMem);
        bgfx::ShaderHandle fsh = bgfx::createShader(fsMem);
        bgfx::ProgramHandle program = bgfx::createProgram(vsh, fsh, true);
        return program.idx;
    }
    return bgfx::kInvalidHandle;
}

} // namespace Engine::Renderer
