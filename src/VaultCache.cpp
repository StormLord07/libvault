#include "VaultCache.h"

static VaultError map_http_status(int code)
{
    if (code == 401) return VaultError::HTTP_UNAUTHORIZED;
    if (code == 403) return VaultError::HTTP_FORBIDDEN;
    return VaultError::EMPTY_REPLY;
}

VaultSession::VaultSession(Vault::Config cfg,
                           Vault::AuthenticationStrategy& auth_strategy,
                           Vault::HttpErrorCallback http_cb,
                           Vault::ResponseErrorCallback resp_cb,
                           Vault::SecretMount mnt)
    : config(std::move(cfg)),
      auth(auth_strategy),
      http_error_cb(std::move(http_cb)),
      response_error_cb(std::move(resp_cb)),
      mount(std::move(mnt))
{
    relogin();
}

bool VaultSession::relogin()
{
    std::lock_guard lk(mutex);

    last_http_status.store(0, std::memory_order_relaxed);

    client = std::make_shared<Vault::Client>(
        config, auth, http_error_cb, response_error_cb);

    if (!client || !client->is_authenticated())
        return false;

    kv = std::make_unique<Vault::KeyValue>(*client, mount);
    return true;
}

std::optional<std::string> VaultSession::read(const Vault::Path& path)
{
    std::lock_guard lk(mutex);
    last_http_status.store(0, std::memory_order_relaxed);
    return kv ? kv->read(path) : std::nullopt;
}

/* ================= VaultCache ================= */

VaultCache::VaultCache(VaultSession& session)
    : session_(session)
{
}

std::expected<glz::generic, VaultError>
VaultCache::fetch_with_relogin_(const std::string& key, bool allow_relogin)
{
    auto fetch_once = [&](const std::string& k)
        -> std::expected<glz::generic, VaultError>
    {
        Vault::Path path{k};
        auto resp = session_.read(path);

        if (!resp)
        {
            int code = session_.last_http_status.load(std::memory_order_relaxed);
            return std::unexpected(map_http_status(code));
        }

        glz::generic json{};
        if (auto err = glz::read_json(json, *resp); err)
        {
            return std::unexpected(VaultError::WRONG_BODY);
        }

        // KV v2: {"data":{"data":{...}}}
        auto data = json.at("data").at("data");
        cache_[k] = data;
        return data;
    };

    auto r = fetch_once(key);
    if (r) return r;

    if (allow_relogin &&
        (r.error() == VaultError::HTTP_UNAUTHORIZED ||
            r.error() == VaultError::HTTP_FORBIDDEN))
    {
        if (!session_.relogin())
            return std::unexpected(VaultError::RELOGIN_FAILED);

        return fetch_once(key); // retry ровно 1 раз
    }

    return r;
}

std::expected<glz::generic, VaultError>
VaultCache::read(const std::string& key)
{
    {
        std::shared_lock lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end())
            return it->second;
    }

    std::unique_lock lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end())
        return it->second;

    return fetch_with_relogin_(key, true);
}

std::expected<glz::generic, VaultError>
VaultCache::force_refresh(const std::string& key)
{
    std::unique_lock lock(mutex_);
    return fetch_with_relogin_(key, true);
}
