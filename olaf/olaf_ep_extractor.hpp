// Olaf: Overly Lightweight Acoustic Fingerprinting
// Copyright (C) 2019-2025  Joren Six

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef OLAF_EP_EXTRACTOR_HPP
#define OLAF_EP_EXTRACTOR_HPP

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "olaf_config.hpp"
#include "olaf_max_filter.hpp"

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace olaf
{

/**
 * @struct EventPoint
 * @brief An event point is a combination of a frequency bin, time bin and magnitude
 */
struct EventPoint
{
  int frequency_bin = 0;
  int time_index = (1 << 23);
  float magnitude = 0.0f;
  int usages = 0;

  void print() const
  {
    std::fprintf(
      stderr, "t:%d, f:%d, u:%d, mag:%.4f\n", time_index, frequency_bin, usages, magnitude);
  }
};

/**
 * @struct ExtractedEventPoints
 * @brief The result of event point extraction
 */
struct ExtractedEventPoints
{
  std::vector<EventPoint> event_points;
  int event_point_index = 0;
};

/**
 * @class EPExtractor
 * @brief Event Point extractor with state information
 */
class EPExtractor
{
private:
  const Config & config_;
  std::vector<std::vector<float>> mags_;
  std::vector<std::vector<float>> maxes_;
  int filter_index_ = 0;
  int audio_block_index_ = 0;
  ExtractedEventPoints event_points_;

  static float max_filter_time(const float * array, std::size_t array_size)
  {
#if defined(__ARM_NEON)
    assert(array_size % 4 == 0);
    float32x4_t vec_max = vld1q_f32(array);
    for (std::size_t j = 4; j < array_size; j += 4) {
      float32x4_t vec = vld1q_f32(array + j);
      vec_max = vmaxq_f32(vec_max, vec);
    }
    float32x2_t max_val = vpmax_f32(vget_low_f32(vec_max), vget_high_f32(vec_max));
    max_val = vpmax_f32(max_val, max_val);
    return vget_lane_f32(max_val, 0);
#else
    float max_val = -10000000.0f;
    for (std::size_t i = 0; i < array_size; ++i) {
      max_val = std::max(max_val, array[i]);
    }
    return max_val;
#endif
  }

  void max_filter_frequency(
    const std::vector<float> & data, std::vector<float> & max_output, int half_filter_size)
  {
    const std::size_t filter_size = half_filter_size * 2 + 1;
    max_filter(data, filter_size, max_output);
  }

  void extract_internal()
  {
    const std::size_t filter_size_time = config_.filterSizeTime;
    const std::size_t half_filter_size_time = config_.halfFilterSizeTime;
    const std::size_t half_audio_block_size = config_.audioBlockSize / 2;
    const int min_frequency_bin = config_.minFrequencyBin;

    int event_point_index = event_points_.event_point_index;

    for (std::size_t j = min_frequency_bin; j < half_audio_block_size - 1; ++j) {
      const float current_val = mags_[half_filter_size_time][j];
      const float max_val = maxes_[half_filter_size_time][j];

      if (current_val < config_.minEventPointMagnitude || current_val != max_val) {
        continue;
      }

      std::vector<float> timeslice(filter_size_time);
      for (std::size_t t = 0; t < filter_size_time; ++t) {
        timeslice[t] = maxes_[t][j];
      }

      const float max_val_time = max_filter_time(timeslice.data(), config_.filterSizeTime);

      if (current_val == max_val_time) {
        const int time_index = audio_block_index_ - half_filter_size_time;
        const int frequency_bin = static_cast<int>(j);
        const float magnitude = mags_[half_filter_size_time][frequency_bin];

        if (event_point_index == config_.maxEventPoints) {
          std::fprintf(
            stderr,
            "Warning: Eventpoint maximum index %d reached, event points are ignored, "
            "consider increasing config.maxEventPoints if you see this often.\n",
            config_.maxEventPoints);
        } else {
          event_points_.event_points[event_point_index].time_index = time_index;
          event_points_.event_points[event_point_index].frequency_bin = frequency_bin;
          event_points_.event_points[event_point_index].magnitude = magnitude;
          event_points_.event_points[event_point_index].usages = 0;

          ++event_point_index;
        }

        assert(event_point_index <= config_.maxEventPoints);
      }
    }

    event_points_.event_point_index = event_point_index;
  }

  void rotate()
  {
    std::vector<float> temp_max = std::move(maxes_[0]);
    std::vector<float> temp_mag = std::move(mags_[0]);

    for (int i = 1; i < config_.filterSizeTime; ++i) {
      maxes_[i - 1] = std::move(maxes_[i]);
      mags_[i - 1] = std::move(mags_[i]);
    }

    assert(filter_index_ == config_.filterSizeTime - 1);

    maxes_[config_.filterSizeTime - 1] = std::move(temp_max);
    mags_[config_.filterSizeTime - 1] = std::move(temp_mag);
  }

public:
  explicit EPExtractor(const Config & config) : config_(config)
  {
    const int half_audio_block_size = config_.audioBlockSize / 2;

    event_points_.event_points.resize(config_.maxEventPoints);
    event_points_.event_point_index = 0;

    for (auto & ep : event_points_.event_points) {
      ep.time_index = (1 << 23);
    }

    mags_.resize(config_.filterSizeTime, std::vector<float>(half_audio_block_size, 0.0f));
    maxes_.resize(config_.filterSizeTime, std::vector<float>(half_audio_block_size, 0.0f));

    filter_index_ = 0;
  }

  const std::vector<float> & get_mags() const
  {
    if (filter_index_ == config_.filterSizeTime - 1) {
      return mags_[config_.filterSizeTime - 2];
    } else {
      return mags_[filter_index_ - 1];
    }
  }

  void extract(const float * fft_out, int audio_block_index)
  {
    audio_block_index_ = audio_block_index;

    int magnitude_index = 0;
    for (int j = 0; j < config_.audioBlockSize; j += 2) {
      mags_.at(filter_index_).at(magnitude_index) = std::hypot(fft_out[j], fft_out[j + 1]);
      if (config_.sqrtMagnitude) {
        mags_.at(filter_index_).at(magnitude_index) =
          std::sqrt(mags_.at(filter_index_).at(magnitude_index));
      }
      ++magnitude_index;
    }

    max_filter_frequency(
      mags_.at(filter_index_), maxes_.at(filter_index_), config_.halfFilterSizeFrequency);

    if (filter_index_ == config_.filterSizeTime - 1) {
      extract_internal();
      rotate();
    } else {
      ++filter_index_;
    }
  }

  ExtractedEventPoints & event_points() { return event_points_; }
};

}  // namespace olaf

#endif  // OLAF_EP_EXTRACTOR_HPP
