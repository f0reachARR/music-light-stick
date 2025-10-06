/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Sample app to demonstrate PWM.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

#include "olaf.hpp"
#include "olaf_fp_ref_mem.h"
#include "pdm.hpp"

static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));
static const struct device * dmic_device_dt = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));

// Olaf configuration
#define OLAF_SAMPLE_RATE 16000
#define OLAF_AUDIO_BLOCK_SIZE 1024

// PDM configuration
#define PDM_SAMPLE_FREQUENCY OLAF_SAMPLE_RATE
#define PDM_SAMPLE_BIT_WIDTH 16
#define PDM_BYTES_PER_SAMPLE sizeof(int16_t)
#define PDM_NUMBER_OF_CHANNELS 1
#define PDM_SAMPLES_PER_BLOCK OLAF_AUDIO_BLOCK_SIZE * PDM_NUMBER_OF_CHANNELS
#define PDM_TIMEOUT 1000

// Memory slab for PDM
#define PDM_BLOCK_SIZE (PDM_BYTES_PER_SAMPLE * PDM_SAMPLES_PER_BLOCK)
#define PDM_BLOCK_COUNT 8

PDMAudioInput<uint16_t, PDM_SAMPLE_BIT_WIDTH, PDM_SAMPLE_FREQUENCY, PDM_BLOCK_SIZE, PDM_BLOCK_COUNT>
  pdm_input(dmic_device_dt);
OlafRecognizer<OLAF_AUDIO_BLOCK_SIZE, OLAF_SAMPLE_RATE> olaf_recognizer;

static void print_sys_memory_stats(struct sys_heap * hp)
{
  struct sys_memory_stats stats;

  sys_heap_runtime_stats_get(hp, &stats);

  printk(
    "allocated %zu, free %zu, max allocated %zu\n", stats.allocated_bytes, stats.free_bytes,
    stats.max_allocated_bytes);
}

void print_all_heaps(void)
{
  struct sys_heap ** ha;
  int n;

  n = sys_heap_array_get(&ha);
  printk("%d heap(s) allocated (including static):\n", n);

  for (int i = 0; i < n; i++) {
    printk("\t%d - address %p ", i, ha[i]);
    print_sys_memory_stats(ha[i]);
  }
}

int main(void)
{
  if (!device_is_ready(dmic_device_dt)) {
    printk("Error: PDM device %s is not ready\n", dmic_device_dt->name);
    return 0;
  }
  pdm_input.start();
  olaf_recognizer.config().verbose = false;
  olaf_recognizer.db().register_audio(
    1, olaf_db_mem_fps, sizeof(olaf_db_mem_fps) / sizeof(olaf_db_mem_fps[0]));
  while (1) {
    auto buffer = pdm_input.read();
    olaf_recognizer.process_audio(buffer.as<int16_t>());
    printf(".");
  }
  return 0;
}
