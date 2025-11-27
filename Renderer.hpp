#pragma once
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp> // For CA::MetalLayer
#include <vector>
#include "Config.hpp"

class Renderer {
public:
  Renderer(MTL::Device *device);
  ~Renderer();

  void draw(CA::MetalLayer *layer);

private:
  MTL::Device *_device;
  MTL::CommandQueue *_commandQueue;
  MTL::RenderPipelineState *_pipelineState;

  MTL::Texture *_simTexture1;
  MTL::Texture *_simTexture2;

  Config _config;

  void buildShaders();
  void buildTextures();
};