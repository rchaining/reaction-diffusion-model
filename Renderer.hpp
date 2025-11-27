#pragma once
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp> // For CA::MetalLayer
#include <vector>
#include "Config.hpp"

class Renderer {
public:
  Renderer(MTL::Device *device, std::string confPath, std::string configName);
  Renderer(MTL::Device *device);
  ~Renderer();

  void draw(CA::MetalLayer *layer);

private:
  MTL::Device *_device;
  MTL::CommandQueue *_commandQueue;
  MTL::ComputePipelineState *_computePipelineState;
  MTL::RenderPipelineState *_vizPipelineState;

  NS::UInteger _maxThreadGroupSize;
  MTL::Texture *_simTexture1;
  MTL::Texture *_simTexture2; // TODO: Rename to simOutputTexture, since we always swap them on CPU.

  Config _config;

  void buildShaders();
  void buildTextures();
};