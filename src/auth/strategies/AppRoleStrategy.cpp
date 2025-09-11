#include "VaultClient.h"
#include <glaze/glaze.hpp>

std::optional<Vault::AuthenticationResponse>
Vault::AppRoleStrategy::authenticate(const Vault::Client &client) {
  return Vault::HttpConsumer::authenticate(
      client, getUrl(client, Vault::Path{"/login"}), [&]() {
        Parameters j;
        j = nlohmann::json::object();
        j["role_id"] = roleId_.value();
        j["secret_id"] = secretId_.value();
        return j.dump().value_or("{}");
      });
}

Vault::Url Vault::AppRoleStrategy::getUrl(const Vault::Client &client,
                                          const Vault::Path &path) {
  return client.getUrl("/v1/auth/" + mount_, path);
}
