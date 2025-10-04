// Hash table implementation
// Copyright (c) 2005-2008, Simon Howard
//
// Permission to use, copy, modify, and/or distribute this software
// for any purpose with or without fee is hereby granted, provided
// that the above copyright notice and this permission notice appear
// in all copies.

#ifndef HASH_TABLE_HPP
#define HASH_TABLE_HPP

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace olaf
{

/**
 * @class HashTable
 * @brief A modern C++17 hash table implementation
 */
template <typename Key, typename Value>
class HashTable
{
public:
  using HashFunc = std::function<unsigned int(const Key &)>;
  using EqualFunc = std::function<bool(const Key &, const Key &)>;
  using KeyValuePair = std::pair<Key, Value>;

private:
  struct Entry
  {
    KeyValuePair pair;
    std::unique_ptr<Entry> next;

    Entry(const Key & k, const Value & v) : pair(k, v), next(nullptr) {}
    Entry(Key && k, Value && v) : pair(std::move(k), std::move(v)), next(nullptr) {}
  };

  static constexpr std::array<unsigned int, 24> primes = {
    193,      389,      769,      1543,      3079,      6151,      12289,     24593,
    49157,    98317,    196613,   393241,    786433,    1572869,   3145739,   6291469,
    12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741};

  std::vector<std::unique_ptr<Entry>> table_;
  unsigned int table_size_;
  HashFunc hash_func_;
  EqualFunc equal_func_;
  unsigned int entries_;
  unsigned int prime_index_;

  void allocate_table()
  {
    if (prime_index_ < primes.size()) {
      table_size_ = primes[prime_index_];
    } else {
      table_size_ = entries_ * 10;
    }
    table_.resize(table_size_);
  }

  void enlarge()
  {
    auto old_table = std::move(table_);
    const unsigned int old_table_size = table_size_;

    ++prime_index_;
    allocate_table();

    for (unsigned int i = 0; i < old_table_size; ++i) {
      Entry * rover = old_table[i].get();

      while (rover != nullptr) {
        Entry * next = rover->next.get();
        const unsigned int index = hash_func_(rover->pair.first) % table_size_;

        // Move the entry to the new table
        auto moved_entry =
          std::make_unique<Entry>(std::move(rover->pair.first), std::move(rover->pair.second));
        moved_entry->next = std::move(table_[index]);
        table_[index] = std::move(moved_entry);

        rover = next;
      }
    }
  }

public:
  class Iterator
  {
  private:
    const HashTable * table_;
    unsigned int chain_index_;
    Entry * current_entry_;

    void find_next()
    {
      if (current_entry_ && current_entry_->next) {
        current_entry_ = current_entry_->next.get();
        return;
      }

      current_entry_ = nullptr;
      ++chain_index_;

      while (chain_index_ < table_->table_size_) {
        if (table_->table_[chain_index_]) {
          current_entry_ = table_->table_[chain_index_].get();
          break;
        }
        ++chain_index_;
      }
    }

  public:
    Iterator(const HashTable * table, unsigned int chain, Entry * entry)
    : table_(table), chain_index_(chain), current_entry_(entry)
    {
    }

    bool has_more() const { return current_entry_ != nullptr; }

    std::optional<KeyValuePair> next()
    {
      if (!current_entry_) {
        return std::nullopt;
      }

      KeyValuePair result = current_entry_->pair;
      find_next();
      return result;
    }

    KeyValuePair operator*() const { return current_entry_->pair; }

    Iterator & operator++()
    {
      find_next();
      return *this;
    }

    bool operator!=(const Iterator & other) const { return current_entry_ != other.current_entry_; }
  };

  HashTable(HashFunc hash_func, EqualFunc equal_func)
  : table_size_(0),
    hash_func_(std::move(hash_func)),
    equal_func_(std::move(equal_func)),
    entries_(0),
    prime_index_(0)
  {
    allocate_table();
  }

  ~HashTable() = default;

  // Delete copy operations
  HashTable(const HashTable &) = delete;
  HashTable & operator=(const HashTable &) = delete;

  // Allow move operations
  HashTable(HashTable &&) noexcept = default;
  HashTable & operator=(HashTable &&) noexcept = default;

  bool insert(const Key & key, const Value & value)
  {
    if ((entries_ * 3) / table_size_ > 0) {
      enlarge();
    }

    const unsigned int index = hash_func_(key) % table_size_;
    Entry * rover = table_[index].get();

    while (rover != nullptr) {
      if (equal_func_(rover->pair.first, key)) {
        rover->pair.second = value;
        return true;
      }
      rover = rover->next.get();
    }

    auto new_entry = std::make_unique<Entry>(key, value);
    new_entry->next = std::move(table_[index]);
    table_[index] = std::move(new_entry);

    ++entries_;
    return true;
  }

  bool insert(Key && key, Value && value)
  {
    if ((entries_ * 3) / table_size_ > 0) {
      enlarge();
    }

    const unsigned int index = hash_func_(key) % table_size_;
    Entry * rover = table_[index].get();

    while (rover != nullptr) {
      if (equal_func_(rover->pair.first, key)) {
        rover->pair.second = std::move(value);
        return true;
      }
      rover = rover->next.get();
    }

    auto new_entry = std::make_unique<Entry>(std::move(key), std::move(value));
    new_entry->next = std::move(table_[index]);
    table_[index] = std::move(new_entry);

    ++entries_;
    return true;
  }

  std::optional<Value> lookup(const Key & key) const
  {
    const unsigned int index = hash_func_(key) % table_size_;
    Entry * rover = table_[index].get();

    while (rover != nullptr) {
      if (equal_func_(key, rover->pair.first)) {
        return rover->pair.second;
      }
      rover = rover->next.get();
    }

    return std::nullopt;
  }

  bool remove(const Key & key)
  {
    const unsigned int index = hash_func_(key) % table_size_;

    if (!table_[index]) {
      return false;
    }

    if (equal_func_(key, table_[index]->pair.first)) {
      table_[index] = std::move(table_[index]->next);
      --entries_;
      return true;
    }

    Entry * rover = table_[index].get();
    while (rover->next) {
      if (equal_func_(key, rover->next->pair.first)) {
        rover->next = std::move(rover->next->next);
        --entries_;
        return true;
      }
      rover = rover->next.get();
    }

    return false;
  }

  unsigned int num_entries() const { return entries_; }

  Iterator begin() const
  {
    for (unsigned int i = 0; i < table_size_; ++i) {
      if (table_[i]) {
        return Iterator(this, i, table_[i].get());
      }
    }
    return end();
  }

  Iterator end() const { return Iterator(this, table_size_, nullptr); }
};

}  // namespace olaf

#endif  // HASH_TABLE_HPP
