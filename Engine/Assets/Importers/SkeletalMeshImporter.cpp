#include "SkeletalMeshImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <meshoptimizer.h>
#include <iostream>
#include <map>

namespace Engine {
namespace Assets {

static Math::Matrix4 toMatrix4(const aiMatrix4x4& from) {
    Math::Matrix4 to = Math::Matrix4::identity();
    to.m[0] = from.a1; to.m[1] = from.b1; to.m[2] = from.c1; to.m[3] = from.d1;
    to.m[4] = from.a2; to.m[5] = from.b2; to.m[6] = from.c2; to.m[7] = from.d2;
    to.m[8] = from.a3; to.m[9] = from.b3; to.m[10] = from.c3; to.m[11] = from.d3;
    to.m[12] = from.a4; to.m[13] = from.b4; to.m[14] = from.c4; to.m[15] = from.d4;
    return to;
}

static void processNodeHierarchy(const aiNode* node, const aiScene* scene, Animation::Skeleton& skeleton, int parentIndex, std::map<std::string, int>& boneMapping) {
    int currentIndex = parentIndex;
    
    if (boneMapping.find(node->mName.C_Str()) != boneMapping.end()) {
        currentIndex = boneMapping[node->mName.C_Str()];
        skeleton.bones[currentIndex].parentIndex = parentIndex;
        skeleton.bones[currentIndex].bindPoseLocalTransform = toMatrix4(node->mTransformation);
    }
    
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNodeHierarchy(node->mChildren[i], scene, skeleton, currentIndex, boneMapping);
    }
}

ImportedSkeletalMesh SkeletalMeshImporter::importFBX(const std::string& path) {
    ImportedSkeletalMesh result;
    std::vector<uint32_t> baseIndices;
    
    Assimp::Importer importer;
    // Limit bone weights to 4 per vertex (aiProcess_LimitBoneWeights)
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_LimitBoneWeights | aiProcess_GenNormals | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Assimp error: " << importer.GetErrorString() << "\n";
        return result;
    }

    std::map<std::string, int> boneMapping;

    // Process meshes
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];
        
        int baseVertex = (int)result.vertices.size();
        
        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            SkinnedVertex vertex = {};
            vertex.x = mesh->mVertices[j].x;
            vertex.y = mesh->mVertices[j].y;
            vertex.z = mesh->mVertices[j].z;
            
            if (mesh->HasNormals()) {
                vertex.nx = mesh->mNormals[j].x;
                vertex.ny = mesh->mNormals[j].y;
                vertex.nz = mesh->mNormals[j].z;
            }
            
            if (mesh->mTextureCoords[0]) {
                vertex.u = mesh->mTextureCoords[0][j].x;
                vertex.v = mesh->mTextureCoords[0][j].y;
            }
            
            result.vertices.push_back(vertex);
        }
        
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                baseIndices.push_back(baseVertex + face.mIndices[k]);
            }
        }
        
        // Bones & Weights
        for (unsigned int j = 0; j < mesh->mNumBones; j++) {
            aiBone* bone = mesh->mBones[j];
            std::string boneName = bone->mName.C_Str();
            
            int boneIndex = 0;
            if (boneMapping.find(boneName) == boneMapping.end()) {
                boneIndex = (int)result.skeleton.bones.size();
                boneMapping[boneName] = boneIndex;
                
                Animation::Bone b;
                b.name = boneName;
                b.inverseBindPoseWorldTransform = toMatrix4(bone->mOffsetMatrix);
                result.skeleton.bones.push_back(b);
            } else {
                boneIndex = boneMapping[boneName];
            }
            
            for (unsigned int k = 0; k < bone->mNumWeights; k++) {
                int vertexId = baseVertex + bone->mWeights[k].mVertexId;
                float weight = bone->mWeights[k].mWeight;
                
                // Assign to first empty slot
                for (int w = 0; w < 4; w++) {
                    if (result.vertices[vertexId].boneWeights[w] == 0.0f) {
                        result.vertices[vertexId].boneIndices[w] = boneIndex;
                        result.vertices[vertexId].boneWeights[w] = weight;
                        break;
                    }
                }
            }
        }
    }
    
    // Process Node Hierarchy to get local transforms and parents
    processNodeHierarchy(scene->mRootNode, scene, result.skeleton, -1, boneMapping);
    
    // Process Animations
    if (scene->HasAnimations()) {
        for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
            aiAnimation* aiAnim = scene->mAnimations[i];
            Animation::AnimationClip clip;
            clip.name = aiAnim->mName.C_Str();
            
            float ticksPerSecond = (float)(aiAnim->mTicksPerSecond != 0 ? aiAnim->mTicksPerSecond : 25.0f);
            clip.duration = (float)aiAnim->mDuration / ticksPerSecond;
            
            for (unsigned int j = 0; j < aiAnim->mNumChannels; j++) {
                aiNodeAnim* channel = aiAnim->mChannels[j];
                std::string boneName = channel->mNodeName.C_Str();
                
                if (boneMapping.find(boneName) == boneMapping.end()) continue;
                
                Animation::BoneKeyframes track;
                track.boneIndex = boneMapping[boneName];
                
                // For MVP, we assume pos/rot/scale have the same number of keys and same times
                for (unsigned int k = 0; k < channel->mNumPositionKeys; k++) {
                    float time = (float)channel->mPositionKeys[k].mTime / ticksPerSecond;
                    track.times.push_back(time);
                    
                    auto pos = channel->mPositionKeys[k].mValue;
                    track.positions.push_back(Math::Vector3(pos.x, pos.y, pos.z));
                    
                    if (k < channel->mNumRotationKeys) {
                        auto rot = channel->mRotationKeys[k].mValue;
                        track.rotations.push_back(Math::Quaternion(rot.x, rot.y, rot.z, rot.w));
                    } else {
                        track.rotations.push_back(Math::Quaternion::identity());
                    }
                    
                    if (k < channel->mNumScalingKeys) {
                        auto scale = channel->mScalingKeys[k].mValue;
                        track.scales.push_back(Math::Vector3(scale.x, scale.y, scale.z));
                    } else {
                        track.scales.push_back(Math::Vector3(1, 1, 1));
                    }
                }
                clip.boneTracks.push_back(track);
            }
            result.clips.push_back(clip);
        }
    }
    
    // Generate LODs using meshoptimizer
    result.lodIndices.push_back(baseIndices); // LOD0 (Original)
    
    float lodRatios[] = {0.5f, 0.15f}; // LOD1 = 50%, LOD2 = 15%
    for (float ratio : lodRatios) {
        size_t targetIndexCount = size_t(baseIndices.size() * ratio);
        std::vector<uint32_t> simplifiedIndices(baseIndices.size());
        
        float lodError = 0.0f;
        size_t newCount = meshopt_simplify(
            simplifiedIndices.data(),
            baseIndices.data(),
            baseIndices.size(),
            &result.vertices[0].x,
            result.vertices.size(),
            sizeof(SkinnedVertex),
            targetIndexCount,
            0.02f, // targetError
            0, // options
            &lodError
        );
        
        simplifiedIndices.resize(newCount);
        // Optimize index buffer for vertex cache
        meshopt_optimizeVertexCache(simplifiedIndices.data(), simplifiedIndices.data(), simplifiedIndices.size(), result.vertices.size());
        result.lodIndices.push_back(simplifiedIndices);
    }

    return result;
}

} // namespace Assets
} // namespace Engine
