// We need to define these implementations in EXACTLY one .cpp file
#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include "Renderer.hpp"
#include <cmath>
#include <iostream>

std::vector<float> buildTexSeed(uint width, uint height) {
  std::vector<float> seedData(width * height * 2);
  for (uint i = 0; i < width * height; i++) {
    seedData[i * 2] = rand() % 256;
    seedData[i * 2 + 1] = rand() % 256;
  }
  return seedData;
}

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

  computeEncoder->setTexture(_simTexInput, 0);
  computeEncoder->setTexture(_simTexOutput, 1);
  computeEncoder->setBytes(&_config.simArgs, sizeof(SimArgs), 3);
  
  // Set distpatch
  NS::UInteger w = _computePipelineState->threadExecutionWidth();
  NS::UInteger h = _computePipelineState->maxTotalThreadsPerThreadgroup() / w;
  MTL::Size threadgroupCount;
  threadgroupCount.width  = (_config.width  + w - 1) / w;
  threadgroupCount.height = (_config.height + h - 1) / h;
  threadgroupCount.depth  = 1;
  MTL::Size threadGroupSize = MTL::Size::Make(w, h, 1);
  computeEncoder->dispatchThreadgroups(threadgroupCount, threadGroupSize);
  computeEncoder->endEncoding();

  // --- Viz ---
  MTL::RenderPassDescriptor *vizPass = MTL::RenderPassDescriptor::renderPassDescriptor();
  vizPass->colorAttachments()->object(0)->setTexture(drawable->texture());
  vizPass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
  vizPass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
  vizPass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0, 0, 0, 1));
  MTL::RenderCommandEncoder *vizEncoder = cmdBuf->renderCommandEncoder(vizPass);
  vizEncoder->setRenderPipelineState(_vizPipelineState);
  vizEncoder->setFragmentTexture(_simTexOutput, 0);
  vizEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, (NS::UInteger)0, (NS::UInteger)3);
  vizEncoder->endEncoding();

  // --- Commit ---
  cmdBuf->presentDrawable(drawable);
  cmdBuf->commit();
}

void Renderer::buildTextures() {
  // Build simulation textures
  MTL::TextureDescriptor *simTexDesc =
      MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRG32Float,
                                                  _config.width,
                                                  _config.height, false);
  simTexDesc->setStorageMode(MTL::StorageModeShared); // Shared so I can upload an intial texture for now.
  simTexDesc->setUsage(MTL::TextureUsageRenderTarget | // What usage settings for the sim textures?
                       MTL::TextureUsageShaderWrite);
  _simTexInput = _device->newTexture(simTexDesc);
  _simTexOutput = _device->newTexture(simTexDesc);

  // Seed the input tex
  std::vector<float> seedData = buildTexSeed(_config.width, _config.height);
  MTL::Region region = MTL::Region::Make2D(0, 0, _config.width, _config.height);
  NS::UInteger bytesPerRow = _config.width * 2 * sizeof(float);
  _simTexInput->replaceRegion(region, 0, seedData.data(), bytesPerRow);
  
  simTexDesc->release();
}