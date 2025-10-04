#pragma once

#include <zephyr/audio/dmic.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include "slab.hpp"

template <
  typename TBuf, uint8_t BitWidth, uint32_t SampleRate, uint16_t BlockSize, size_t BlockCount>
class PDMAudioInput
{
public:
  PDMAudioInput(const struct device * device) : device_(device), stream_({}), cfg_({})
  {
    ret = k_mem_slab_init(&slab_, buffer_, BlockSize, BlockCount);
    if (ret < 0) {
      printk("k_mem_slab_init failed %d\n", ret);
      return;
    }

    stream_.pcm_rate = SampleRate;
    stream_.pcm_width = BitWidth;
    stream_.block_size = BlockSize;
    stream_.mem_slab = &slab_;

    cfg_.streams = &stream_;
    cfg_.channel.req_num_streams = 1;
    cfg_.channel.req_num_chan = 1;
    cfg_.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
    cfg_.io.min_pdm_clk_freq = 1000000;
    cfg_.io.max_pdm_clk_freq = 3500000;
    cfg_.io.min_pdm_clk_dc = 40;
    cfg_.io.max_pdm_clk_dc = 60;

    ret = dmic_configure(device_, &cfg_);
    if (ret < 0) {
      printk("Failed to configure PDM driver: %d", ret);
      return;
    }
  }

  ~PDMAudioInput() { stop(); }

  int start()
  {
    ret = dmic_trigger(device_, DMIC_TRIGGER_START);
    if (ret < 0) printk("START trigger failed: %d", ret);
    return ret;
  }

  int stop()
  {
    ret = dmic_trigger(device_, DMIC_TRIGGER_STOP);
    if (ret < 0) printk("STOP trigger failed: %d", ret);
    return ret;
  }

  MemorySlabChunk read(k_timeout_t timeout = K_FOREVER)
  {
    size_t size;
    void * buf;
    ret = dmic_read(device_, 0, &buf, &size, timeout.ticks);
    if (ret < 0) {
      printk("PDM read failed: %d", ret);
      size = 0;
    }
    return MemorySlabChunk(&slab_, buf, size);
  }

private:
  int ret;
  const struct device * device_;
  struct k_mem_slab slab_;
  char __aligned(4) buffer_[BlockSize * BlockCount];
  struct pcm_stream_cfg stream_;
  struct dmic_cfg cfg_;
};
