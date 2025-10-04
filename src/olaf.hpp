#pragma once

#include <arm_const_structs.h>
#include <arm_math.h>
#include <dsp/window_functions.h>

#include <cstdio>

#include "olaf_config.hpp"
#include "olaf_db.hpp"
#include "olaf_ep_extractor.hpp"
#include "olaf_fp_extractor.hpp"
#include "olaf_fp_matcher.hpp"
extern "C" {
#include "olaf_window.h"
}

using namespace std::placeholders;

template <size_t BlockSize = 1024, size_t SampleRate = 16000>
class OlafRecognizer
{
public:
  OlafRecognizer()
  : window_(BlockSize == 1024 ? hamming_window_1024 : hamming_window_512),
    fft_in_(),
    fft_out_(),
    config_(olaf::Config::create_esp_32()),
    ep_extractor_(config_),
    fp_extractor_(config_),
    fp_matcher(config_, db_, std::bind(&OlafRecognizer::on_match, this, _1, _2, _3, _4, _5, _6))
  {
    init_fft();
  }

  olaf::Config & config() { return config_; }

  olaf::DB & db() { return db_; }

  void process_audio(const uint16_t * audio_data)
  {
    // Copy and convert to float (expected 16-bit PCM)
    for (size_t i = 0; i < BlockSize; ++i) {
      fft_in_[i] = static_cast<float>(audio_data[i]) / 32768.0f;
    }

    // Apply window
    for (size_t i = 0; i < BlockSize; ++i) {
      fft_in_[i] *= window_[i];
    }

    // Perform FFT
    arm_rfft_fast_f32(&instance_, fft_in_, fft_out_, 0);

    // Extract event points
    ep_extractor_.extract(fft_out_, audio_block_index_);
    auto & event_points = ep_extractor_.event_points();

    if (event_points.event_point_index > config_.eventPointThreshold) {
      // Extract fingerprints
      fp_extractor_.extract(event_points, audio_block_index_);
      auto & fingerprints = fp_extractor_.get_fingerprints();

      if (fingerprints.fingerprint_index > 0) {
        fp_matcher.match(fingerprints);
      }

      fingerprints.fingerprint_index = 0;
    }

    audio_block_index_++;
  }

private:
  arm_rfft_fast_instance_f32 instance_;
  const float * window_;
  float fft_in_[BlockSize];
  float fft_out_[BlockSize];

  olaf::Config config_;
  olaf::DB db_;
  olaf::EPExtractor ep_extractor_;
  olaf::FPExtractor fp_extractor_;
  olaf::FPMatcher fp_matcher;

  uint32_t audio_block_index_ = 0;

  void init_fft()
  {
    static_assert(BlockSize == 1024 || BlockSize == 512, "Only 512 and 1024 supported");
    if constexpr (BlockSize == 1024) {
      if (auto result = arm_rfft_fast_init_1024_f32(&instance_); result != ARM_MATH_SUCCESS) {
        printf("FFT init error %d\n", result);
      }
    }
    if constexpr (BlockSize == 512) {
      if (auto result = arm_rfft_fast_init_512_f32(&instance_); result != ARM_MATH_SUCCESS) {
        printf("FFT init error %d\n", result);
      }
    }
  }

  void on_match(
    int match_count, float query_start, float query_stop, std::uint32_t audio_id,
    float reference_start, float reference_stop)
  {
    printf(
      "Match: count=%d, q_start=%.2f, q_stop=%.2f, id=%u, ref_start=%.2f, ref_stop=%.2f\n",
      match_count, query_start, query_stop, audio_id, reference_start, reference_stop);
  }
};
