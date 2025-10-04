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

#ifndef OLAF_DB_HPP
#define OLAF_DB_HPP

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

namespace olaf
{

/**
 * @struct AudioReference
 * @brief Reference to a single audio file's fingerprint array
 */
struct AudioReference
{
  std::uint32_t audio_id;
  std::span<const std::uint64_t> fingerprints;
};

/**
 * @class DB
 * @brief In-memory fingerprint database supporting multiple audio files
 *
 * Each audio file is represented by a static array like olaf_db_mem_fps[].
 * The database stores pointers to these arrays without copying data.
 */
class DB
{
private:
  // List of audio references (no heap allocation for fingerprint data)
  std::vector<AudioReference> audio_refs_;

  static void unpack(std::uint64_t packed, std::uint64_t & hash, std::uint32_t & timestamp)
  {
    hash = (packed >> 16);
    timestamp = static_cast<std::uint32_t>(static_cast<std::uint16_t>(packed));
  }

  static std::uint64_t pack(std::uint64_t hash, std::uint32_t timestamp)
  {
    return (hash << 16) + (timestamp & 0xFFFF);
  }

public:
  explicit DB() {}

  /**
     * @brief Register a static fingerprint array for an audio file
     * @param audio_id Unique identifier for this audio
     * @param fingerprints Pointer to static array of packed fingerprints
     * @param fp_length Number of fingerprints in array
     * @param path Audio file path
     * @param duration Audio duration in seconds
     */
  void register_audio(
    std::uint32_t audio_id, const std::uint64_t * fingerprints, std::size_t fp_length)
  {
    AudioReference ref;
    ref.audio_id = audio_id;
    ref.fingerprints = std::span<const std::uint64_t>(fingerprints, fp_length);

    audio_refs_.push_back(ref);

    std::fprintf(stderr, "Registered audio ID %u (%zu fingerprints)\n", audio_id, fp_length);
  }

  /**
     * @brief Find fingerprints across all registered audio files
     * @param start_key Start hash (inclusive)
     * @param stop_key Stop hash (inclusive)
     * @param results Output array (timestamp << 32 | audio_id)
     * @param results_size Maximum results
     * @return Number of results found
     */
  std::size_t find(
    std::uint64_t start_key, std::uint64_t stop_key, std::uint64_t * results,
    std::size_t results_size) const
  {
    std::size_t results_index = 0;

    // Search through each audio file
    for (const auto & audio_ref : audio_refs_) {
      const std::uint64_t * match = nullptr;

      // Binary search for any key in range within this audio's fingerprints
      for (std::uint64_t current_key = start_key; current_key <= stop_key; ++current_key) {
        const std::uint64_t packed_key = pack(current_key, 0);

        // Binary search in sorted array
        auto it = std::lower_bound(
          audio_ref.fingerprints.begin(), audio_ref.fingerprints.end(), packed_key,
          [](std::uint64_t a, std::uint64_t b) { return (a >> 16) < (b >> 16); });

        if (it != audio_ref.fingerprints.end() && ((*it) >> 16) == (packed_key >> 16)) {
          match = &(*it);
          break;
        }
      }

      if (match != nullptr) {
        // Find all collisions (same hash range, different timestamps)
        const std::size_t index = match - audio_ref.fingerprints.data();

        // Search backwards from match
        for (std::size_t i = index; i < audio_ref.fingerprints.size(); --i) {
          std::uint64_t ref_hash;
          std::uint32_t ref_t;
          unpack(audio_ref.fingerprints[i], ref_hash, ref_t);

          if (ref_hash >= start_key && ref_hash <= stop_key) {
            if (results_index < results_size) {
              const std::uint64_t t = ref_t;
              results[results_index++] = (t << 32) | audio_ref.audio_id;
            } else {
              std::fprintf(stderr, "Warning: Max results %zu reached\n", results_size);
              return results_index;
            }
          } else {
            break;
          }

          if (i == 0) break;  // Prevent underflow
        }

        // Search forwards from match
        for (std::size_t i = index + 1; i < audio_ref.fingerprints.size(); ++i) {
          std::uint64_t ref_hash;
          std::uint32_t ref_t;
          unpack(audio_ref.fingerprints[i], ref_hash, ref_t);

          if (ref_hash >= start_key && ref_hash <= stop_key) {
            if (results_index < results_size) {
              const std::uint64_t t = ref_t;
              results[results_index++] = (t << 32) | audio_ref.audio_id;
            } else {
              std::fprintf(stderr, "Warning: Max results %zu reached\n", results_size);
              return results_index;
            }
          } else {
            break;
          }
        }
      }
    }

    return results_index;
  }

  /**
     * @brief Check if any fingerprint exists in range across all audio files
     */
  bool find_single(std::uint64_t start_key, std::uint64_t stop_key) const
  {
    for (const auto & audio_ref : audio_refs_) {
      for (const auto & packed : audio_ref.fingerprints) {
        std::uint64_t ref_hash;
        std::uint32_t ref_t;
        unpack(packed, ref_hash, ref_t);

        if (ref_hash < start_key) continue;
        if (ref_hash > stop_key) break;

        return true;  // Found hash in range
      }
    }
    return false;
  }

  /**
     * @brief Remove registered audio file
     */
  void delete_audio(std::uint32_t audio_id)
  {
    audio_refs_.erase(
      std::remove_if(
        audio_refs_.begin(), audio_refs_.end(),
        [audio_id](const AudioReference & ref) { return ref.audio_id == audio_id; }),
      audio_refs_.end());
  }

  /**
     * @brief Get database statistics
     */
  void print_stats(bool verbose = false) const
  {
    std::size_t total_fingerprints = 0;
    for (const auto & ref : audio_refs_) {
      total_fingerprints += ref.fingerprints.size();
    }

    std::printf("Database Statistics:\n");
    std::printf("  Total audio files: %zu\n", audio_refs_.size());
    std::printf("  Total fingerprints: %zu\n", total_fingerprints);

    if (verbose) {
      std::printf("\nRegistered audio files:\n");
      for (const auto & ref : audio_refs_) {
        std::printf("  ID %u:\n", ref.audio_id);
      }
    }
  }

  /**
     * @brief Simple string hash (Jenkins hash)
     */
  static std::uint32_t string_hash(const char * key, std::size_t len)
  {
    std::uint32_t hash = 0;
    for (std::size_t i = 0; i < len; ++i) {
      hash += static_cast<std::uint32_t>(key[i]);
      hash += (hash << 10);
      hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
  }

  // Utility methods

  std::size_t get_audio_count() const { return audio_refs_.size(); }

  std::size_t get_total_fingerprints() const
  {
    std::size_t total = 0;
    for (const auto & ref : audio_refs_) {
      total += ref.fingerprints.size();
    }
    return total;
  }

  void clear() { audio_refs_.clear(); }
};

}  // namespace olaf

#endif  // OLAF_DB_HPP
