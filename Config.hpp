#pragma once

#include <string>
#include <fstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

struct SimArgs {
    int width;
    int height;
    float diffA;
    float diffB;
    float feed;
    float kill;
};

struct Config {
  std::string name;
  SimArgs simArgs;
};

Config getConfig(std::string path, std::string configName) {
  std::ifstream f(path);
  json data = json::parse(f);
  Config config;
  config.name = configName;
  config.simArgs.width = data[configName]["width"];
  config.simArgs.height = data[configName]["height"];
  config.simArgs.diffA = data[configName]["diffA"];
  config.simArgs.diffB = data[configName]["diffB"];
  config.simArgs.feed = data[configName]["feed_rate"];
  config.simArgs.kill = data[configName]["kill_rate"];
  return config;
}

// Convenience for default config
Config getConfig() {
  return getConfig("pattern-confs/pearson.json", "coral");
}