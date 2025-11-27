#pragma once

// Config object for parsing the config from cli flags
// Keeping the scope to C++ so I didn't feel like finding an Objective C
// solution tbh
class Config {
private:
public:
  int w;
  int h;
  Config(int argc, char **argv);
};
