// We need to define these implementations in EXACTLY one .cpp file
#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include "Renderer.hpp"
#include <cmath>
#include <iostream>


Renderer::Renderer(MTL::Device *device, std::string confPath, std::string configName)
    : _device(device) {
  _maxThreadGroupSize = 0;
  _config = getConfig(confPath, configName);
  // In C++, we need to retain objects we keep around
  _device->retain();
  _commandQueue = _device->newCommandQueue();
  buildShaders();
}

Renderer::Renderer(MTL::Device *device)
    : _device(device) {
  // In C++, we need to retain objects we keep around
  _device->retain();
  _commandQueue = _device->newCommandQueue();
  buildShaders();
}

Renderer::~Renderer() {
  _commandQueue->release();
  _computePipelineState->release();
  _vizPipelineState->release();
  _device->release();
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

  // Build compute pipeline
  // Functions
  NS::String *kernelName =
      NS::String::string("sim_main", NS::UTF8StringEncoding);
  MTL::Function *simFn = pLibrary->newFunction(kernelName);

  // descriptor and pipeline state
  _computePipelineState = _device->newComputePipelineState(simFn, &pError);
  if (!_computePipelineState) {
    std::cerr << "Failed to create compute pipeline state: "
              << pError->localizedDescription()->utf8String() << std::endl;
  }
  _maxThreadGroupSize = _computePipelineState->maxTotalThreadsPerThreadgroup();

  // Build viz pipeline
  // Functions
  NS::String *vizVertName =
      NS::String::string("full_screen_tri", NS::UTF8StringEncoding);
  MTL::Function *vizVertFn = pLibrary->newFunction(vizVertName);
  NS::String *vizFragName =
      NS::String::string("sim_visualizer", NS::UTF8StringEncoding);
  MTL::Function *vizFragFn = pLibrary->newFunction(vizFragName);
  
  // descriptor and pipeline state
  MTL::RenderPipelineDescriptor *vizDesc =
      MTL::RenderPipelineDescriptor::alloc()->init();
  vizDesc->setVertexFunction(vizVertFn);
  vizDesc->setFragmentFunction(vizFragFn);
  vizDesc->colorAttachments()->object(0)->setPixelFormat(
      MTL::PixelFormat::PixelFormatBGRA8Unorm);
  _vizPipelineState = _device->newRenderPipelineState(vizDesc, &pError);
  if (!_vizPipelineState) {
    std::cerr << "Failed to create viz pipeline state: "
              << pError->localizedDescription()->utf8String() << std::endl;
  }
  
  // Cleanup
  pLibrary->release();
  vizVertName->release();
  vizVertFn->release();
  vizFragName->release();
  vizFragFn->release();
  vizDesc->release();
}

void Renderer::draw(CA::MetalLayer *layer) {
  CA::MetalDrawable *drawable = layer->nextDrawable();
  if (!drawable)
    return;
  MTL::CommandBuffer *cmdBuf = _commandQueue->commandBuffer();
  // --- Compute ---
  // Set encoder and Input/Output texs
  MTL::ComputeCommandEncoder *computeEncoder = cmdBuf->computeCommandEncoder();
  computeEncoder->setComputePipelineState(_computePipelineState); // Set the compute pipeline state

  computeEncoder->setTexture(_simTexture1, 0); // Input (Read)
  computeEncoder->setTexture(_simTexture2, 1); // Output (Write)

  // Set distpatch
  MTL::Size gridSize = MTL::Size::Make(_config.width, _config.height, 1);
  int maxSize = 16 < _maxThreadGroupSize ? 16 : _maxThreadGroupSize;
  MTL::Size threadGroupSize = MTL::Size::Make(maxSize, maxSize, 1); // TODO: Make this dynamic based on max thread group size
  computeEncoder->dispatchThreadgroups(gridSize, threadGroupSize);
  computeEncoder->endEncoding();

  // --- Viz ---
  // Set encoder, set tex
  MTL::RenderPassDescriptor *vizPass = MTL::RenderPassDescriptor::renderPassDescriptor();
  vizPass->colorAttachments()->object(0)->setTexture(drawable->texture());
  vizPass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
  vizPass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
  vizPass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0, 0, 0, 1));
  MTL::RenderCommandEncoder *vizEncoder = cmdBuf->renderCommandEncoder(vizPass);

  // Set pipeline and draw
  vizEncoder->setRenderPipelineState(_vizPipelineState);
  vizEncoder->setFragmentTexture(_simTexture2, 0);
  vizEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, (NS::UInteger)0, (NS::UInteger)3);
  vizEncoder->endEncoding();

  // --- Commit ---
  cmdBuf->presentDrawable(drawable);
  cmdBuf->commit();
}

void Renderer::buildTextures() {
  // Build simulation textures
  // For now, do not bother initializing.

  MTL::TextureDescriptor *simTexDesc =
      MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm,
                                                  _config.width,
                                                  _config.height, false);
  simTexDesc->setUsage(MTL::TextureUsageRenderTarget | // What usage settings for the sim textures?
                       MTL::TextureUsageShaderRead);

  simTexDesc->setStorageMode(MTL::StorageModePrivate); // Set private in hopes I can keep the whole thing in the GPU.
  _simTexture1 = _device->newTexture(simTexDesc);
  _simTexture2 = _device->newTexture(simTexDesc);
  simTexDesc->release();
}