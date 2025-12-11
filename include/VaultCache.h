#pragma once
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "VaultClient.h"

enum VaultError
{
    EMPTY_REPLY,
    WRONG_BODY
};

class VaultCache
{
public:
    explicit VaultCache(Vault::KeyValue& kv) : kv_(kv)
    {
    }

    // читаем по строковому ключу
    std::expected<glz::generic, VaultError> read(const std::string& key)
    {
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
            return std::unexpected(EMPTY_REPLY);

        glz::generic json{};
        auto err = glz::read_json(json, resp.value());
        if (err) return std::unexpected(WRONG_BODY);

        auto [pos, _] = cache_.emplace(key, json.at("data").at("data"));
        return pos->second;
    }

    std::expected<glz::generic, VaultError> force_refresh(const std::string& key)
    {
        std::unique_lock lock(mutex_);

        Vault::Path path{key};
        auto resp = kv_.read(path);
        if (!resp)
            return std::unexpected(EMPTY_REPLY);

        glz::generic json{};
        auto err = glz::read_json(json, resp.value());
        if (err)
            return std::unexpected(WRONG_BODY);

        auto data = json.at("data").at("data");
        cache_[key] = data; // перезапись

        return data;
    }

private:
    Vault::KeyValue& kv_;
    std::unordered_map<std::string, glz::generic> cache_;
    mutable std::shared_mutex mutex_;
};