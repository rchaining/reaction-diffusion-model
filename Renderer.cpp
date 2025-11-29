// We need to define these implementations in EXACTLY one .cpp file
#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include "Renderer.hpp"
#include <cmath>
#include <iostream>

Renderer::Renderer(MTL::Device *device, std::string confPath, std::string configName)
    : _device(device), _initialized(false) {
  _maxThreadGroupSize = 0;
  std::cout << "Loading config: " << configName << " from " << confPath << std::endl;
  _config = getConfig(confPath, configName);
  _device->retain();
  _commandQueue = _device->newCommandQueue();
  buildTextures();
  buildShaders();
}

Renderer::Renderer(MTL::Device *device)
    : _device(device), _initialized(false) {
  std::cout << "Loading default config" << std::endl;
  _config = getConfig();
  _maxThreadGroupSize = 0;
  _device->retain();
  _commandQueue = _device->newCommandQueue();
  buildShaders();
  buildTextures();
}

Renderer::~Renderer() {
  _commandQueue->release();
  _initPipelineState->release();
  _computePipelineState->release();
  _vizPipelineState->release();
  _device->release();
}

void Renderer::buildShaders() {
  std::cout << "Building shaders" << std::endl;
  // Load the library
  NS::Error *pError = nullptr;
  // Update path to match the Makefile's build directory
  MTL::Library *pLibrary = _device->newLibrary(
      NS::String::string("./build/default.metallib", NS::UTF8StringEncoding),
      &pError);
  
  if (!pLibrary) {
      // Try loading from current directory (for release/packaged builds)
      pLibrary = _device->newLibrary(
          NS::String::string("default.metallib", NS::UTF8StringEncoding),
          &pError);
  }

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

  // Initializer pipeline
  MTL::Function* initFn = pLibrary->newFunction(NS::String::string("init_simulation", NS::UTF8StringEncoding));
  _initPipelineState = _device->newComputePipelineState(initFn, &pError);
  if (!_initPipelineState) {
      std::cerr << "Failed to create init pipeline: " << pError->localizedDescription()->utf8String() << std::endl;
  }
  
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
  initFn->release();
  pLibrary->release();
  vizVertName->release();
  vizVertFn->release();
  vizFragName->release();
  vizFragFn->release();
  vizDesc->release();
}

void Renderer::texInitializerPass(MTL::CommandBuffer* cmdBuf) {
    MTL::ComputeCommandEncoder* initEncoder = cmdBuf->computeCommandEncoder();
    initEncoder->setComputePipelineState(_initPipelineState);
    initEncoder->setTexture(_simTexInput, 0);
    initEncoder->setBytes(&_config.simArgs, sizeof(SimArgs), 1);
    NS::UInteger w = _initPipelineState->threadExecutionWidth();
    NS::UInteger h = _initPipelineState->maxTotalThreadsPerThreadgroup() / w;
    MTL::Size threadgroupCount = MTL::Size::Make((_config.width + w - 1) / w,
                                                 (_config.height + h - 1) / h, 
                                                 1);
    MTL::Size threadGroupSize = MTL::Size::Make(w, h, 1);
    initEncoder->dispatchThreadgroups(threadgroupCount, threadGroupSize);
    initEncoder->endEncoding();
}

void Renderer::draw(CA::MetalLayer *layer) {
  CA::MetalDrawable *drawable = layer->nextDrawable();
  if (!drawable)
    return;
  MTL::CommandBuffer *cmdBuf = _commandQueue->commandBuffer();

  // Removed initializer pass. WIll add config option to choose initializer logic.
  // if(!_initialized) {
  //   std::cout << "First step, initializing simulation" << std::endl;
  //   texInitializerPass(cmdBuf);
  //   _initialized = true;
  // }

  // --- Compute ---
  // Set encoder and Input/Output texs
  for(int i = 0; i < _config.stepsPerFrame; i++) {
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

    // Swap textures for next frame
    std::swap(_simTexInput, _simTexOutput);
  }
  // --- Viz ---
  MTL::RenderPassDescriptor *vizPass = MTL::RenderPassDescriptor::renderPassDescriptor();
  vizPass->colorAttachments()->object(0)->setTexture(drawable->texture());
  vizPass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
  vizPass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
  vizPass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(.5, .5, .5, 1));
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
  std::cout << "Building textures" << std::endl;

  MTL::TextureDescriptor *simTexDesc =
      MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRG32Float,
                                                  _config.width,
                                                  _config.height, false);

  simTexDesc->setStorageMode(MTL::StorageModeManaged);
  
  simTexDesc->setUsage(MTL::TextureUsageShaderRead |
                       MTL::TextureUsageShaderWrite);

  _simTexInput = _device->newTexture(simTexDesc);
  _simTexOutput = _device->newTexture(simTexDesc);
  std::vector<float> seedData(_config.width * _config.height * 2);

  // Configurable "Density" for the noise (how much B to sprinkle)
  // 1% - 5% is usually good for Coral/Mazes
  float noiseDensity = _config.noiseDensity;

  for (int i = 0; i < seedData.size(); i += 2) {
      seedData[i] = 1.0f;
      float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
      if (r < noiseDensity) {
          seedData[i + 1] = 1.0f;
      } else {
          seedData[i + 1] = 0.0f;
      }
  }

  // Upload seed data to texture
  MTL::Region region = MTL::Region::Make2D(0, 0, _config.width, _config.height);
  NS::UInteger bytesPerRow = _config.width * 2 * sizeof(float);
  _simTexInput->replaceRegion(region, 0, seedData.data(), bytesPerRow);
  
  // Clean up
  simTexDesc->release();
}