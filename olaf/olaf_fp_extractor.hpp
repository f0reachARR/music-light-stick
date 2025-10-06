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

#ifndef OLAF_FP_EXTRACTOR_HPP
#define OLAF_FP_EXTRACTOR_HPP

#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "olaf_config.hpp"
#include "olaf_ep_extractor.hpp"

namespace olaf
{

/**
 * @struct Fingerprint
 * @brief A fingerprint is a combination of three event points
 */
struct Fingerprint
{
  int frequency_bin1 = 0;
  int time_index1 = 0;
  float magnitude1 = 0.0f;

  int frequency_bin2 = 0;
  int time_index2 = 0;
  float magnitude2 = 0.0f;

  int frequency_bin3 = 0;
  int time_index3 = 0;
  float magnitude3 = 0.0f;

  void print() const
  {
    std::fprintf(stderr, "FP hash: %" PRIu64 " \n", calculate_hash());
    std::fprintf(stderr, "\tt1: %d, f1: %d, m1: %.3f\n", time_index1, frequency_bin1, magnitude1);
    std::fprintf(stderr, "\tt2: %d, f2: %d, m2: %.3f\n", time_index2, frequency_bin2, magnitude2);
    std::fprintf(stderr, "\tt3: %d, f3: %d, m3: %.3f\n", time_index3, frequency_bin3, magnitude3);
  }

  std::uint64_t calculate_hash() const
  {
    const int f1 = frequency_bin1;
    const int f2 = frequency_bin2;
    const int f3 = frequency_bin3;

    const int t1 = time_index1;
    const int t2 = time_index2;
    const int t3 = time_index3;

    const float m1 = magnitude1;
    const float m2 = magnitude2;
    const float m3 = magnitude3;

    const std::uint64_t f1_larger_than_f2 = (f1 > f2) ? 1 : 0;
    const std::uint64_t f2_larger_than_f3 = (f2 > f3) ? 1 : 0;
    const std::uint64_t f3_larger_than_f1 = (f3 > f1) ? 1 : 0;

    std::uint64_t m1_larger_than_m2 = (m1 > m2) ? 1 : 0;
    std::uint64_t m2_larger_than_m3 = (m2 > m3) ? 1 : 0;
    std::uint64_t m3_larger_than_m1 = (m3 > m1) ? 1 : 0;

    // Magnitude info is disabled
    m1_larger_than_m2 = 0;
    m2_larger_than_m3 = 0;
    m3_larger_than_m1 = 0;

    const std::uint64_t dt1t2_larger_than_t3t2 = ((t2 - t1) > (t3 - t2)) ? 1 : 0;
    const std::uint64_t df1f2_larger_than_f3f2 = (std::abs(f2 - f1) > std::abs(f3 - f2)) ? 1 : 0;

    const std::uint64_t f1_range = (f1 >> 1);
    const std::uint64_t df2f1 = (std::abs(f2 - f1) >> 2);
    const std::uint64_t df3f2 = (std::abs(f3 - f2) >> 2);
    const std::uint64_t diff_t = (t3 - t1);

    const std::uint64_t hash =
      ((diff_t & ((1 << 6) - 1)) << 0) + ((f1_larger_than_f2 & ((1 << 1) - 1)) << 6) +
      ((f2_larger_than_f3 & ((1 << 1) - 1)) << 7) + ((f3_larger_than_f1 & ((1 << 1) - 1)) << 8) +
      ((m1_larger_than_m2 & ((1 << 1) - 1)) << 9) + ((m2_larger_than_m3 & ((1 << 1) - 1)) << 10) +
      ((m3_larger_than_m1 & ((1 << 1) - 1)) << 11) +
      ((dt1t2_larger_than_t3t2 & ((1 << 1) - 1)) << 12) +
      ((df1f2_larger_than_f3f2 & ((1 << 1) - 1)) << 13) + ((f1_range & ((1 << 8) - 1)) << 14) +
      ((df2f1 & ((1 << 6) - 1)) << 22) +
      ((df3f2 & ((static_cast<std::uint64_t>(1) << 6) - 1)) << 28);

    return hash;
  }
};

/**
 * @struct ExtractedFingerprints
 * @brief The result of fingerprint extraction
 */
struct ExtractedFingerprints
{
  std::vector<Fingerprint> fingerprints;
  std::size_t fingerprint_index = 0;
};

/**
 * @class FPExtractor
 * @brief Fingerprint extractor state
 */
class FPExtractor
{
private:
  const Config & config_;
  ExtractedFingerprints fingerprints_;
  std::size_t total_fp_extracted_ = 0;
  bool warning_given_ = false;

  static int compare_event_points(const void * a, const void * b)
  {
    const auto & a_point = *static_cast<const EventPoint *>(a);
    const auto & b_point = *static_cast<const EventPoint *>(b);
    return a_point.time_index - b_point.time_index;
  }

  ExtractedFingerprints & extract_three(ExtractedEventPoints & event_points, int audio_block_index)
  {
    for (int i = 0; i < event_points.event_point_index; ++i) {
      const int t1 = event_points.event_points[i].time_index;
      const int f1 = event_points.event_points[i].frequency_bin;
      const float m1 = event_points.event_points[i].magnitude;
      const int u1 = event_points.event_points[i].usages;

      if (f1 == 0 && t1 == 0) break;
      if (u1 > config_.maxEventPointUsages) break;

      const int diff_to_current_time = audio_block_index - config_.maxTimeDistance;
      if (t1 > diff_to_current_time) break;

      for (int j = i + 1; j < event_points.event_point_index; ++j) {
        const int t2 = event_points.event_points[j].time_index;
        const int f2 = event_points.event_points[j].frequency_bin;
        const float m2 = event_points.event_points[j].magnitude;
        const int u2 = event_points.event_points[j].usages;

        int f_diff = std::abs(f1 - f2);
        int t_diff = t2 - t1;

        assert(t2 >= t1);
        assert(t_diff >= 0);

        if (u2 > config_.maxEventPointUsages) break;
        if (t_diff > config_.maxTimeDistance) break;

        if (
          t_diff >= config_.minTimeDistance && t_diff <= config_.maxTimeDistance &&
          f_diff >= config_.minFreqDistance && f_diff <= config_.maxFreqDistance) {
          assert(t2 > t1);

          for (int k = j + 1; k < event_points.event_point_index; ++k) {
            const int t3 = event_points.event_points[k].time_index;
            const int f3 = event_points.event_points[k].frequency_bin;
            const float m3 = event_points.event_points[k].magnitude;
            const int u3 = event_points.event_points[k].usages;

            if (u3 > config_.maxEventPointUsages) break;
            if (t_diff > config_.maxTimeDistance) break;

            f_diff = std::abs(f2 - f3);
            t_diff = t3 - t2;
            assert(t3 >= t2);
            assert(t_diff >= 0);

            if (
              t_diff >= config_.minTimeDistance && t_diff <= config_.maxTimeDistance &&
              f_diff >= config_.minFreqDistance && f_diff <= config_.maxFreqDistance) {
              assert(t3 > t2);

              if (fingerprints_.fingerprint_index >= config_.maxFingerprints) {
                if (!warning_given_) {
                  std::fprintf(
                    stderr,
                    "Warning: Fingerprint maximum index %zu reached, fingerprints are ignored, "
                    "consider increasing config.maxFingerprints if you see this often.\n",
                    fingerprints_.fingerprint_index);
                  warning_given_ = true;
                }
              } else {
                auto & fp = fingerprints_.fingerprints[fingerprints_.fingerprint_index];
                fp.time_index1 = t1;
                fp.time_index2 = t2;
                fp.time_index3 = t3;
                fp.frequency_bin1 = f1;
                fp.frequency_bin2 = f2;
                fp.frequency_bin3 = f3;
                fp.magnitude1 = m1;
                fp.magnitude2 = m2;
                fp.magnitude3 = m3;

                event_points.event_points[i].usages++;
                event_points.event_points[j].usages++;
                event_points.event_points[k].usages++;

                if (config_.verbose) {
                  std::fprintf(
                    stderr, "Fingerprint at index %zu\n", fingerprints_.fingerprint_index);
                  fp.print();
                }

                ++fingerprints_.fingerprint_index;
              }
            }
          }
        }
      }
    }
    return fingerprints_;
  }

  ExtractedFingerprints & extract_two(ExtractedEventPoints & event_points, int audio_block_index)
  {
    for (int i = 0; i < event_points.event_point_index; ++i) {
      const int t1 = event_points.event_points[i].time_index;
      const int f1 = event_points.event_points[i].frequency_bin;
      const float m1 = event_points.event_points[i].magnitude;
      const int u1 = event_points.event_points[i].usages;

      if (f1 == 0 && t1 == 0) break;
      if (u1 > config_.maxEventPointUsages) break;

      const int diff_to_current_time = audio_block_index - config_.maxTimeDistance;
      if (t1 > diff_to_current_time) break;

      for (int j = i + 1; j < event_points.event_point_index; ++j) {
        const int t2 = event_points.event_points[j].time_index;
        const int f2 = event_points.event_points[j].frequency_bin;
        const float m2 = event_points.event_points[j].magnitude;
        const int u2 = event_points.event_points[j].usages;

        const int f_diff = std::abs(f1 - f2);
        const int t_diff = t2 - t1;

        assert(t2 >= t1);
        assert(t_diff >= 0);

        if (u2 > config_.maxEventPointUsages) break;
        if (t_diff > config_.maxTimeDistance) break;

        if (
          t_diff >= config_.minTimeDistance && t_diff <= config_.maxTimeDistance &&
          f_diff >= config_.minFreqDistance && f_diff <= config_.maxFreqDistance) {
          assert(t2 > t1);

          if (fingerprints_.fingerprint_index == config_.maxFingerprints) {
            if (!warning_given_) {
              std::fprintf(
                stderr,
                "Warning: Fingerprint maximum index %zu reached, fingerprints are ignored, "
                "consider increasing config.maxFingerprints if you see this often.\n",
                fingerprints_.fingerprint_index);
              warning_given_ = true;
            }
          } else {
            auto & fp = fingerprints_.fingerprints[fingerprints_.fingerprint_index];
            fp.time_index1 = t1;
            fp.time_index2 = t2;
            fp.time_index3 = t2;
            fp.frequency_bin1 = f1;
            fp.frequency_bin2 = f2;
            fp.frequency_bin3 = f2;
            fp.magnitude1 = m1;
            fp.magnitude2 = m2;
            fp.magnitude3 = m2;

            event_points.event_points[i].usages++;
            event_points.event_points[j].usages++;

            if (config_.verbose) {
              fp.print();
            }

            ++fingerprints_.fingerprint_index;
          }

          assert(fingerprints_.fingerprint_index <= config_.maxFingerprints);
        }
      }
    }
    return fingerprints_;
  }

public:
  explicit FPExtractor(const Config & config) : config_(config)
  {
    fingerprints_.fingerprints.resize(config_.maxFingerprints);
    fingerprints_.fingerprint_index = 0;
    total_fp_extracted_ = 0;
    warning_given_ = false;
  }

  std::size_t get_total() const { return total_fp_extracted_; }

  void extract(ExtractedEventPoints & event_points, int audio_block_index)
  {
    if (config_.verbose) {
      std::fprintf(stderr, "Combining event points into fingerprints:\n");
      for (int i = 0; i < event_points.event_point_index; ++i) {
        std::fprintf(stderr, "\tidx: %d, ", i);
        event_points.event_points[i].print();
      }
    }

    if (config_.numberOfEPsPerFP == 2) {
      extract_two(event_points, audio_block_index);
    } else if (config_.numberOfEPsPerFP == 3) {
      extract_three(event_points, audio_block_index);
    } else {
      assert(false);
    }

    const int cutoff_time =
      event_points.event_points[event_points.event_point_index - 1].time_index -
      config_.maxTimeDistance;
    const int max_event_point_usages = config_.maxEventPointUsages;

    for (int i = 0; i < event_points.event_point_index; ++i) {
      if (
        event_points.event_points[i].time_index <= cutoff_time ||
        event_points.event_points[i].usages == max_event_point_usages) {
        event_points.event_points[i].time_index = (1 << 23);
        event_points.event_points[i].frequency_bin = 0;
        event_points.event_points[i].magnitude = 0;
      }
    }

    // std::qsort(
    // event_points.event_points.data(), config_.maxEventPoints, sizeof(EventPoint),
    // compare_event_points);
    std::sort(
      event_points.event_points.begin(),
      event_points.event_points.begin() + event_points.event_point_index,
      [](const EventPoint & a, const EventPoint & b) { return a.time_index < b.time_index; });

    for (int i = 0; i < event_points.event_point_index; ++i) {
      if (event_points.event_points[i].time_index == (1 << 23)) {
        event_points.event_point_index = i;
        break;
      }
    }

    total_fp_extracted_ += fingerprints_.fingerprint_index;

    if (config_.verbose) {
      std::fprintf(
        stderr, "New EP index %d, cutoffTime %d\n", event_points.event_point_index, cutoff_time);
      for (int i = 0; i < event_points.event_point_index; ++i) {
        std::fprintf(stderr, "idx:%d, ", i);
        event_points.event_points[i].print();
      }
    }
  }

  ExtractedFingerprints & get_fingerprints() { return fingerprints_; }
};

}  // namespace olaf

#endif  // OLAF_FP_EXTRACTOR_HPP
