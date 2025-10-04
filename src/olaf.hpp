#pragma once

#include <arm_const_structs.h>
#include <arm_math.h>
#include <dsp/window_functions.h>

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
  : window(BlockSize == 1024 ? hamming_window_1024 : hamming_window_512),
    fft_in(),
    fft_out(),
    config(olaf::Config::create_esp_32()),
    ep_extractor(config),
    fp_extractor(config),
    fp_matcher(config, db, std::bind(&OlafRecognizer::on_match, this, _1, _2, _3, _4, _5, _6))
  {
    init_fft();
  }

  void process_audio(const uint16_t * audio_data, size_t length)
  {
    size_t offset = 0;
    while (offset + config.audioBlockSize <= length) {
      // Copy and convert to float (expected 16-bit PCM)
      for (size_t i = 0; i < BlockSize; ++i) {
        fft_in[i] = static_cast<float>(audio_data[offset + i]) / 32768.0f;
      }

      // Apply window
      for (size_t i = 0; i < BlockSize; ++i) {
        fft_in[i] *= window[i];
      }

      // Perform FFT
      arm_rfft_fast_f32(&instance, fft_in, fft_out, 0);

      // Extract event points
      auto event_points = ep_extractor.extract(fft_out, audio_block_index_);

      if (event_points->eventPointIndex > config.eventPointThreshold) {
        // Extract fingerprints
        auto fingerprints = fp_extractor.extract(event_points, audio_block_index_);

        if (fingerprints->fingerprint_index > 0) {
          fp_matcher.match(fingerprints);
        }

        fingerprints->fingerprint_index = 0;
      }

      audio_block_index_++;
      offset += config.audioBlockSize;
    }
  }

private:
  arm_rfft_fast_instance_f32 instance;
  const float * window;
  float fft_in[BlockSize];
  float fft_out[BlockSize];

  olaf::Config config;
  olaf::DB db;
  olaf::EPExtractor ep_extractor;
  olaf::FPExtractor fp_extractor;
  olaf::FPMatcher fp_matcher;

  uint32_t audio_block_index_ = 0;

  void init_fft()
  {
    static_assert(BlockSize == 1024 || BlockSize == 512, "Only 512 and 1024 supported");
    if constexpr (BlockSize == 1024) {
      if (auto result = arm_rfft_fast_init_1024_f32(&instance); result != ARM_MATH_SUCCESS) {
        printf("FFT init error %d\n", result);
      }
    }
    if constexpr (BlockSize == 512) {
      if (auto result = arm_rfft_fast_init_512_f32(&instance); result != ARM_MATH_SUCCESS) {
        printf("FFT init error %d\n", result);
      }
    }
  }

  void on_match(
    int match_count, float query_start, float query_stop, std::uint32_t audio_id,
    float reference_start, float reference_stop)
  {
  }
};
