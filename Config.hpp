#pragma once
// Header only config loader.

#include <string>
#include <fstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Saved as a seperate struct here, as it must match the shader args struct exactly.
struct SimArgs {
    float diffA;
    float diffB;
    float feed;
    float kill;
};

struct Config {
  std::string name;
  int width;
  int height;
  SimArgs simArgs;
};

inline Config getConfig(std::string path, std::string configName) {
  std::ifstream f(path);
  json data = json::parse(f);
  Config config;
  config.name = configName;
  config.width = data[configName]["width"];
  config.height = data[configName]["height"];
  config.simArgs.diffA = data[configName]["diffA"];
  config.simArgs.diffB = data[configName]["diffB"];
  config.simArgs.feed = data[configName]["feed_rate"];
  config.simArgs.kill = data[configName]["kill_rate"];
  return config;
}

// Convenience for default config
inline Config getConfig() {
  return getConfig("pattern-confs/pearson.json", "coral");
}