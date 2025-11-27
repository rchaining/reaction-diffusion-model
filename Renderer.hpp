#pragma once
#include "arg-config.hpp"
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp> // For CA::MetalLayer

class Renderer {
public:
  Renderer(MTL::Device *device);
  ~Renderer();

  void draw(CA::MetalLayer *layer);

private:
  MTL::Device *_device;
  MTL::CommandQueue *_commandQueue;
  MTL::RenderPipelineState *_pipelineState;
  MTL::RenderPipelineState *_postPipelineState; // Pipeline for pass 2
  MTL::DepthStencilState *_depthStencilState;

  MTL::Texture *_offscreenColorTexture; // Hold output of pass 1.
  MTL::Texture *_depthTexture;          // Cheat temp depth tex.
  MTL::Buffer *_vertexBuffer;
  int _vertexCount;

  float _angleDelta;
  float _angle;

  Config _config;

  void buildShaders();
  void buildBuffers();
  void buildFirstPassTex();
};