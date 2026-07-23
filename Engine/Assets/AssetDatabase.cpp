#include "AssetDatabase.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <random>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Engine::Assets {

std::string AssetGuid::toString() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << high << std::setw(16) << low;
    return ss.str();
}

AssetGuid AssetGuid::fromString(const std::string& str) {
    AssetGuid guid;
    if (str.length() == 32) {
        guid.high = std::stoull(str.substr(0, 16), nullptr, 16);
        guid.low = std::stoull(str.substr(16, 16), nullptr, 16);
    }
    return guid;
}

void AssetDatabase::initialize(const std::string& projectRoot) {
    m_projectRoot = projectRoot;
    std::string assetsDir = m_projectRoot + "/Assets";
    
    if (!fs::exists(assetsDir)) {
        fs::create_directories(assetsDir);
    }
    
    // Scan existing .meta files
    scanDirectory(assetsDir);
}

void AssetDatabase::scanDirectory(const std::string& dirPath) {
    if (!fs::exists(dirPath)) return;

    for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".meta") {
            loadMetaFile(entry.path().string());
        }
    }
    
    // Also check for files that lack .meta
    for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() != ".meta") {
            std::string relative = getRelativePath(entry.path().string());
            if (m_pathToGuid.find(relative) == m_pathToGuid.end()) {
                getOrCreateGuid(relative);
            }
        }
    }
}

AssetGuid AssetDatabase::getOrCreateGuid(const std::string& relativePath) {
    auto it = m_pathToGuid.find(relativePath);
    if (it != m_pathToGuid.end()) {
        return it->second;
    }

    AssetGuid newGuid = generateGuid();
    m_pathToGuid[relativePath] = newGuid;
    
    AssetMetadata meta;
    meta.guid = newGuid;
    meta.relativePath = relativePath;
    
    std::string ext = fs::path(relativePath).extension().string();
    if (ext == ".fbx" || ext == ".obj") {
        meta.importerType = "Mesh";
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
        meta.importerType = "Texture";
    } else {
        meta.importerType = "Unknown";
    }
    
    meta.sourceFileHash = computeFileHash(getAbsolutePath(relativePath));
    meta.importSettings = "{}";
    
    m_metadata[newGuid] = meta;
    saveMetaFile(meta);
    
    return newGuid;
}

const AssetMetadata* AssetDatabase::find(AssetGuid guid) const {
    auto it = m_metadata.find(guid);
    return it != m_metadata.end() ? &it->second : nullptr;
}

AssetMetadata* AssetDatabase::findMutable(AssetGuid guid) {
    auto it = m_metadata.find(guid);
    return it != m_metadata.end() ? &it->second : nullptr;
}

std::string AssetDatabase::getRelativePath(const std::string& absolutePath) const {
    fs::path root(m_projectRoot);
    fs::path absPath(absolutePath);
    return fs::relative(absPath, root).string();
}

std::string AssetDatabase::getAbsolutePath(const std::string& relativePath) const {
    fs::path root(m_projectRoot);
    fs::path rel(relativePath);
    return (root / rel).string();
}

void AssetDatabase::updateMetadata(AssetGuid guid, const AssetMetadata& meta) {
    m_metadata[guid] = meta;
    m_pathToGuid[meta.relativePath] = guid;
    saveMetaFile(meta);
}

std::vector<AssetGuid> AssetDatabase::getAllAssets() const {
    std::vector<AssetGuid> guids;
    for (const auto& pair : m_metadata) {
        guids.push_back(pair.first);
    }
    return guids;
}

AssetGuid AssetDatabase::generateGuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    AssetGuid guid;
    guid.high = dis(gen);
    guid.low = dis(gen);
    return guid;
}

void AssetDatabase::loadMetaFile(const std::string& metaFilePath) {
    std::ifstream file(metaFilePath);
    if (!file.is_open()) return;

    try {
        json j;
        file >> j;
        
        AssetMetadata meta;
        meta.guid = AssetGuid::fromString(j.value("guid", ""));
        if (!meta.guid.isValid()) return;
        
        std::string absPath = metaFilePath.substr(0, metaFilePath.length() - 5); // remove .meta
        meta.relativePath = getRelativePath(absPath);
        meta.importerType = j.value("importerType", "");
        meta.sourceFileHash = j.value("sourceFileHash", 0ULL);
        
        if (j.contains("importSettings")) {
            meta.importSettings = j["importSettings"].dump();
        }
        
        m_metadata[meta.guid] = meta;
        m_pathToGuid[meta.relativePath] = meta.guid;
        
    } catch (const std::exception& e) {
        // failed to parse
    }
}

void AssetDatabase::saveMetaFile(const AssetMetadata& meta) const {
    std::string metaPath = getAbsolutePath(meta.relativePath) + ".meta";
    
    json j;
    j["guid"] = meta.guid.toString();
    j["importerType"] = meta.importerType;
    j["sourceFileHash"] = meta.sourceFileHash;
    try {
        j["importSettings"] = json::parse(meta.importSettings.empty() ? "{}" : meta.importSettings);
    } catch (...) {
        j["importSettings"] = json::object();
    }

    std::ofstream file(metaPath);
    if (file.is_open()) {
        file << j.dump(4);
    }
}

uint64_t AssetDatabase::computeFileHash(const std::string& absolutePath) const {
    if (!fs::exists(absolutePath)) return 0;
    auto ftime = fs::last_write_time(absolutePath);
    return ftime.time_since_epoch().count();
}

} // namespace Engine::Assets
