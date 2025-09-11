#include "VaultClient.h"
#include <glaze/glaze.hpp>

std::optional<Vault::AuthenticationResponse>
Vault::JwtStrategy::authenticate(const Vault::Client &client) {
  return HttpConsumer::authenticate(
      client, getUrl(client, Vault::Path{"/login"}), [this]() {
        Parameters j;
        j["role"] = role_.value();
        j["jwt"] = jwt_.value();
        return j.dump().value_or("{}");
      });
}

Vault::Url Vault::JwtStrategy::getUrl(const Vault::Client &client,
                                      const Vault::Path &path) {
  return client.getUrl("/v1/auth/" + mount_, path);
}
