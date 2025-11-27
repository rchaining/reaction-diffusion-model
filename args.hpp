#pragma once
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

ABSL_FLAG(int, w, 600, "Width of the pattern tex");
ABSL_FLAG(int, h, 800, "Height of the pattern tex");

void doParse(int argc, char **argv){
  absl::ParseCommandLine(argc, argv);
}