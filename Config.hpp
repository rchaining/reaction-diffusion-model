#pragma once
// Header only config loader.

#include <string>
#include <fstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Saved as a seperate struct here, as it must match the shader args struct exactly.
struct SimArgs {
  // Initial perlin noise vars
  float frequency;
  float scale;

  // Simulation variables
  float diffA;
  float diffB;
  float feed;
  float kill;
  float timeStep;
};

struct Config {
  float noiseDensity;
  int stepsPerFrame;
  std::string name;
  int width;
  int height;
  SimArgs simArgs;
};

inline Config getConfig(std::string path, std::string configName) {
  std::ifstream f(path);
  json data = json::parse(f);
  Config config;
  // Global config values
  config.name = configName;
  config.width = data["width"];
  config.height = data["height"];
  config.stepsPerFrame = data["steps_per_frame"];
  config.noiseDensity = data["noise_density"];
  // Simulations specific overrides for global confs
  if (data[configName].contains("noise_density")) {
    config.noiseDensity = data[configName]["noise_density"];
  } 
  if (data[configName].contains("steps_per_frame")) {
    config.stepsPerFrame = data[configName]["steps_per_frame"];
  }
  // Simulation args
  config.simArgs.frequency = data[configName]["frequency"];
  config.simArgs.scale = data[configName]["scale"];
  config.simArgs.diffA = data[configName]["diffA"];
  config.simArgs.diffB = data[configName]["diffB"];
  config.simArgs.feed = data[configName]["feed_rate"];
  config.simArgs.kill = data[configName]["kill_rate"];
  config.simArgs.timeStep = data["time_step"];
  return config;
}

// Convenience for default config
inline Config getConfig() {
  return getConfig("pattern-confs/pearson.json", "coral");
}