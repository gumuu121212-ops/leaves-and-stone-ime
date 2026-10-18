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

}  // namespace

CandidateScene BuildCandidateScene(const SceneInput& input) {
  CandidateScene scene;
  const auto theme = ValidateThemeConfig(input.theme);
  const float scale = Clamp(input.dpi_scale, 0.5f, 4.0f);
  const float text_scale = TextScale(theme);
  scene.dpi_scale = scale;
  scene.text_scale = text_scale;
  scene.has_visible_shared_background = false;

  const int page_size =
      NormalizeCandidateCount(input.requested_candidate_count, theme);
  const size_t visible_count =
      std::min(input.candidates.size(), static_cast<size_t>(page_size));
  const bool has_plaque = !input.preedit.empty();

  // Everything below is in dip and follows the supplied skin specification.
  const float bar_height = 88.0f * scale;
  const float pinyin_left = 33.0f * scale;
  const float pinyin_top = 10.0f * scale;
  const float row_top = 43.5f * scale;
  const float capsule_height = 31.0f * scale;
  const float first_left = 26.0f * scale;
  const float item_gap = 16.0f * scale;
  const float page_width = 61.0f * scale;
  const float page_height = 28.5f * scale;
  const float page_right = 28.0f * scale;
  const float page_bottom = 14.0f * scale;
  const float paw_size = 24.0f * scale;
  const float paw_right = 20.0f * scale;
  const float paw_top = 14.0f * scale;

  // Mascot: 0.6 dip per source pixel, anchor (512,620) sits on the bar's top
  // edge at 60.5% of its width. The visible content box is (342,450,340,188).
  const float mascot_scale = 0.6f * scale;
  const float mascot_anchor_x = 512.0f;
  const float mascot_baseline = 620.0f;
  const float content_left = 342.0f;
  const float content_top = 450.0f;
  const float content_width = 340.0f;
  const float content_height = 188.0f;
  const float overhang = (mascot_baseline - content_top) * mascot_scale;
  const float content_offset_x = (mascot_anchor_x - content_left) * mascot_scale;

  std::vector<std::wstring> display_texts;
  std::vector<float> item_widths;
  display_texts.reserve(visible_count);
  item_widths.reserve(visible_count);
  for (size_t i = 0; i < visible_count; ++i) {
    display_texts.push_back(TruncateCandidate(input.candidates[i].text));
    const float label = 16.0f * scale;
    const float glyphs = static_cast<float>(display_texts.back().size());
    item_widths.push_back(Clamp(label + glyphs * 16.0f * scale * text_scale +
                                    14.0f * scale,
                                40.0f * scale, 220.0f * scale));
  }

  float items_width = 0.0f;
  for (const float width : item_widths)
    items_width += width;
  if (visible_count > 1)
    items_width += static_cast<float>(visible_count - 1) * item_gap;

  const float work_width = std::max(0.0f, input.work_area.Width());
  float bar_width =
      first_left + items_width + item_gap + page_width + page_right;
  bar_width = std::min(bar_width, std::max(0.0f, work_width - 8.0f * scale));
  bar_width = std::max(bar_width, std::min(320.0f * scale, work_width));

  const float anchor_x = bar_width * 0.605f;
  const float mascot_left = anchor_x - content_offset_x;
  const float mascot_right = mascot_left + content_width * mascot_scale;
  const float window_left = std::min(0.0f, mascot_left);
  const float window_right = std::max(bar_width, mascot_right);
  const float window_width = window_right - window_left;
  const float window_height = overhang + bar_height;

  float left = Clamp(input.anchor.left - window_width * 0.605f,
                     input.work_area.left,
                     input.work_area.right - window_width);
  float top = input.anchor.bottom + 10.0f * scale - overhang;
  if (top + window_height > input.work_area.bottom) {
    // Place the whole window above the caret, not just the bar: the mascot is
    // part of the window and would otherwise cover the line being typed.
    top = input.anchor.top - 10.0f * scale - window_height;
    scene.placed_above = true;
  }
  top = Clamp(top, input.work_area.top,
              input.work_area.bottom - window_height);
  scene.window_bounds = {left, top, left + window_width, top + window_height};

  const float bar_left = left - window_left;
  const float bar_top = top + overhang;
  scene.bar = {bar_left, bar_top, bar_left + bar_width, bar_top + bar_height};
  scene.has_bar = true;
  scene.mascot = {bar_left + mascot_left, bar_top - overhang,
                  bar_left + mascot_left + content_width * mascot_scale,
                  bar_top - overhang + content_height * mascot_scale};

  if (has_plaque) {
    const float available = std::max(0.0f, bar_width - pinyin_left - 20.0f);
    scene.pinyin = {{bar_left + pinyin_left, bar_top + pinyin_top,
                     bar_left + pinyin_left + available,
                     bar_top + pinyin_top + 24.0f * scale},
                    input.preedit};
  }

  scene.candidates.reserve(visible_count);
  float cursor = bar_left + first_left;
  for (size_t i = 0; i < visible_count; ++i) {
    const bool selected = i == input.selected_index;
    CandidateCard card;
    card.candidate_index = i;
    card.bounds = {cursor, bar_top + row_top, cursor + item_widths[i],
                   bar_top + row_top + capsule_height};
    card.full_text = input.candidates[i].text;
    card.display_text = display_texts[i];
    card.comment = input.candidates[i].comment;
    card.selected = selected;
    card.elevation = selected ? theme.selected_elevation_dip * scale : 0.0f;
    scene.candidates.push_back(std::move(card));
    cursor += item_widths[i] + item_gap;
  }

  const float page_left = bar_left + bar_width - page_right - page_width;
  const float page_top = bar_top + bar_height - page_bottom - page_height;
  const float arrow_width = page_width * 0.5f;
  scene.controls.push_back(
      {ControlKind::PreviousPage,
       {page_left, page_top, page_left + arrow_width, page_top + page_height},
       input.has_previous_page});
  scene.controls.push_back({ControlKind::NextPage,
                            {page_left + arrow_width, page_top,
                             page_left + page_width, page_top + page_height},
                            input.has_next_page});
  scene.page_group = {page_left, page_top, page_left + page_width,
                      page_top + page_height};
  scene.paw = {bar_left + bar_width - paw_right - paw_size,
               bar_top + paw_top, bar_left + bar_width - paw_right,
               bar_top + paw_top + paw_size};

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
