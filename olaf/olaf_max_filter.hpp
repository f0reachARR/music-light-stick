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

#ifndef OLAF_MAX_FILTER_HPP
#define OLAF_MAX_FILTER_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace olaf
{

// Precomputed perceptual min/max indices for 512-sized arrays
constexpr std::array<std::size_t, 512> perceptual_min_idx = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   9,   9,   9,   9,   9,   9,   9,   9,   10,  10,
  11,  12,  12,  12,  13,  14,  14,  14,  15,  15,  16,  16,  17,  17,  18,  19,  19,  19,  21,
  21,  22,  22,  23,  23,  25,  25,  25,  26,  26,  26,  27,  27,  27,  29,  29,  29,  31,  31,
  31,  33,  33,  33,  35,  35,  35,  35,  37,  37,  37,  37,  39,  39,  39,  39,  41,  41,  41,
  41,  43,  43,  43,  43,  43,  47,  47,  47,  47,  47,  51,  51,  51,  51,  51,  53,  53,  53,
  53,  53,  55,  55,  55,  55,  55,  55,  59,  59,  59,  59,  59,  59,  63,  63,  63,  63,  63,
  63,  63,  67,  67,  67,  67,  67,  67,  67,  71,  71,  71,  71,  71,  71,  71,  75,  75,  75,
  75,  75,  75,  75,  75,  79,  79,  79,  79,  79,  79,  79,  79,  83,  83,  83,  83,  83,  83,
  83,  83,  83,  87,  87,  87,  87,  87,  87,  87,  87,  87,  95,  95,  95,  95,  95,  95,  95,
  95,  95,  95,  99,  99,  99,  99,  99,  99,  99,  99,  99,  99,  103, 103, 103, 103, 103, 103,
  103, 103, 103, 103, 103, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 119, 119,
  119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 143,
  143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 151, 151, 151, 151, 151, 151,
  151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 159, 159, 159, 159, 159, 159, 159, 159, 159,
  159, 159, 159, 159, 159, 159, 159, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167,
  167, 167, 167, 167, 167, 167, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175,
  175, 175, 175, 175, 175, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191,
  191, 191, 191, 191, 191, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
  199, 199, 199, 199, 199, 199, 199, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
  207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 223, 223, 223, 223, 223, 223, 223, 223, 223,
  223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 239, 239, 239, 239, 239,
  239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
  239, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 271, 271, 271, 271, 271, 271, 271, 271, 271, 271, 271,
  271, 271, 271, 271, 271, 271, 271, 271, 271, 271, 271, 271, 271, 271, 271, 271, 287, 287, 287,
  287, 287, 287, 287, 287, 287, 287, 287, 287, 287, 287, 287, 287, 287, 287, 287, 287, 287};

constexpr std::array<std::size_t, 512> perceptual_max_idx = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   16,  18,  19,  22,  23,  26,  27,  29,  31,  33,
  35,  37,  37,  39,  41,  43,  43,  47,  51,  51,  53,  53,  55,  55,  59,  63,  63,  63,  67,
  67,  71,  71,  75,  75,  79,  79,  79,  83,  83,  83,  87,  87,  87,  95,  95,  95,  99,  99,
  99,  103, 103, 103, 111, 111, 111, 111, 119, 119, 119, 119, 127, 127, 127, 127, 135, 135, 135,
  135, 143, 143, 143, 143, 143, 151, 151, 151, 151, 151, 159, 159, 159, 159, 159, 167, 167, 167,
  167, 167, 175, 175, 175, 175, 175, 175, 191, 191, 191, 191, 191, 191, 199, 199, 199, 199, 199,
  199, 199, 207, 207, 207, 207, 207, 207, 207, 223, 223, 223, 223, 223, 223, 223, 239, 239, 239,
  239, 239, 239, 239, 239, 255, 255, 255, 255, 255, 255, 255, 255, 271, 271, 271, 271, 271, 271,
  271, 271, 271, 287, 287, 287, 287, 287, 287, 287, 287, 287, 303, 303, 303, 303, 303, 303, 303,
  303, 303, 303, 319, 319, 319, 319, 319, 319, 319, 319, 319, 319, 335, 335, 335, 335, 335, 335,
  335, 335, 335, 335, 335, 351, 351, 351, 351, 351, 351, 351, 351, 351, 351, 351, 351, 383, 383,
  383, 383, 383, 383, 383, 383, 383, 383, 383, 383, 399, 399, 399, 399, 399, 399, 399, 399, 399,
  399, 399, 399, 399, 415, 415, 415, 415, 415, 415, 415, 415, 415, 415, 415, 415, 415, 415, 447,
  447, 447, 447, 447, 447, 447, 447, 447, 447, 447, 447, 447, 447, 479, 479, 479, 479, 479, 479,
  479, 479, 479, 479, 479, 479, 479, 479, 479, 479, 495, 495, 495, 495, 495, 495, 495, 495, 495,
  495, 495, 495, 495, 495, 495, 495, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
  512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512};

// for speed, there is a limit on the number of bins to evaluate
constexpr std::size_t van_herk_filter_width = 103;

// the naive implementation has a changing and small filter width
// it is not that easy to optimize. From this bin on the filter is replaced
// by a filter with a fixed width starting from this bin
constexpr std::size_t naive_implementation_stop_bin = 82;
/**
 * @brief A naive max filter implementation for reference.
 */
inline void max_filter_naive(
  const std::vector<float> & array, std::size_t filter_width, std::vector<float> & maxvalues)
{
  const std::size_t array_size = array.size();
  const std::size_t half_filter_width = filter_width / 2;
  for (std::size_t i = 0; i < array_size; ++i) {
    const std::size_t start_index = (i >= half_filter_width) ? (i - half_filter_width) : 0;
    const std::size_t stop_index = std::min(i + half_filter_width + 1, array_size);

    maxvalues[i] = -100000.0f;
    for (std::size_t j = start_index; j < stop_index; ++j) {
      maxvalues[i] = std::max(maxvalues[i], array[j]);
    }
  }
}

/**
 * @brief Van Herk-Gil-Werman max filter implementation.
 * Based on https://github.com/lemire/runningmaxmin (LGPL)
 */
inline void max_filter_van_herk_gil_werman(
  const std::vector<float> & array, std::size_t offset, std::size_t array_size,
  std::vector<float> & maxvalues, std::size_t output_offset)
{
  static std::array<float, van_herk_filter_width> R = {0.0f};
  static std::array<float, van_herk_filter_width> S = {0.0f};

  for (std::size_t j = 0; j < array_size - van_herk_filter_width + 1; j += van_herk_filter_width) {
    const std::size_t Rpos = std::min(j + van_herk_filter_width - 1, array_size - 1);
    R[0] = array.at(offset + Rpos);

    for (std::size_t i = Rpos - 1; i + 1 > j; --i) {
      R.at(Rpos - i) = std::max(R.at(Rpos - i - 1), array.at(offset + i));
    }

    S[0] = array.at(offset + Rpos);
    const std::size_t m1 = std::min(j + 2 * van_herk_filter_width - 1, array_size);

    for (std::size_t i = Rpos + 1; i < m1; ++i) {
      S.at(i - Rpos) = std::max(S.at(i - Rpos - 1), array.at(offset + i));
    }

    for (std::size_t i = 0; i < m1 - Rpos; ++i) {
      maxvalues.at(output_offset + j + i) = std::max(S.at(i), R.at((Rpos - j + 1) - i - 1));
    }
  }
}

/**
 * @brief Perceptually-weighted max filter optimized for 512-sized arrays.
 */
inline void max_filter(
  const std::vector<float> & array, std::size_t filter_width, std::vector<float> & maxvalues)
{
  // filter_width is ignored; perceptual indices are used instead
  (void)filter_width;

  const std::size_t array_size = array.size();

  // This filter only works for 512 sized arrays
  assert(array_size == 512);

  // Process lower frequency bins with naive implementation (varying filter widths)
  for (std::size_t f = 9; f < naive_implementation_stop_bin; ++f) {
    const std::size_t start_index = perceptual_min_idx[f];
    const std::size_t stop_index = perceptual_max_idx[f];

    assert(stop_index - start_index < van_herk_filter_width);

    float max_value = -1000000.0f;
    for (std::size_t j = start_index; j < stop_index; ++j) {
      max_value = std::max(max_value, array[j]);
    }

    maxvalues.at(f) = max_value;
  }

  // Process higher frequency bins with Van Herk filter (fixed width)
  const std::size_t output_offset = naive_implementation_stop_bin + (van_herk_filter_width / 2);
  const std::size_t input_offset = naive_implementation_stop_bin;
  const std::size_t to_filter_size = array_size - naive_implementation_stop_bin;

  max_filter_van_herk_gil_werman(array, input_offset, to_filter_size, maxvalues, output_offset);
}

}  // namespace olaf

#endif  // OLAF_MAX_FILTER_HPP
