#pragma once
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "VaultClient.h"

enum VaultError { EMPTY_REPLY, WRONG_BODY };

class VaultCache {
public:
  explicit VaultCache(Vault::KeyValue &kv);

  // читаем по строковому ключу
  std::expected<glz::generic, VaultError> read(const std::string &key);

  std::expected<glz::generic, VaultError> force_refresh(const std::string &key);

private:
  Vault::KeyValue &kv_;
  std::unordered_map<std::string, glz::generic> cache_;
  mutable std::shared_mutex mutex_;
};