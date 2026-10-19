#pragma once

#include <vector>

namespace leaves_stone {

enum class Orientation { Horizontal, Vertical };
enum class TextSize { Small, Medium, Large };

struct ThemeConfig {
  Orientation orientation = Orientation::Horizontal;
  TextSize text_size = TextSize::Medium;
  bool animation_enabled = true;
  bool decoration_enabled = true;
  int default_candidate_count = 5;
  std::vector<int> allowed_candidate_counts = {5, 7, 9};
  float gap_dip = 12.0f;
  float selected_elevation_dip = 4.0f;
};

ThemeConfig ValidateThemeConfig(ThemeConfig config);
int NormalizeCandidateCount(int requested, const ThemeConfig& config);
bool ShouldAnimateAppearance(bool was_visible,
                             bool is_visible,
                             const ThemeConfig& config);
float CandidateAppearanceOpacity(float elapsed_ms);
float TextScale(const ThemeConfig& config);

}  // namespace leaves_stone
