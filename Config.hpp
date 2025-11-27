#pragma once

#include <string>
#include <fstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

json getConfig(std::string path) {
  std::ifstream f(path);
  json data = json::parse(f);
  return data;
}

// Convenience for default config
json getConfig() {
  return getConfig("config.json");
}