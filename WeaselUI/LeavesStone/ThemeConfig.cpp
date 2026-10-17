#include "ThemeConfig.h"

#include <algorithm>
#include <utility>

namespace leaves_stone {

ThemeConfig ValidateThemeConfig(ThemeConfig config) {
  std::vector<int> allowed;
  for (const int count : config.allowed_candidate_counts) {
    if ((count == 5 || count == 7 || count == 9) &&
        std::find(allowed.begin(), allowed.end(), count) == allowed.end()) {
      allowed.push_back(count);
    }
  }
  if (allowed.empty())
    allowed = {5, 7, 9};
  std::sort(allowed.begin(), allowed.end());
  config.allowed_candidate_counts = std::move(allowed);
  if (std::find(config.allowed_candidate_counts.begin(),
                config.allowed_candidate_counts.end(),
                config.default_candidate_count) ==
      config.allowed_candidate_counts.end()) {
    config.default_candidate_count = 5;
  }
  if (config.gap_dip <= 0.0f)
    config.gap_dip = 12.0f;
  if (config.selected_elevation_dip < 0.0f)
    config.selected_elevation_dip = 4.0f;
  // Orientation used to be forced back to Horizontal here, which silently made
  // the vertical layout unreachable and the layout setting a no-op.
  return config;
}

int NormalizeCandidateCount(int requested, const ThemeConfig& raw_config) {
  const auto config = ValidateThemeConfig(raw_config);
  return std::find(config.allowed_candidate_counts.begin(),
                   config.allowed_candidate_counts.end(),
                   requested) != config.allowed_candidate_counts.end()
             ? requested
             : config.default_candidate_count;
}

bool ShouldAnimateAppearance(bool was_visible,
                             bool is_visible,
                             const ThemeConfig& config) {
  return config.animation_enabled && is_visible && !was_visible;
}

float TextScale(const ThemeConfig& config) {
  switch (config.text_size) {
    case TextSize::Small:
      return 0.88f;
    case TextSize::Large:
      return 1.18f;
    default:
      return 1.0f;
  }
}

}  // namespace leaves_stone
