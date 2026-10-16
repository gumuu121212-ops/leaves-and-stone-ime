#include "SceneLayout.h"

#include <algorithm>
#include <numeric>
#include <utility>

namespace leaves_stone {
namespace {

float Clamp(float value, float low, float high) {
  if (high < low)
    return low;
  return std::max(low, std::min(value, high));
}

std::wstring TruncateCandidate(const std::wstring& value) {
  constexpr size_t kMaximumCharacters = 6;
  if (value.size() <= kMaximumCharacters)
    return value;
  return value.substr(0, kMaximumCharacters - 1) + L"\u2026";
}

float DesiredCardWidth(const std::wstring& display_text, float scale) {
  const float character_width = 16.0f * scale;
  const float horizontal_padding = 24.0f * scale;
  return Clamp(horizontal_padding +
                   character_width * static_cast<float>(display_text.size()),
               36.0f * scale, 112.0f * scale);
}

}  // namespace

CandidateScene BuildCandidateScene(const SceneInput& input) {
  CandidateScene scene;
  const auto theme = ValidateThemeConfig(input.theme);
  const float scale = Clamp(input.dpi_scale, 0.5f, 4.0f);
  scene.dpi_scale = scale;
  scene.has_visible_shared_background = false;

  const int page_size =
      NormalizeCandidateCount(input.requested_candidate_count, theme);
  const size_t visible_count =
      std::min(input.candidates.size(), static_cast<size_t>(page_size));
  const bool has_plaque = !input.preedit.empty();
  const float gap = theme.gap_dip * scale;
  const float card_height = 64.0f * scale;
  const float plaque_height = 42.0f * scale;
  const float control_size = 36.0f * scale;
  const float vertical_margin = 12.0f * scale;
  const float caret_offset = 10.0f * scale;
  const float elevation = theme.selected_elevation_dip * scale;
  const float plaque_width =
      Clamp((52.0f + static_cast<float>(input.preedit.size()) * 9.0f) * scale,
            84.0f * scale, 136.0f * scale);

  const size_t element_count = visible_count + 2 + (has_plaque ? 1 : 0);
  const float total_gaps =
      element_count > 0 ? static_cast<float>(element_count - 1) * gap : 0.0f;
  const float work_width = std::max(0.0f, input.work_area.Width());
  const float fixed_width = (has_plaque ? plaque_width : 0.0f) +
                            control_size * 2.0f + total_gaps;

  std::vector<std::wstring> display_texts;
  std::vector<float> card_widths;
  display_texts.reserve(visible_count);
  card_widths.reserve(visible_count);
  for (size_t i = 0; i < visible_count; ++i) {
    display_texts.push_back(TruncateCandidate(input.candidates[i].text));
    card_widths.push_back(DesiredCardWidth(display_texts.back(), scale));
  }

  const float desired_card_width =
      std::accumulate(card_widths.begin(), card_widths.end(), 0.0f);
  const float card_budget = std::max(0.0f, work_width - fixed_width);
  if (visible_count > 0 && desired_card_width > card_budget) {
    const float shared_width =
        std::max(36.0f * scale,
                 card_budget / static_cast<float>(visible_count));
    for (float& width : card_widths)
      width = std::min(width, shared_width);
  }

  const float cards_width =
      std::accumulate(card_widths.begin(), card_widths.end(), 0.0f);
  const float scene_width = fixed_width + cards_width;
  const float scene_height = vertical_margin * 2.0f + card_height;

  float left = Clamp(input.anchor.left, input.work_area.left,
                     input.work_area.right - scene_width);
  float top = input.anchor.bottom + caret_offset;
  if (top + scene_height > input.work_area.bottom) {
    top = input.anchor.top - caret_offset - scene_height;
    scene.placed_above = true;
  }
  top = Clamp(top, input.work_area.top, input.work_area.bottom - scene_height);
  scene.window_bounds = {left, top, left + scene_width, top + scene_height};

  const float row_top = top + vertical_margin + elevation;
  float cursor = left;
  if (has_plaque) {
    const float plaque_top = row_top + (card_height - plaque_height) / 2.0f;
    scene.pinyin = {{cursor, plaque_top, cursor + plaque_width,
                     plaque_top + plaque_height},
                    input.preedit};
    cursor += plaque_width + gap;
  }

  scene.candidates.reserve(visible_count);
  for (size_t i = 0; i < visible_count; ++i) {
    const bool selected = i == input.selected_index;
    const float card_top = row_top - (selected ? elevation : 0.0f);
    CandidateCard card;
    card.candidate_index = i;
    card.bounds =
        {cursor, card_top, cursor + card_widths[i], card_top + card_height};
    card.full_text = input.candidates[i].text;
    card.display_text = display_texts[i];
    card.comment = input.candidates[i].comment;
    card.selected = selected;
    card.elevation = selected ? elevation : 0.0f;
    scene.candidates.push_back(std::move(card));
    cursor += card_widths[i] + gap;
  }

  const float control_top = row_top + (card_height - control_size) / 2.0f;
  scene.controls.push_back({ControlKind::PreviousPage,
                            {cursor, control_top, cursor + control_size,
                             control_top + control_size},
                            input.has_previous_page});
  cursor += control_size + gap;
  scene.controls.push_back({ControlKind::NextPage,
                            {cursor, control_top, cursor + control_size,
                             control_top + control_size},
                            input.has_next_page});

  if (theme.decoration_enabled) {
    const float decoration_height = 10.0f * scale;
    const float decoration_width =
        std::min(scene_width / 3.0f, 64.0f * scale);
    scene.decorations.push_back(
        {DecorationKind::LeafLeft,
         {left, top, left + decoration_width, top + decoration_height}});
    scene.decorations.push_back(
        {DecorationKind::LeafRight,
         {left + scene_width - decoration_width, top,
          left + scene_width, top + decoration_height}});
  }

  return scene;
}

std::optional<HitTarget> HitTest(const CandidateScene& scene, PointF point) {
  for (const auto& candidate : scene.candidates) {
    if (candidate.bounds.Contains(point))
      return HitTarget{HitKind::Candidate, candidate.candidate_index};
  }
  for (const auto& control : scene.controls) {
    if (!control.enabled || !control.bounds.Contains(point))
      continue;
    return HitTarget{control.kind == ControlKind::PreviousPage
                         ? HitKind::PreviousPage
                         : HitKind::NextPage,
                     std::numeric_limits<size_t>::max()};
  }
  return std::nullopt;
}

}  // namespace leaves_stone
