// We need to define these implementations in EXACTLY one .cpp file
#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include "Renderer.hpp"
#include <cmath>
#include <iostream>

// /10 to slow it way down while I'm fiddling.
const float angleChange = 0.05f / 10.0f;

struct Uniforms {
  float rotationMatrix[4][4];
};

Renderer::Renderer(MTL::Device *device, std::string confPath, std::string configName)
    : _device(device), _angle(0.0f), _angleDelta(angleChange) {
  _config = getConfig(confPath, configName);
  // In C++, we need to retain objects we keep around
  _device->retain();
  _commandQueue = _device->newCommandQueue();
  buildShaders();
  buildBuffers();
}

Renderer::Renderer(MTL::Device *device)
    : _device(device), _angle(0.0f), _angleDelta(angleChange), _config(getConfig()) {
  // In C++, we need to retain objects we keep around
  _device->retain();
  _commandQueue = _device->newCommandQueue();
  buildShaders();
  buildBuffers();
}

Renderer::~Renderer() {
  _vertexBuffer->release();
  _commandQueue->release();
  _pipelineState->release();
  _device->release();
  _depthStencilState->release();
  _offscreenColorTexture->release();
  _depthTexture->release();
}

void Renderer::buildShaders() {
  // Load the library
  NS::Error *pError = nullptr;
  // Update path to match the Makefile's build directory
  MTL::Library *pLibrary = _device->newLibrary(
      NS::String::string("./build/default.metallib", NS::UTF8StringEncoding),
      &pError);
  if (!pLibrary) {
    __builtin_printf("%s", pError->localizedDescription()->utf8String());
    assert(false);
  }

  // Build functions
  NS::String *vertexName =
      NS::String::string("vertex_main", NS::UTF8StringEncoding);
  NS::String *fragName =
      NS::String::string("fragment_main", NS::UTF8StringEncoding);
  MTL::Function *vertexFn = pLibrary->newFunction(vertexName);
  MTL::Function *fragFn = pLibrary->newFunction(fragName);

  MTL::RenderPipelineDescriptor *desc =
      MTL::RenderPipelineDescriptor::alloc()->init();
  desc->setVertexFunction(vertexFn);
  desc->setFragmentFunction(fragFn);
  desc->colorAttachments()->object(0)->setPixelFormat(
      MTL::PixelFormat::PixelFormatBGRA8Unorm);

  NS::Error *error = nullptr;
  _pipelineState = _device->newRenderPipelineState(desc, &error);
  if (!_pipelineState) {
    std::cerr << "Failed to create pipeline state: "
              << error->localizedDescription()->utf8String() << std::endl;
  }

  // Cleanup
  vertexFn->release();
  fragFn->release();
  desc->release();
  pLibrary->release();
  vertexName->release();
  fragName->release();
}

void Renderer::draw(CA::MetalLayer *layer) {
  CA::MetalDrawable *drawable = layer->nextDrawable();
  if (!drawable)
    return;

  MTL::CommandBuffer *cmdBuf = _commandQueue->commandBuffer();

  // Pass 1: Render object and depth to offscreen tex
  MTL::RenderPassDescriptor *pass1 =
      MTL::RenderPassDescriptor::renderPassDescriptor();
  // Set color
  pass1->colorAttachments()->object(0)->setTexture(_offscreenColorTexture);
  pass1->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
  pass1->colorAttachments()->object(0)->setClearColor(
      MTL::ClearColor::Make(0.1, 0.1, 0.1, 1));
  pass1->colorAttachments()->object(0)->setStoreAction(
      MTL::StoreActionStore); // Save for Pass 2!
  // Set depth
  pass1->depthAttachment()->setTexture(_depthTexture);
  pass1->depthAttachment()->setLoadAction(MTL::LoadActionClear);
  pass1->depthAttachment()->setStoreAction(
      MTL::StoreActionStore); // Save for Pass 2!
  pass1->depthAttachment()->setClearDepth(1.0);
  // Set uniforms and encode first pass
  MTL::RenderCommandEncoder *enc1 = cmdBuf->renderCommandEncoder(pass1);
  enc1->setRenderPipelineState(_pipelineState);
  enc1->drawPrimitives(MTL::PrimitiveTypeTriangle, (NS::UInteger)0,
                       (NS::UInteger)3);
  enc1->endEncoding();

  // --- Commit ---
  cmdBuf->presentDrawable(drawable);
  cmdBuf->commit();
}

void Renderer::buildTextures() {
  // Depth texture
  int l = 1000, w = 1000;
  MTL::TextureDescriptor *depthDesc =
      MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatDepth32Float,
                                                  l, w, false);
  depthDesc->setUsage(MTL::TextureUsageRenderTarget);
  depthDesc->setStorageMode(MTL::StorageModePrivate); // GPU only
  _depthTexture = _device->newTexture(depthDesc);
  depthDesc->release();

  // First pass texture
  MTL::TextureDescriptor *colorDesc =
      MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, l,
                                                  w, false);
  colorDesc->setUsage(MTL::TextureUsageRenderTarget |
                      MTL::TextureUsageShaderRead); // Allow Reading!
  colorDesc->setStorageMode(MTL::StorageModePrivate);
  _offscreenColorTexture = _device->newTexture(colorDesc);
  colorDesc->release();
}