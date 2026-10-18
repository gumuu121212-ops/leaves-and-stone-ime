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
    input.candidates.push_back({L"候选" + std::to_wstring(i + 1), L""});
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
        // The plush bar uses the specified 31 dip candidate capsule.
        Check(NearlyEqual(scene.candidates.front().bounds.Height(),
                          31.0f * scale),
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
  // The candidate bar sits below the caret; near the bottom of the monitor the
  // whole scene must flip above the input line and remain inside the work area.
  auto lower_edge = MakeInput(5, 1.5f);
  lower_edge.anchor = {1600.0f, 1030.0f, 1602.0f, 1050.0f};
  const auto beside = leaves_stone::BuildCandidateScene(lower_edge);
  Check(beside.window_bounds.top >= lower_edge.work_area.top &&
            beside.window_bounds.bottom <= lower_edge.work_area.bottom,
        "scene must stay inside the work area at a lower-edge caret");
  Check(beside.placed_above &&
            NearlyEqual(beside.bar.bottom,
                        lower_edge.anchor.top - 4.0f * lower_edge.dpi_scale),
        "a lower-edge caret must place the bar just above the input line");

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
  Check(scene.candidates[2].selected && !scene.candidates[1].selected,
        "the selected candidate must be the only one marked");
}

void TestBarTracksCaretWithoutMascotGap() {
  for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
    for (const bool inline_preedit : {false, true}) {
      auto input = MakeInput(5, scale);
      if (inline_preedit)
        input.preedit.clear();
      const auto scene = leaves_stone::BuildCandidateScene(input);
      Check(NearlyEqual(scene.bar.top,
                        input.anchor.bottom + 4.0f * scale),
            "the visible bar must start four dip below the caret, independent "
            "of the mascot's height or inline preedit");
      Check(!scene.placed_above,
            "the bar must stay below the input line when there is room");
      Check(scene.window_bounds.top < input.anchor.bottom,
            "the mascot's window must extend above the bar without adding "
            "distance from the caret");
      Check(NearlyEqual(scene.bar.top - scene.mascot.top, 102.0f * scale),
            "moving the scene must preserve the mascot's paw baseline");
      CheckInsideWorkArea(scene, input.work_area);

      // There is enough room for the bar below this line, even though there
      // would not be room for the mascot if it were added below the caret.
      input.anchor.bottom = input.work_area.bottom - 100.0f * scale;
      input.anchor.top = input.anchor.bottom - 26.0f * scale;
      const auto near_bottom = leaves_stone::BuildCandidateScene(input);
      Check(!near_bottom.placed_above &&
                NearlyEqual(near_bottom.bar.top,
                            input.anchor.bottom + 4.0f * scale),
            "the mascot overhang must not cause an unnecessary flip above");
      CheckInsideWorkArea(near_bottom, input.work_area);
    }
  }

  auto upper_edge = MakeInput(5);
  upper_edge.anchor = {480.0f, 0.0f, 482.0f, 26.0f};
  CheckInsideWorkArea(leaves_stone::BuildCandidateScene(upper_edge),
                      upper_edge.work_area);

  auto secondary_monitor = MakeInput(5);
  secondary_monitor.work_area = {-1920.0f, -1080.0f, 0.0f, 0.0f};
  secondary_monitor.anchor = {-1440.0f, -660.0f, -1438.0f, -634.0f};
  const auto negative_scene =
      leaves_stone::BuildCandidateScene(secondary_monitor);
  Check(NearlyEqual(negative_scene.bar.top,
                    secondary_monitor.anchor.bottom + 4.0f),
        "bar-to-caret spacing must also work on a negative-coordinate monitor");
  CheckInsideWorkArea(negative_scene, secondary_monitor.work_area);
}

void TestHitRegionsAndTruncation() {
  auto input = MakeInput(5);
  input.candidates[0].text = L"这是一个非常非常长的候选文本";
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

void TestUserLayoutPreferences() {
  // The plush skin is a single horizontal bar; every candidate shares one row
  // and the mascot always sits above it.
  const auto bar_scene = leaves_stone::BuildCandidateScene(MakeInput(5));
  Check(bar_scene.has_bar, "the scene must expose the plush candidate bar");
  Check(bar_scene.mascot.top < bar_scene.bar.top &&
            bar_scene.mascot.bottom > bar_scene.bar.top,
        "the mascot must lean over the top edge of the bar");
  for (size_t index = 1; index < bar_scene.candidates.size(); ++index) {
    Check(NearlyEqual(bar_scene.candidates[index].bounds.top,
                      bar_scene.candidates.front().bounds.top),
          "every candidate must sit on the same row");
    Check(bar_scene.candidates[index].bounds.left >
              bar_scene.candidates[index - 1].bounds.left,
          "candidates must be laid out left to right");
  }

  auto small = MakeInput(5);
  small.theme.text_size = leaves_stone::TextSize::Small;
  auto large = small;
  large.theme.text_size = leaves_stone::TextSize::Large;
  Check(leaves_stone::TextScale(large.theme) >
            leaves_stone::TextScale(small.theme),
        "large text preference must enlarge the text scale");

  auto undecorated = MakeInput(5);
  undecorated.theme.decoration_enabled = false;
  Check(leaves_stone::BuildCandidateScene(undecorated).decorations.empty(),
        "disabled decoration preference must remove ornamental layers");
}
}  // namespace

int RunSceneLayoutTests() {
  TestCountsAndDpi();
  TestPlacementAndElevation();
  TestBarTracksCaretWithoutMascotGap();
  TestHitRegionsAndTruncation();
  TestUserLayoutPreferences();
  return failures;
}
