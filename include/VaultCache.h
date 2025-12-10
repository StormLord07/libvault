#pragma once
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "VaultClient.h"

class VaultCache {
public:
  explicit VaultCache(Vault::KeyValue &kv) : kv_(kv) {}

  // читаем по строковому ключу
  std::optional<std::string> read(const std::string &key) {
    // быстрая дорожка: только shared-lock
    {
      std::shared_lock lock(mutex_);
      auto it = cache_.find(key);
      if (it != cache_.end())
        return it->second;
    }

    // промах — эксклюзивный lock + повторная проверка
    std::unique_lock lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end())
      return it->second;

    // читаем из Vault
    Vault::Path path{key};
    auto resp = kv_.read(path);
    if (!resp)
      return std::nullopt;

    std::string value = resp.value(); // если тип другой — приведи тут

    auto [pos, _] = cache_.emplace(key, std::move(value));
    return pos->second;
  }

private:
  Vault::KeyValue &kv_;
  std::unordered_map<std::string, std::string> cache_;
  mutable std::shared_mutex mutex_;
};
