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

Renderer::Renderer(MTL::Device *device)
    : _device(device), _angle(0.0f), _angleDelta(angleChange) {
  // In C++, we need to retain objects we keep around
  _device->retain();
  _commandQueue = _device->newCommandQueue();
  buildShaders();
  buildBuffers();
  buildFirstPassTex();
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
  desc->setDepthAttachmentPixelFormat(
      MTL::PixelFormat::PixelFormatDepth32Float);

  NS::Error *error = nullptr;
  _pipelineState = _device->newRenderPipelineState(desc, &error);
  if (!_pipelineState) {
    std::cerr << "Failed to create pipeline state: "
              << error->localizedDescription()->utf8String() << std::endl;
  }

  // Create Depth/Stencil State
  MTL::DepthStencilDescriptor *depthDesc =
      MTL::DepthStencilDescriptor::alloc()->init();
  depthDesc->setDepthCompareFunction(
      MTL::CompareFunctionLess); // "Only draw if closer"
  depthDesc->setDepthWriteEnabled(
      true); // "Update the depth buffer when drawing"
  _depthStencilState = _device->newDepthStencilState(depthDesc);

  // Post-Process Pipeline
  // Fns
  NS::String *postVertName =
      NS::String::string("post_vertex_main", NS::UTF8StringEncoding);
  NS::String *postFragName =
      NS::String::string("post_fragment_main", NS::UTF8StringEncoding);
  MTL::Function *postVertFn = pLibrary->newFunction(postVertName);
  MTL::Function *postFragFn = pLibrary->newFunction(postFragName);

  // Descriptor
  MTL::RenderPipelineDescriptor *postDesc =
      MTL::RenderPipelineDescriptor::alloc()->init();
  postDesc->setVertexFunction(postVertFn);
  postDesc->setFragmentFunction(postFragFn);
  postDesc->colorAttachments()->object(0)->setPixelFormat(
      MTL::PixelFormatBGRA8Unorm);
  postDesc->setDepthAttachmentPixelFormat(
      MTL::PixelFormatInvalid); // Post-process does not use depth testing, b/c
                                // flat image

  _postPipelineState = _device->newRenderPipelineState(postDesc, &error);
  // Cleanup
  depthDesc->release();
  vertexFn->release();
  fragFn->release();
  desc->release();
  pLibrary->release();
  vertexName->release();
  fragName->release();
  postVertName->release();
  postFragName->release();
  postVertFn->release();
  postFragFn->release();
  postDesc->release();
}

void Renderer::buildBuffers() {
  // monke.obj should be in the same folder as the executable
  // std::vector<Vertex> mesh = MeshLoader::loadObj("monke.obj");
  // _vertexCount = mesh.size();
  // size_t dataSize = mesh.size() * sizeof(Vertex);
  // Create GPU buffer
  // MTLResourceStorageModeShared = CPU writes, GPU reads
  _vertexBuffer = _device->newBuffer(dataSize, MTL::ResourceStorageModeShared);
  // Copy data from C++ Vector to Metal Buffer
  // memcpy(_vertexBuffer->contents(), mesh.data(), dataSize);
}

// Helper for math
Uniforms makeRotation(float angleRadians) {
  float c = cos(angleRadians);
  float s = sin(angleRadians);
  Uniforms u;

  // Initialize identity matrix
  // For loop will output:
  // [[1, 0, 0, 0],
  //  [0, 1, 0, 0],
  //  [0, 0, 1, 0],
  //  [0, 0, 0, 1]]
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      u.rotationMatrix[i][j] = (i == j ? 1.0f : 0.0f);
  // Then make it into a rotation matrix:
  u.rotationMatrix[0][0] = c;
  u.rotationMatrix[0][1] = s;
  u.rotationMatrix[1][0] = -s;
  u.rotationMatrix[1][1] = c;
  return u;
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
  enc1->setDepthStencilState(_depthStencilState);
  _angle += _angleDelta;
  Uniforms u = makeRotation(_angle);

  enc1->setVertexBuffer(_vertexBuffer, 0, 0);
  enc1->setVertexBytes(&u, sizeof(u), 1);
  enc1->drawPrimitives(MTL::PrimitiveTypeTriangle, (NS::UInteger)0,
                       (NS::UInteger)_vertexCount);
  enc1->endEncoding();

  // Pass 2 (post-processor): Render fullscreen quad using results from Pass 1
  MTL::RenderPassDescriptor *pass2 =
      MTL::RenderPassDescriptor::renderPassDescriptor();

  // Output tex to the real screen (drawable)
  pass2->colorAttachments()->object(0)->setTexture(drawable->texture());
  pass2->colorAttachments()->object(0)->setLoadAction(
      MTL::LoadActionDontCare); // Overwriting anyway
  pass2->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

  // Encode pass 2
  MTL::RenderCommandEncoder *enc2 = cmdBuf->renderCommandEncoder(pass2);
  enc2->setRenderPipelineState(_postPipelineState);
  // Inputs (the textures from pass 1)
  enc2->setFragmentTexture(_offscreenColorTexture, 0);
  enc2->setFragmentTexture(_depthTexture, 1);
  // Draw 3 vertices (The shader generates the fullscreen triangle coordinates
  // automatically)
  enc2->drawPrimitives(MTL::PrimitiveTypeTriangle, (NS::UInteger)0,
                       (NS::UInteger)3);
  enc2->endEncoding();
  // --- Commit ---
  cmdBuf->presentDrawable(drawable);
  cmdBuf->commit();
}

void Renderer::buildFirstPassTex() {
  // Depth texture
  // Match window size -- make configurable or read window size dynamically
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