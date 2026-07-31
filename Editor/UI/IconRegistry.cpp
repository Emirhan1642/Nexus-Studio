#include "IconRegistry.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

IconRegistry& IconRegistry::instance() {
    static IconRegistry s_instance;
    return s_instance;
}

void IconRegistry::loadAll(const std::string& iconsDir) {
    if (!fs::exists(iconsDir) || !fs::is_directory(iconsDir)) {
        std::cerr << "IconRegistry: Directory not found -> " << iconsDir << "\n";
        return;
    }

    for (const auto& entry : fs::directory_iterator(iconsDir)) {
        if (entry.path().extension() == ".png") {
            std::string name = entry.path().stem().string();
            std::string fullPath = entry.path().string();

            int w, h, ch;
            unsigned char* pixels = stbi_load(fullPath.c_str(), &w, &h, &ch, 4);
            if (!pixels) {
                std::cerr << "IconRegistry: Failed to load icon -> " << fullPath << "\n";
                continue;
            }

            const bgfx::Memory* mem = bgfx::copy(pixels, w * h * 4);
            stbi_image_free(pixels); // bgfx::copy took a copy, we can free STB's data

            bgfx::TextureHandle handle = bgfx::createTexture2D(
                (uint16_t)w, (uint16_t)h, false, 1,
                bgfx::TextureFormat::RGBA8, 0, mem
            );

            m_icons[name] = handle;
        }
    }
}

ImTextureID IconRegistry::get(const char* name) const {
    auto it = m_icons.find(name);
    if (it == m_icons.end()) {
        return (ImTextureID)(uintptr_t)0;
    }
    return (ImTextureID)(uintptr_t)it->second.idx;
}

void IconRegistry::shutdown() {
    for (auto& pair : m_icons) {
        if (bgfx::isValid(pair.second)) {
            bgfx::destroy(pair.second);
        }
    }
    m_icons.clear();
}
