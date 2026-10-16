#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "ThemeConfig.h"

namespace leaves_stone {

struct PointF {
  float x = 0.0f;
  float y = 0.0f;
};

struct RectF {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  float Width() const { return right - left; }
  float Height() const { return bottom - top; }
  bool Contains(PointF point) const {
    return point.x >= left && point.x < right && point.y >= top &&
           point.y < bottom;
  }
};

struct CandidateInput {
  std::wstring text;
  std::wstring comment;
};

enum class ControlKind { PreviousPage, NextPage };
enum class DecorationKind { LeafLeft, LeafRight, ArchitecturalLine };
enum class HitKind { Candidate, PreviousPage, NextPage };

struct PinyinPlaque {
  RectF bounds;
  std::wstring text;
};

struct CandidateCard {
  size_t candidate_index = 0;
  RectF bounds;
  std::wstring full_text;
  std::wstring display_text;
  std::wstring comment;
  bool selected = false;
  float elevation = 0.0f;
};

struct ControlCard {
  ControlKind kind = ControlKind::PreviousPage;
  RectF bounds;
  bool enabled = false;
};

struct DecorationLayer {
  DecorationKind kind = DecorationKind::LeafLeft;
  RectF bounds;
};

struct CandidateScene {
  RectF window_bounds;
  PinyinPlaque pinyin;
  std::vector<CandidateCard> candidates;
  std::vector<ControlCard> controls;
  std::vector<DecorationLayer> decorations;
  float dpi_scale = 1.0f;
  float text_scale = 1.0f;
  bool placed_above = false;
  bool has_visible_shared_background = false;
};

struct SceneInput {
  RectF anchor;
  RectF work_area;
  float dpi_scale = 1.0f;
  std::wstring preedit;
  std::vector<CandidateInput> candidates;
  size_t selected_index = std::numeric_limits<size_t>::max();
  int requested_candidate_count = 5;
  bool has_previous_page = false;
  bool has_next_page = false;
  ThemeConfig theme;
};

struct HitTarget {
  HitKind kind = HitKind::Candidate;
  size_t candidate_index = std::numeric_limits<size_t>::max();
};

}  // namespace leaves_stone
