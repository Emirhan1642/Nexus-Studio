#pragma once
#include <string>
#include <unordered_map>
#include <bgfx/bgfx.h>
#include <imgui.h>

class IconRegistry {
public:
    static IconRegistry& instance();
    void loadAll(const std::string& iconsDir);
    ImTextureID get(const char* name) const;
    void shutdown();
private:
    std::unordered_map<std::string, bgfx::TextureHandle> m_icons;
};
