#pragma once

#include <expected>
#include <shared_mutex>
#include <mutex>
#include <string>
#include <unordered_map>
#include <atomic>

#include <glaze/glaze.hpp>
#include "VaultClient.h"

enum class VaultError
{
    EMPTY_REPLY,
    WRONG_BODY,
    HTTP_UNAUTHORIZED, // 401
    HTTP_FORBIDDEN, // 403
    RELOGIN_FAILED
};

struct VaultSession
{
    Vault::Config config;
    Vault::AuthenticationStrategy& auth;
    Vault::HttpErrorCallback http_error_cb;
    Vault::ResponseErrorCallback response_error_cb;

    std::atomic_int last_http_status{0};

    std::mutex mutex;
    std::shared_ptr<Vault::Client> client;
    std::unique_ptr<Vault::KeyValue> kv;
    Vault::SecretMount mount;

    VaultSession(Vault::Config cfg,
                 Vault::AuthenticationStrategy& auth_strategy,
                 Vault::HttpErrorCallback http_cb,
                 Vault::ResponseErrorCallback resp_cb,
                 Vault::SecretMount mnt = Vault::SecretMount{"kv"});

    bool relogin();
    std::optional<std::string> read(const Vault::Path& path);
};

class VaultCache
{
public:
    explicit VaultCache(VaultSession& session);

    std::expected<glz::generic, VaultError> read(const std::string& key);
    std::expected<glz::generic, VaultError> force_refresh(const std::string& key);

private:
    std::expected<glz::generic, VaultError>
    fetch_with_relogin_(const std::string& key, bool allow_relogin);

    VaultSession& session_;
    std::unordered_map<std::string, glz::generic> cache_;
    mutable std::shared_mutex mutex_;
};
