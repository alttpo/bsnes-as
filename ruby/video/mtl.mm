
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

struct VideoMTL;


// this window is for full-screen only:
@interface RubyWindowMTL : NSWindow <NSWindowDelegate> {
@public
  VideoMTL* video;
}
-(id) initWith:(VideoMTL*)video;
-(BOOL) canBecomeKeyWindow;
-(BOOL) canBecomeMainWindow;
@end


@interface RubyMetalRenderer : NSObject <MTKViewDelegate> {
@public
  VideoMTL* video;
  MTKView*  view;

  id<MTLDevice>       device;
  id<MTLCommandQueue> commandQueue;

  id<MTLRenderPipelineState> pipelineState;
  id<MTLBuffer> vertexBuffer;
  id<MTLTexture> currentTexture;
  id<MTLSamplerState> samplerState;
}
@property bool blocking;
-(id)   initWith:(VideoMTL*)video mtkView:(MTKView*)mtkView;
-(void) drawInMTKView:(MTKView *) view;
-(void) mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size;
@end


struct VideoMTL : VideoDriver {
  VideoMTL& self = *this;
  VideoMTL(Video& super) : VideoDriver(super) {}
  ~VideoMTL() { terminate(); }

  auto create() -> bool override {
    return initialize();
  }

  auto driver() -> string override { return "Metal"; }
  auto ready() -> bool override { return _ready; }

  auto hasFullScreen() -> bool override { return true; }
  auto hasContext() -> bool override { return true; }
  auto hasBlocking() -> bool override { return true; }
  auto hasFlush() -> bool override { return true; }
  auto hasShader() -> bool override { return true; }

  auto setFullScreen(bool fullScreen) -> bool override {
    return initialize();
  }

  auto setContext(uintptr context) -> bool override {
    return initialize();
  }

  auto setBlocking(bool blocking) -> bool override {
    if(!view) return true;
    if(!renderer) return true;
    [renderer setBlocking:blocking];
    return true;
  }

  auto setFlush(bool flush) -> bool override {
    return true;
  }

  auto setShader(string shader) -> bool override {
    // TODO
    return true;
  }

  auto focused() -> bool override {
    return true;
  }

  auto clear() -> void override {
    @autoreleasepool {
      [view lockFocus];
      // TODO
      [view unlockFocus];
    }
  }

  auto size(uint& width, uint& height) -> void override {
    @autoreleasepool {
      auto area = [view convertRectToBacking:[view bounds]];
      width = area.size.width;
      height = area.size.height;
    }
  }

  auto acquire(uint32_t*& data, uint& pitch, uint width, uint height) -> bool override {
    if (width <= 0 || height <= 0) {
      if (_buffer) delete[] _buffer;
      return false;
    }

    if (width != _width || height != _height) {
      if (_buffer) delete[] _buffer;
      _buffer = new uint32_t[width * height];
      _width = width;
      _height = height;
    }

    data = _buffer;
    pitch = width * sizeof(uint32_t);
    return true;
  }

  auto release() -> void override {
    @autoreleasepool {
      [renderer uploadTexture:_buffer width:_width height:_height];
    }
  }

  auto output(uint width, uint height) -> void override {
    // uint windowWidth, windowHeight;
    // size(windowWidth, windowHeight);

    @autoreleasepool {
      if([view lockFocusIfCanDraw]) {
        [view draw];

        [view unlockFocus];
      }
    }
  }

private:
  auto initialize() -> bool {
    terminate();
    if(!self.fullScreen && !self.context) return false;

    @autoreleasepool {
      if(self.fullScreen) {
        window = [[RubyWindowMTL alloc] initWith:this];
        [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
        [window toggleFullScreen:nil];
      //[NSApp setPresentationOptions:NSApplicationPresentationFullScreen];
      }

      auto context = self.fullScreen ? [window contentView] : (NSView*)self.context;
      auto size = [context frame].size;

      auto device = MTLCreateSystemDefaultDevice();

      view = [[MTKView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height) device:device];
      view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
      view.autoResizeDrawable = true;
      view.colorPixelFormat = MTLPixelFormatRGBA8Unorm;

      [context addSubview:view];
      [[view window] makeFirstResponder:view];

      // Explicit drawing: The view redraws its contents only when you explicitly call the draw method. In this case, set paused to true and enableSetNeedsDisplay to false. Use this mode to create your own custom workflow.
      [view setPaused:true];
      [view setEnableSetNeedsDisplay:false];

      renderer = [[RubyMetalRenderer alloc] initWith:this mtkView:view device:device];
    }

    clear();
    return _ready = true;
  }

  auto terminate() -> void {
    _ready = false;

    @autoreleasepool {
      if(view) {
        [view removeFromSuperview];
        [view release];
        view = nil;
      }

      if(window) {
      //[NSApp setPresentationOptions:NSApplicationPresentationDefault];
        [window toggleFullScreen:nil];
        [window setCollectionBehavior:NSWindowCollectionBehaviorDefault];
        [window close];
        [window release];
        window = nil;
      }
    }
  }

  MTKView* view = nullptr;
  RubyWindowMTL* window = nullptr;
  RubyMetalRenderer* renderer = nullptr;

  bool _ready = false;

  uint _width = 0;
  uint _height = 0;
  uint32_t *_buffer = nullptr;
};


@implementation RubyMetalRenderer : NSObject

  uint width;
  uint height;

-(id) initWith:(VideoMTL*)videoPointer mtkView:(MTKView*)mtkView device:(id<MTLDevice>)newDevice {
  self = [super init];

  video = videoPointer;
  view = mtkView;
  [view setClearColor: MTLClearColorMake(0.0, 0.5, 0.0, 1)];

  device = newDevice;
  commandQueue = [device newCommandQueue];
  currentTexture = nil;

  [self createBuffers];
  [self createPipeline];

  [view setDelegate: self];

  return self;
}

-(void) createBuffers {
  // Vertices for a rectangle that fills the screen
  static const float vertexData[] = {
    -1.0,  1.0, 0.0, 1.0,  0.0, 0.0,  // Top-left
     1.0,  1.0, 0.0, 1.0,  1.0, 0.0,  // Top-right
    -1.0, -1.0, 0.0, 1.0,  0.0, 1.0,  // Bottom-left
     1.0, -1.0, 0.0, 1.0,  1.0, 1.0   // Bottom-right
  };

  vertexBuffer = [device newBufferWithBytes:vertexData
                                     length:sizeof(vertexData)
                                    options:MTLResourceStorageModeShared];
}

const string MetalShaders = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float4 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 texCoord [[user(texCoord)]];
};

vertex VertexOut vertex_main(
    VertexIn in [[stage_in]]
) {
    VertexOut out;
    out.position = in.position;
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 fragment_main(
    VertexOut        in  [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler          texSampler [[sampler(0)]]
) {
    // Sample the texture at the given texture coordinates
    float4 color = tex.sample(texSampler, in.texCoord);
    return color;
}
)";

-(void) createPipeline {
  // Create a basic Metal pipeline
  NSError *error = nil;

  // Load shaders
  NSString *shadersSource = [[NSString alloc] initWithUTF8String:MetalShaders];

  id<MTLLibrary> library = [device newLibraryWithSource:shadersSource options:nil error:&error];
  if (error) {
    NSLog(@"Error creating library: %@", error);
    return;
  }

  id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
  id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];

  // Set up the vertex descriptor
  MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];
  
  // Position attribute (0)
  vertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;
  vertexDescriptor.attributes[0].offset = 0;
  vertexDescriptor.attributes[0].bufferIndex = 0;
  
  // TexCoord attribute (1)
  vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
  vertexDescriptor.attributes[1].offset = sizeof(float) * 4; // After 4 floats for position
  vertexDescriptor.attributes[1].bufferIndex = 0;
  
  // Set up the buffer layout (how the vertex data is arranged in memory)
  vertexDescriptor.layouts[0].stride = sizeof(float) * 6;  // 4 floats for position, 2 for texcoord

  MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
  pipelineDescriptor.vertexFunction = vertexFunction;
  pipelineDescriptor.fragmentFunction = fragmentFunction;
  pipelineDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
  pipelineDescriptor.vertexDescriptor = vertexDescriptor;

  pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
  if (error) {
    NSLog(@"Error creating pipeline state: %@", error);
    return;
  }

  // Create a sampler descriptor
  MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];

  // Set the minification filter (how to handle when the texture is minified)
  samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;

  // Set the magnification filter (how to handle when the texture is magnified)
  samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;

  // Set the mipmap filter (how to handle when using mipmaps)
  samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear; // Linear mipmap filtering

  // Set texture addressing modes
  samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat; // Repeat texture when coordinates are out of bounds
  samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
  samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;

  // Optionally, set whether to enable comparison (e.g., for shadow maps)
  samplerDescriptor.compareFunction = MTLCompareFunctionAlways; // No comparison, always return the sample

  // Create the sampler state using the descriptor
  samplerState = [device newSamplerStateWithDescriptor:samplerDescriptor];
}

-(void) uploadTexture:(uint32_t*)bytes width:(uint)newWidth height:(uint)newHeight {
  if (newWidth <= 0 || newHeight <= 0) {
    // [currentTexture release];
    currentTexture = nil;
    return;
  }

  // recreate texture if width/height changes:
  if (width != newWidth || height != newHeight) {
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
    textureDescriptor.width = newWidth;
    textureDescriptor.height = newHeight;
    textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
    textureDescriptor.storageMode = MTLStorageModeShared;
    textureDescriptor.usage = MTLTextureUsageShaderRead;

    currentTexture = [device newTextureWithDescriptor:textureDescriptor];
    width = newWidth;
    height = newHeight;
  }

  // upload new data:
  [currentTexture replaceRegion:MTLRegionMake2D(0, 0, width, height)
          mipmapLevel:0
          withBytes:bytes
          bytesPerRow:width * sizeof(uint32_t)];
}

-(void) drawInMTKView:(MTKView*)view {
  @autoreleasepool {
    [self render:view];
  }
}

-(void) render:(MTKView*)view {
  if (currentTexture == nil) {
    return;
  }

  id<CAMetalDrawable> drawable = view.currentDrawable;
  if (drawable == nil) {
    return;
  }

  // Create a command buffer:
  id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

  // Create a render command encoder:
  MTLRenderPassDescriptor *passDescriptor = view.currentRenderPassDescriptor;
  id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];

  [renderEncoder setRenderPipelineState:pipelineState];
  [renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
  [renderEncoder setFragmentTexture:currentTexture atIndex:0];
  [renderEncoder setFragmentSamplerState:samplerState atIndex:0];
  [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

  [renderEncoder endEncoding];

  [commandBuffer presentDrawable:drawable];

  // create a sharedEvent for blocking/synchronizing:
  bool isBlocking = self.blocking;
  id<MTLSharedEvent> sharedEvent = nil;
  if (isBlocking) {
    sharedEvent = [device newSharedEvent];
    [commandBuffer encodeSignalEvent:sharedEvent value:1];
  }

  // Commit the command buffer
  [commandBuffer commit];

  if (isBlocking) {
    // Now we wait for the shared event to be signaled (i.e., GPU has finished the work)
    [sharedEvent waitUntilSignaledValue:1 timeoutMS:16];
  }
}

-(void) mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
  video->output(0, 0);
}

@end

@implementation RubyWindowMTL : NSWindow

-(id) initWith:(VideoMTL*)videoPointer {
  auto primaryRect = [[[NSScreen screens] objectAtIndex:0] frame];
  if(self = [super initWithContentRect:primaryRect styleMask:0 backing:NSBackingStoreBuffered defer:YES]) {
    video = videoPointer;
    [self setDelegate:self];
    [self setReleasedWhenClosed:NO];
    [self setAcceptsMouseMovedEvents:YES];
    [self setTitle:@""];
    [self makeKeyAndOrderFront:nil];
  }
  return self;
}

-(BOOL) canBecomeKeyWindow {
  return YES;
}

-(BOOL) canBecomeMainWindow {
  return YES;
}

@end
