#pragma once
#include "ShaderGraph.h"
#include <string>

namespace Engine::Renderer {

class ShaderGraphCompiler {
public:
    ShaderGraphCompiler();
    ~ShaderGraphCompiler() = default;

    // Compiles the graph into a bgfx shader program (handle).
    // Returns true if successful, false otherwise.
    bool compileGraph(ShaderGraph* graph, uint16_t& outProgramHandle);

private:
    std::string generateVertexShader();
    std::string generateFragmentShader(ShaderGraph* graph);
    std::string getPinVariableName(int pinId);
    std::string getDefaultValueString(PinType type);

    // Runtime shaderc invocation
    bool runShaderc(const std::string& shaderCode, const std::string& type, const std::string& outputPath);
    
    // Internal bgfx program load
    uint16_t loadShaderProgram(const std::string& vsPath, const std::string& fsPath);
};

} // namespace Engine::Renderer
