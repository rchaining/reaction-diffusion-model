# Reaction Diffusion Model

A GPU-accelerated implementation of the [Gray-Scott Reaction-Diffusion model](https://en.wikipedia.org/wiki/Turing_pattern) built for macOS using C++, Objective-C++, and Metal.

I can't confirm this will work on other MacOSs -- I haven't ironed out all the GPU stuff yet, so some of the hardcoded values may be specific to my Apple Silicon chip.
It almost certianly won't work on Intel Macs.

This is a learning project to get familiar with Compute Shaders using Metal. Shaders.metal contains the shader code. The GPU pipeline computes the simulation steps each frame and then a visualizer fragment shader converts the simulation state into a color rendered to the window.

![Coral Simulation Pattern](/img/coral.png)
*Figure 1: Coral pattern simulation. Generated with `./ReactionDiffusionModel coral`*

## Overview

This application simulates the interaction of two chemical species ($A$ and $B$) using the Gray-Scott equations. It utilizes Apple's Metal API for high-performance compute shaders to calculate diffusion, feed, and kill rates in real-time.

**Key Features:**
* Real-time simulation using Metal Compute Shaders.
* Configurable simulation parameters via JSON.
* Visualization of chemical concentrations.

![Worms Simulation Pattern](/img/worms.png)
*Figure 2: Worms pattern simulation.*

## Prerequisites

* macOS (Requires Metal support)
* CMake (3.25 or higher)
* Xcode Command Line Tools

## Building

You can build the project using the provided helper script or manually via CMake.

### Using Release Script
```bash
./release.sh
```

This will create a release/ directory containing the packaged application.

### Manual Build
```bash
mkdir build
cd build
cmake ..
make
```

### Usage
Run the executable from the build directory. You can specify a configuration pattern name as an argument.
```bash
./ReactionDiffusionModel [pattern_name]
```

### Example
```bash
# Runs the default "coral" pattern
./ReactionDiffusionModel

# Runs the "mitosis" pattern (if defined in your config)
./ReactionDiffusionModel mitosis
```

### Configuration
Simulation parameters are defined in pattern-confs/pearson.json. The application parses this file to set initial conditions and reaction rates.

Configurable Parameters:
- feed_rate ($F$): Rate at which chemical A is added.
- kill_rate ($K$): Rate at which chemical B is removed.
- diffA / diffB: Diffusion rates for species A and B.
- time_step: ~~Simulation speed.~~ Simulation accuracy
- steps_per_frame: Number of steps to take per frame. Effectively controls simulation speed.
- noise_density: Initial random distribution density.

noise_density and steps_per_frame can be configured globally, or independent to the pattern. The parser defaults to the global setting if the pattern does not define a value.

## License
This project relies on metal-cpp and nlohmann/json. Please refer to their respective licenses in the metal-cpp folder and build cache.