#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace Engine::Assets {

struct AssetGuid {
    uint64_t high = 0;
    uint64_t low = 0;

    bool operator==(const AssetGuid& other) const { return high == other.high && low == other.low; }
    bool isValid() const { return high != 0 || low != 0; }
    
    std::string toString() const;
    static AssetGuid fromString(const std::string& str);
};

} // namespace Engine::Assets

namespace std {
    template<> struct hash<Engine::Assets::AssetGuid> {
        std::size_t operator()(const Engine::Assets::AssetGuid& guid) const {
            return std::hash<uint64_t>()(guid.high) ^ (std::hash<uint64_t>()(guid.low) << 1);
        }
    };
}

namespace Engine::Assets {

struct AssetMetadata {
    AssetGuid guid;
    std::string relativePath;
    std::string importerType;
    uint64_t sourceFileHash = 0;
    std::string importSettings; // JSON string
};

class AssetDatabase {
public:
    static AssetDatabase& instance() { static AssetDatabase db; return db; }

    void initialize(const std::string& projectRoot);

    AssetGuid getOrCreateGuid(const std::string& relativePath);
    const AssetMetadata* find(AssetGuid guid) const;
    AssetMetadata* findMutable(AssetGuid guid);
    
    std::string getRelativePath(const std::string& absolutePath) const;
    std::string getAbsolutePath(const std::string& relativePath) const;
    std::string getProjectRoot() const { return m_projectRoot; }

    void updateMetadata(AssetGuid guid, const AssetMetadata& meta);
    
    std::vector<AssetGuid> getAllAssets() const;

private:
    AssetGuid generateGuid();
    void loadMetaFile(const std::string& metaFilePath);
    void saveMetaFile(const AssetMetadata& meta) const;
    uint64_t computeFileHash(const std::string& absolutePath) const;
    void scanDirectory(const std::string& dirPath);

    std::string m_projectRoot;
    std::unordered_map<std::string, AssetGuid> m_pathToGuid;
    std::unordered_map<AssetGuid, AssetMetadata> m_metadata;
};

} // namespace Engine::Assets
