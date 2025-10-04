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

#ifndef OLAF_FP_MATCHER_HPP
#define OLAF_FP_MATCHER_HPP

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <vector>

#include "olaf_config.hpp"
#include "olaf_db.hpp"
#include "olaf_fp_extractor.hpp"

namespace olaf
{

/**
 * @brief Callback function to respond to a match result.
 */
using MatchResultCallback = std::function<void(
  int match_count, float query_start, float query_stop, std::uint32_t audio_id,
  float reference_start, float reference_stop)>;

/**
 * @struct MatchResult
 * @brief Represents a single match result
 */
struct MatchResult
{
  int reference_fingerprint_t1 = 0;
  int query_fingerprint_t1 = 0;
  int first_reference_fingerprint_t1 = 0;
  int last_reference_fingerprint_t1 = 0;
  int match_count = 0;
  std::uint32_t match_identifier = 0;
  std::uint64_t result_hash_table_key = 0;
};

/**
 * @class FPMatcher
 * @brief Matches extracted fingerprints with a database
 */
class FPMatcher
{
private:
  const Config & config_;
  DB & db_;
  std::unordered_map<std::uint64_t, MatchResult> result_hash_table_;
  std::vector<std::uint64_t> db_results_;
  MatchResultCallback result_callback_;
  int last_print_at_ = 0;

  void tally_results(
    int query_fingerprint_t1, int reference_fingerprint_t1, std::uint32_t match_identifier)
  {
    const int time_diff = (query_fingerprint_t1 - reference_fingerprint_t1) >> 2;

    const std::uint64_t diff_part = (static_cast<std::uint64_t>(time_diff)) << 32;
    const std::uint64_t match_part = static_cast<std::uint64_t>(match_identifier);
    const std::uint64_t result_hash_table_key = diff_part + match_part;

    auto it = result_hash_table_.find(result_hash_table_key);

    if (it != result_hash_table_.end()) {
      // Update existing match
      auto & match = it->second;
      match.reference_fingerprint_t1 = reference_fingerprint_t1;
      match.query_fingerprint_t1 = query_fingerprint_t1;
      match.match_count++;
      match.first_reference_fingerprint_t1 =
        std::min(reference_fingerprint_t1, match.first_reference_fingerprint_t1);
      match.last_reference_fingerprint_t1 =
        std::max(reference_fingerprint_t1, match.last_reference_fingerprint_t1);
    } else {
      // Create new match
      MatchResult match;
      match.reference_fingerprint_t1 = reference_fingerprint_t1;
      match.first_reference_fingerprint_t1 = reference_fingerprint_t1;
      match.last_reference_fingerprint_t1 = reference_fingerprint_t1;
      match.query_fingerprint_t1 = query_fingerprint_t1;
      match.match_count = 1;
      match.match_identifier = match_identifier;
      match.result_hash_table_key = result_hash_table_key;

      result_hash_table_[result_hash_table_key] = match;
    }
  }

  void match_single_fingerprint(
    std::uint32_t query_fingerprint_t1, std::uint64_t query_fingerprint_hash)
  {
    const int range = config_.searchRange;
    const std::size_t number_of_results = db_.find(
      query_fingerprint_hash - range, query_fingerprint_hash + range, db_results_,
      config_.maxDBCollisions);

    if (config_.verbose) {
      std::fprintf(
        stderr,
        "Matched fp hash %" PRIu64
        " with database at q t1 %u, search range %d.\n"
        "\tNumber of results: %zu\n\tMax num results: %zu\n",
        query_fingerprint_hash, query_fingerprint_t1, range, number_of_results,
        config_.maxDBCollisions);
    }

    if (number_of_results >= config_.maxDBCollisions) {
      std::fprintf(
        stderr,
        "Expected less results for fp hash %" PRIu64
        ", Number of results: %zu, search range %d, max: %zu\n",
        query_fingerprint_hash, number_of_results, range, config_.maxDBCollisions);
    }

    for (const auto & db_result : db_results_) {
      const std::uint32_t reference_fingerprint_t1 = static_cast<std::uint32_t>(db_result >> 32);
      const std::uint32_t match_identifier = static_cast<std::uint32_t>(db_result);

      if (config_.verbose) {
        const int delta =
          static_cast<int>(query_fingerprint_t1) - static_cast<int>(reference_fingerprint_t1);
        std::fprintf(
          stderr, "\taudio id: %u\n\tref t1: %u\n\tdelta qt1-ft1: %d\n", match_identifier,
          reference_fingerprint_t1, delta);
      }

      tally_results(query_fingerprint_t1, reference_fingerprint_t1, match_identifier);
    }
  }

  void remove_old_matches(int current_query_time)
  {
    const int max_age =
      static_cast<int>((config_.keepMatchesFor * config_.audioSampleRate) / config_.audioStepSize);

    auto it = result_hash_table_.begin();
    while (it != result_hash_table_.end()) {
      const int age = current_query_time - it->second.query_fingerprint_t1;
      if (age > max_age) {
        it = result_hash_table_.erase(it);
      } else {
        ++it;
      }
    }
  }

public:
  FPMatcher(const Config & config, DB & db, MatchResultCallback callback)
  : config_(config),
    db_(db),
    result_callback_(std::move(callback)),
    last_print_at_(0)
  {
    db_results_.reserve(config.maxDBCollisions);
  }

  void match(ExtractedFingerprints & fingerprints)
  {
    auto first = fingerprints.fingerprints.begin();
    auto last = first + fingerprints.fingerprint_index;

    for (auto it = first; it != last; ++it) {
      const std::uint64_t hash = it->calculate_hash();
      match_single_fingerprint(it->time_index1, hash);
    }

    if (fingerprints.fingerprint_index > 0 && config_.printResultEvery != 0) {
      const int print_result_every = static_cast<int>(
        (config_.printResultEvery * config_.audioSampleRate) / config_.audioStepSize);
      const int current_query_time = (last - 1)->time_index3;

      if (current_query_time - last_print_at_ > print_result_every) {
        print_header();
        print_results();
        last_print_at_ = current_query_time;
      }
    }

    if (fingerprints.fingerprint_index > 0 && config_.keepMatchesFor != 0) {
      const int current_query_time = (last - 1)->time_index3;
      remove_old_matches(current_query_time);
    }

    fingerprints.fingerprint_index = 0;
  }

  static void print_header()
  {
    std::printf(
      "match count (#), q start (s) , q stop (s), ref path, ref ID, ref start (s), ref stop (s)\n");
  }

  static void print_result_default(
    int match_count, float query_start, float query_stop, const char * path,
    std::uint32_t match_identifier, float reference_start, float reference_stop)
  {
    std::printf(
      "%d, %.2f, %.2f, %s, %u, %.2f, %.2f\n", match_count, query_start, query_stop, path,
      match_identifier, reference_start, reference_stop);
  }

  void print_results()
  {
    std::vector<std::reference_wrapper<const MatchResult>> match_results;
    match_results.reserve(config_.maxResults);

    for (const auto & pair : result_hash_table_) {
      const auto & match = pair.second;

      if (match.match_count >= config_.minMatchCount) {
        if (match_results.size() >= config_.maxResults) {
          std::sort(
            match_results.begin(), match_results.end(),
            [](const MatchResult & a, const MatchResult & b) {
              return b.match_count < a.match_count;
            });

          const int current_least = match_results.back().get().match_count;
          if (match.match_count > current_least) {
            match_results.back() = std::cref(match);
          }
        } else {
          match_results.push_back(std::cref(match));
        }
      }
    }

    if (!match_results.empty()) {
      std::sort(
        match_results.begin(), match_results.end(),
        [](const MatchResult & a, const MatchResult & b) { return b.match_count < a.match_count; });
    }

    const float seconds_per_block =
      static_cast<float>(config_.audioStepSize) / static_cast<float>(config_.audioSampleRate);

    for (const auto & match_ref : match_results) {
      const auto & match = match_ref.get();
      const float time_delta =
        seconds_per_block * (match.query_fingerprint_t1 - match.reference_fingerprint_t1);

      const float reference_start = match.first_reference_fingerprint_t1 * seconds_per_block;
      const float reference_stop = match.last_reference_fingerprint_t1 * seconds_per_block;

      if ((reference_stop - reference_start) >= config_.minMatchTimeDiff) {
        const float query_start = match.first_reference_fingerprint_t1 * seconds_per_block + time_delta;
        const float query_stop = match.last_reference_fingerprint_t1 * seconds_per_block + time_delta;

        result_callback_(
          match.match_count, query_start, query_stop, match.match_identifier, reference_start,
          reference_stop);
      }
    }

    if (match_results.empty()) {
      result_callback_(0, 0, 0, 0, 0, 0);
    }
  }
};

}  // namespace olaf

#endif  // OLAF_FP_MATCHER_HPP
