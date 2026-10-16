#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "WeaselUI/LeavesStone/SceneLayout.h"

namespace {
int failures = 0;

void Check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

bool NearlyEqual(float left, float right, float epsilon = 0.01f) {
  return std::fabs(left - right) <= epsilon;
}

bool Overlaps(const leaves_stone::RectF& left,
              const leaves_stone::RectF& right) {
  return left.left < right.right && left.right > right.left &&
         left.top < right.bottom && left.bottom > right.top;
}

leaves_stone::SceneInput MakeInput(size_t count,
                                   float dpi_scale = 1.0f) {
  leaves_stone::SceneInput input;
  input.anchor = {480.0f, 420.0f, 482.0f, 446.0f};
  input.work_area = {0.0f, 0.0f, 1920.0f, 1080.0f};
  input.dpi_scale = dpi_scale;
  input.requested_candidate_count = static_cast<int>(count);
  input.preedit = L"jintian";
  for (size_t i = 0; i < count; ++i)
    input.candidates.push_back({L"��ѡ" + std::to_wstring(i + 1), L""});
  return input;
}

void CheckInsideWorkArea(const leaves_stone::CandidateScene& scene,
                         const leaves_stone::RectF& work_area) {
  Check(scene.window_bounds.left >= work_area.left,
        "scene must clamp to the left edge");
  Check(scene.window_bounds.top >= work_area.top,
        "scene must clamp to the top edge");
  Check(scene.window_bounds.right <= work_area.right,
        "scene must clamp to the right edge");
  Check(scene.window_bounds.bottom <= work_area.bottom,
        "scene must clamp to the bottom edge");
}

void TestCountsAndDpi() {
  for (const size_t count : {0u, 1u, 5u, 7u, 9u}) {
    for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
      auto input = MakeInput(count, scale);
      const auto scene = leaves_stone::BuildCandidateScene(input);
      Check(scene.candidates.size() == count,
            "scene must preserve candidate count");
      Check(!scene.has_visible_shared_background,
            "scene must not contain a shared visible background");
      CheckInsideWorkArea(scene, input.work_area);
      if (!scene.candidates.empty()) {
        Check(NearlyEqual(scene.candidates.front().bounds.Height(),
                          64.0f * scale),
              "candidate height must scale with DPI");
      }
    }
  }

  auto overlong = MakeInput(20);
  overlong.requested_candidate_count = 9;
  const auto scene = leaves_stone::BuildCandidateScene(overlong);
  Check(scene.candidates.size() == 9,
        "overlong pages must be capped at the normalized page size");
}

void TestPlacementAndElevation() {
  auto lower_edge = MakeInput(5, 1.5f);
  lower_edge.anchor = {1600.0f, 1030.0f, 1602.0f, 1050.0f};
  const auto flipped = leaves_stone::BuildCandidateScene(lower_edge);
  Check(flipped.placed_above, "scene must flip above a lower-edge caret");
  Check(flipped.window_bounds.bottom <= lower_edge.anchor.top,
        "flipped scene must remain above the caret");

  auto left = MakeInput(5);
  left.anchor = {-200.0f, 300.0f, -198.0f, 326.0f};
  CheckInsideWorkArea(leaves_stone::BuildCandidateScene(left), left.work_area);

  auto right = MakeInput(9);
  right.anchor = {1900.0f, 300.0f, 1902.0f, 326.0f};
  CheckInsideWorkArea(leaves_stone::BuildCandidateScene(right), right.work_area);

  auto selected = MakeInput(5, 1.25f);
  selected.selected_index = 2;
  const auto scene = leaves_stone::BuildCandidateScene(selected);
  Check(NearlyEqual(scene.candidates[2].elevation, 4.0f * 1.25f),
        "selected elevation must be four logical pixels");
  Check(NearlyEqual(scene.candidates[2].bounds.top,
                    scene.candidates[1].bounds.top - 4.0f * 1.25f),
        "selected card must visibly rise above adjacent cards");
}

void TestHitRegionsAndTruncation() {
  auto input = MakeInput(5);
  input.candidates[0].text = L"����һ���ǳ��ǳ����ĺ�ѡ�ı�";
  const auto scene = leaves_stone::BuildCandidateScene(input);
  Check(scene.candidates[0].full_text == input.candidates[0].text,
        "full candidate text must survive visual truncation");
  Check(scene.candidates[0].display_text.size() <
            scene.candidates[0].full_text.size(),
        "overlong candidate text must be visually truncated");

  for (size_t i = 0; i < scene.candidates.size(); ++i) {
    for (size_t j = i + 1; j < scene.candidates.size(); ++j) {
      Check(!Overlaps(scene.candidates[i].bounds, scene.candidates[j].bounds),
            "candidate hit regions must not overlap");
    }
  }

  const auto& first = scene.candidates[0].bounds;
  const leaves_stone::PointF center{(first.left + first.right) / 2.0f,
                                    (first.top + first.bottom) / 2.0f};
  const auto candidate_hit = leaves_stone::HitTest(scene, center);
  Check(candidate_hit &&
            candidate_hit->kind == leaves_stone::HitKind::Candidate &&
            candidate_hit->candidate_index == 0,
        "candidate center must hit its semantic candidate id");

  const auto& second = scene.candidates[1].bounds;
  const leaves_stone::PointF gap{(first.right + second.left) / 2.0f,
                                 (first.top + first.bottom) / 2.0f};
  Check(!leaves_stone::HitTest(scene, gap),
        "transparent gaps must not return a hit target");

  if (!scene.decorations.empty()) {
    const auto& decoration = scene.decorations.front().bounds;
    const leaves_stone::PointF decoration_center{
        (decoration.left + decoration.right) / 2.0f,
        (decoration.top + decoration.bottom) / 2.0f};
    Check(!leaves_stone::HitTest(scene, decoration_center),
          "decorations must remain click-through");
  }
}
}  // namespace

int RunSceneLayoutTests() {
  TestCountsAndDpi();
  TestPlacementAndElevation();
  TestHitRegionsAndTruncation();
  return failures;
}
