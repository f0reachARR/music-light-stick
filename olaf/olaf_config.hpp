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

#ifndef OLAF_CONFIG_HPP
#define OLAF_CONFIG_HPP

#include <cstddef>
#include <cstdlib>

namespace olaf
{

/**
 * @struct Config
 * @brief Configuration parameters defining the behaviour of Olaf.
 *
 * Config configuration parameters define the behaviour of Olaf.
 * The configuration determines how Olaf behaves. The configuration settings are
 * set at compile time and should not change in between runs: if they do it is
 * possible that e.g. indexed fingerprints do not match extracted prints any more.
 */
struct Config
{
  // std::string dbFolder;

  //------ General Configuration
  int audioBlockSize;
  int audioSampleRate;
  int audioStepSize;
  int bytesPerAudioSample;
  bool verbose;

  //------ EVENT POINT Configuration
  int maxEventPointsPerBlock;
  int filterSizeTime;
  int halfFilterSizeTime;
  int filterSizeFrequency;
  int halfFilterSizeFrequency;
  float minEventPointMagnitude;
  int minFrequencyBin;
  int maxEventPointUsages;
  int maxEventPoints;
  int eventPointThreshold;
  bool sqrtMagnitude;

  //-----------Fingerprint configuration
  bool useMagnitudeInfo;
  int numberOfEPsPerFP;
  int minTimeDistance;
  int maxTimeDistance;
  int minFreqDistance;
  int maxFreqDistance;
  std::size_t maxFingerprints;

  //------------ Matcher configuration
  std::size_t maxResults;
  int searchRange;
  int minMatchCount;
  float minMatchTimeDiff;
  float keepMatchesFor;
  float printResultEvery;
  std::size_t maxDBCollisions;

  /**
     * The default configuration to use on traditional computers.
     */
  static Config create_default()
  {
    Config config;

    // audio info
    config.audioBlockSize = 1024;
    config.audioSampleRate = 16000;
    config.audioStepSize = 128;
    config.bytesPerAudioSample = 4;  // 32 bits float

    config.maxEventPoints = 60;
    config.eventPointThreshold = 30;
    config.sqrtMagnitude = false;

    // the filter used in both frequency as time direction
    config.filterSizeFrequency = 103;
    config.halfFilterSizeFrequency = config.filterSizeFrequency / 2;

    config.filterSizeTime = 24;
    config.halfFilterSizeTime = config.filterSizeTime / 2;

    config.minEventPointMagnitude = 0.001f;
    config.maxEventPointUsages = 10;
    config.minFrequencyBin = 9;
    config.verbose = false;

    // The number of event points (peaks) per fingerprint
    config.numberOfEPsPerFP = 3;
    config.useMagnitudeInfo = false;

    config.minTimeDistance = 2;
    config.maxTimeDistance = 33;
    config.minFreqDistance = 1;
    config.maxFreqDistance = 128;

    config.maxFingerprints = 300;

    // maximum number of results
    config.maxResults = 50;
    config.searchRange = 5;
    config.minMatchCount = 6;
    config.minMatchTimeDiff = 0;
    config.keepMatchesFor = 0;
    config.printResultEvery = 0;
    config.maxDBCollisions = 2000;

    return config;
  }

  /**
     * The configuration to use on ESP32 microcontrollers.
     */
  static Config create_esp_32()
  {
    Config config = create_default();

    config.verbose = false;
    config.numberOfEPsPerFP = 2;
    config.maxEventPointUsages = 20;
    config.audioStepSize = 256;

    config.maxResults = 20;
    config.maxEventPoints = 50;
    config.eventPointThreshold = 30;
    config.maxFingerprints = 30;
    config.searchRange = 5;
    config.maxDBCollisions = 50;
    config.minMatchCount = 4;
    config.minMatchTimeDiff = 1.0f;
    config.keepMatchesFor = 9;
    config.printResultEvery = 1;

    return config;
  }

  /**
     * The configuration to use for an in memory database.
     */
  static Config create_mem()
  {
    Config config = create_esp_32();

    config.maxResults = 10;
    config.printResultEvery = 0;
    config.keepMatchesFor = 0;
    config.verbose = false;

    return config;
  }
};

}  // namespace olaf

#endif  // OLAF_CONFIG_HPP
