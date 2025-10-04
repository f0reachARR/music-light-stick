#pragma once

#include <zephyr/kernel.h>

class MemorySlabChunk
{
public:
  MemorySlabChunk(struct k_mem_slab * slab, void * buffer, size_t size)
  : slab_(slab), buffer_(buffer), size_(size)
  {
  }

  ~MemorySlabChunk()
  {
    if (slab_ && buffer_ && size_ > 0) k_mem_slab_free(slab_, buffer_);
  }

  template <typename T>
  T * as()
  {
    return static_cast<T *>(buffer_);
  }

  size_t size() const { return size_; }

private:
  struct k_mem_slab * slab_;
  void * buffer_;
  size_t size_;
};
