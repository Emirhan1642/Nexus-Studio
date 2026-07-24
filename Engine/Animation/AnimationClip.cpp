#include "AnimationClip.h"
#include <cmath>
#include <algorithm>

namespace Engine {
namespace Animation {

static std::tuple<int, int, float> findSurroundingKeyframes(const std::vector<float>& times, float time) {
    if (times.empty()) return {0, 0, 0.0f};
    if (time <= times.front()) return {0, 0, 0.0f};
    if (time >= times.back()) return {(int)times.size() - 1, (int)times.size() - 1, 0.0f};

    auto it = std::lower_bound(times.begin(), times.end(), time);
    int nextIdx = (int)std::distance(times.begin(), it);
    int prevIdx = nextIdx - 1;

    float timeRange = times[nextIdx] - times[prevIdx];
    float t = (time - times[prevIdx]) / timeRange;
    return {prevIdx, nextIdx, t};
}

Math::Matrix4 AnimationClip::sampleBone(int boneIndex, float time) const {
    const BoneKeyframes* track = findTrack(boneIndex);
    if (!track) return Math::Matrix4::identity();

    if (track->times.empty()) return Math::Matrix4::identity();

    auto [prevIdx, nextIdx, t] = findSurroundingKeyframes(track->times, time);

    Math::Vector3 pos = (nextIdx == prevIdx) ? track->positions[prevIdx] : 
                        track->positions[prevIdx] + (track->positions[nextIdx] - track->positions[prevIdx]) * t;
                        
    Math::Quaternion rot = (nextIdx == prevIdx) ? track->rotations[prevIdx] : 
                           track->rotations[prevIdx].slerp(track->rotations[nextIdx], t);
                           
    Math::Vector3 scale = (nextIdx == prevIdx) ? track->scales[prevIdx] : 
                          track->scales[prevIdx] + (track->scales[nextIdx] - track->scales[prevIdx]) * t;

    return Math::Matrix4::fromTRS(pos, rot, scale);
}

} // namespace Animation
} // namespace Engine
