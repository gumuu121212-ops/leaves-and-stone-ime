#include "SceneLayout.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace leaves_stone {
namespace {

float Clamp(float value, float low, float high) {
  if (high < low)
    return low;
  return std::max(low, std::min(value, high));
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
  const float minimum_bar_height = 88.0f * scale;
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

  std::vector<float> item_widths;
  item_widths.reserve(visible_count);
  for (size_t i = 0; i < visible_count; ++i) {
    const float glyphs = static_cast<float>(input.candidates[i].text.size());
    float text_width = glyphs * 16.0f * scale * text_scale;
    if (input.measure_candidate_text) {
      const auto measured =
          input.measure_candidate_text(input.candidates[i].text, 0.0f);
      if (measured.width > 0.0f)
        text_width = measured.width;
    }
    // The candidate text starts after the inline index (24 dip), so the width
    // has to reserve that offset plus a right padding; computing it from the
    // index width alone clipped every one-character candidate to "字...".
    item_widths.push_back(
        std::max(24.0f * scale + text_width + 10.0f * scale,
                 44.0f * scale));
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

  // Grow to fit complete phrases. At the monitor boundary, continue whole
  // candidates on a new row; a single screen-wide phrase can wrap internally.
  // Reserve the paging capsule's space so no candidate overlaps its buttons.
  const float row_right = bar_width - page_right - page_width - item_gap;
  const float maximum_item_width = std::max(44.0f * scale,
                                            row_right - first_left);
  std::vector<RectF> item_bounds;
  item_bounds.reserve(visible_count);
  float cursor = first_left;
  float current_row_top = row_top;
  float current_row_height = capsule_height;
  for (size_t i = 0; i < visible_count; ++i) {
    const float width = std::min(item_widths[i], maximum_item_width);
    if (cursor > first_left && cursor + width > row_right + 0.01f) {
      current_row_top += current_row_height + 8.0f * scale;
      current_row_height = capsule_height;
      cursor = first_left;
    }
    const size_t characters_per_line = static_cast<size_t>(std::max(
        1.0f, std::floor((width - 34.0f * scale) /
                           (16.0f * scale * text_scale) + 0.001f)));
    const size_t line_count = std::max(
        size_t{1}, (input.candidates[i].text.size() + characters_per_line - 1) /
                       characters_per_line);
    float height = capsule_height * static_cast<float>(line_count);
    if (input.measure_candidate_text) {
      const auto measured = input.measure_candidate_text(
          input.candidates[i].text, width - 26.0f * scale);
      if (measured.height > 0.0f)
        height = std::max(capsule_height, measured.height + 8.0f * scale);
    }
    item_bounds.push_back({cursor, current_row_top, cursor + width,
                           current_row_top + height});
    current_row_height = std::max(current_row_height, height);
    cursor += width + item_gap;
  }
  const float bar_height = std::max(
      minimum_bar_height,
      current_row_top + current_row_height + 13.5f * scale);

  const float anchor_x = bar_width * 0.605f;
  const float mascot_left = anchor_x - content_offset_x;
  const float mascot_right = mascot_left + content_width * mascot_scale;
  const float window_left = std::min(0.0f, mascot_left);
  const float window_right = std::max(bar_width, mascot_right);
  const float window_width = window_right - window_left;
  const float window_height = overhang + bar_height;

  // Anchor the visible bar, not the top of the mascot's window, to the caret.
  // The mascot overhang extends upward; adding it below the caret leaves a
  // large empty gap between inline preedit and the actual candidates.
  const float caret_gap = 4.0f * scale;
  // Move the entire scene left as one unit; keep the mascot and all hit
  // regions aligned, and let the work-area clamp limit the shift at an edge.
  const float caret_left_offset = 64.0f * scale;
  float left = input.anchor.left - caret_left_offset + window_left;
  float top = input.anchor.bottom + caret_gap - overhang;
  if (left + window_width > input.work_area.right)
    left = input.work_area.right - window_width;
  if (top + window_height > input.work_area.bottom) {
    top = input.anchor.top - caret_gap - window_height;
    scene.placed_above = true;
  }
  left = Clamp(left, input.work_area.left, input.work_area.right - window_width);
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
  for (size_t i = 0; i < visible_count; ++i) {
    const bool selected = i == input.selected_index;
    CandidateCard card;
    card.candidate_index = i;
    const auto& bounds = item_bounds[i];
    card.bounds = {bar_left + bounds.left, bar_top + bounds.top,
                   bar_left + bounds.right, bar_top + bounds.bottom};
    card.full_text = input.candidates[i].text;
    card.display_text = input.candidates[i].text;
    card.comment = input.candidates[i].comment;
    card.selected = selected;
    card.elevation = selected ? theme.selected_elevation_dip * scale : 0.0f;
    scene.candidates.push_back(std::move(card));
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
