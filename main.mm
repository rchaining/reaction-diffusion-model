#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

// Include our C++ Renderer
#include "Renderer.hpp"

const int WIDTH = 1000;
const int HEIGHT = 1000;


int main(int argc, char** argv) {
    std::string confPath = "pattern-confs/pearson.json";
    std::string configName;
    if (argc > 1) {
        configName = argv[1];
    } else {
        configName = "coral";
    }
    NSApplication* app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];

    // ... (Window Setup Code is same as before) ...
    NSRect frame = NSMakeRect(0, 0, WIDTH, HEIGHT);
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window setTitle:@"Hello Metal CPP"];

    // 1. Create Device (Obj-C)
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    
    // 2. Create Layer (Obj-C)
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    metalLayer.device = device;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    window.contentView.layer = metalLayer;
    window.contentView.wantsLayer = YES;
    
    // 3. Create C++ Renderer
    // BRIDGE CAST: (__bridge void*) casts the Obj-C pointer to a C pointer
    MTL::Device* cppDevice = (__bridge MTL::Device*)device;
    Renderer* renderer = new Renderer(cppDevice, confPath, configName);

    [window makeKeyAndOrderFront:nil];
    [app activateIgnoringOtherApps:YES];
    [app finishLaunching];

    while (true) {
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        if (![window isVisible]){ break; }
        NSEvent* event;
        while ((event = [app nextEventMatchingMask:NSEventMaskAny 
                                         untilDate:nil 
                                            inMode:NSDefaultRunLoopMode 
                                           dequeue:YES])) {
            [app sendEvent:event];
            [app updateWindows];
        }
        
        // BRIDGE CAST: Pass the layer to C++
        CA::MetalLayer* cppLayer = (__bridge CA::MetalLayer*)metalLayer;
        renderer->draw(cppLayer);
        
        [pool release];
    }
    
    delete renderer;
    return 0;
}