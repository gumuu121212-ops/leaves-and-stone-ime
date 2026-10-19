#pragma once

#include <optional>

#include "SceneTypes.h"

namespace leaves_stone {

CandidateScene BuildCandidateScene(const SceneInput& input);
std::optional<HitTarget> HitTest(const CandidateScene& scene, PointF point);

}  // namespace leaves_stone
