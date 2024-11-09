
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

struct VideoMTL;

const string VertexShaderMetal = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float4 position [[attribute(0)]]; // Position (x, y)
    float2 texCoord [[attribute(1)]]; // Texture coordinates (u, v)
};

struct VertexOut {
    float4 position [[position]]; // Final position
    float2 texCoord [[user(texCoord)]]; // Passed to the fragment shader
};

vertex VertexOut vertex_main(VertexIn in [[stage_in]]) {
    VertexOut out;

    // Pass the position directly through
    out.position = in.position;

    // Pass the texture coordinates to the fragment shader
    out.texCoord = in.texCoord;

    return out;
}
)";

const string FragmentShaderMetal = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 texCoord [[user(texCoord)]];
};

fragment float4 fragment_main(VertexOut in [[stage_in]], texture2d<half> tex [[texture(0)]], sampler texSampler [[sampler(0)]]) {
    // Sample the texture at the given coordinates (in.texCoord)
    half4 texColor = tex.sample(texSampler, in.texCoord);

    // Return the texture color
    return float4(texColor);
}
)";

@interface RubyWindowMTL : NSWindow <NSWindowDelegate> {
@public
  VideoMTL* video;
}
-(id) initWith:(VideoMTL*)video;
-(BOOL) canBecomeKeyWindow;
-(BOOL) canBecomeMainWindow;
@end

@interface RubyMetalKitView : MTKView {
@public
  VideoMTL* video;
}
-(id) initWith:(VideoMTL*)video frame:(NSRect)frame;
-(void) reshape;
-(BOOL) acceptsFirstResponder;
@end

@interface RubyMetalRenderer : NSObject <MTKViewDelegate> {
@public
  VideoMTL* video;
  MTKView*  view;

  id<MTLDevice>       device;
  id<MTLCommandQueue> commandQueue;

  id<MTLRenderPipelineState> pipelineState;
  id<MTLBuffer> vertexBuffer;
  id<MTLBuffer> indexBuffer;
  id<MTLTexture> texture;
  id<MTLTexture> dynamicTexture;
  id<MTLTexture> currentTexture;
  id<MTLSamplerState> samplerState;

  float verticesUVs[16];
  uint16_t indices[6];
}
-(id)   initWith:(VideoMTL*)video mtkView:(MTKView*)mtkView;
-(void) drawInMTKView:(MTKView *) view;
-(void) mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size;
@end

@implementation RubyMetalRenderer : NSObject

  uint width;
  uint height;

-(id) initWith:(VideoMTL*)videoPointer mtkView:(MTKView*)mtkView {
  self = [super init];

  video = videoPointer;
  view = mtkView;
  [view setClearColor: MTLClearColorMake(0.25, 0.25, 0.25, 1)];

  device = MTLCreateSystemDefaultDevice();
  [view setDevice: device];

  commandQueue = [device newCommandQueue];
  currentTexture = nil;

  [self createBuffers];
  [self createPipeline];

  [view setDelegate: self];

  return self;
}

-(void) createBuffers {
  static float vuvs_static[16] = {
    -0.5f, -0.5f, 0.0f, 0.0f,  // Bottom-left (position + texcoord)
    0.5f, -0.5f, 1.0f, 0.0f,   // Bottom-right
    -0.5f, 0.5f, 0.0f, 1.0f,    // Top-left
    0.5f, 0.5f, 1.0f, 1.0f      // Top-right
  };
  // Indices for rectangle (2 triangles)
  static uint16_t indices_static[6] = {
    0, 1, 2, // First triangle
    1, 3, 2  // Second triangle
  };

  memcpy(verticesUVs, vuvs_static, sizeof(vuvs_static));
  memcpy(indices, indices_static, sizeof(indices_static));

  // Create vertex buffer
  vertexBuffer = [device newBufferWithBytes:verticesUVs length:sizeof(verticesUVs) options:MTLResourceStorageModeShared];

  // Create index buffer
  indexBuffer = [device newBufferWithBytes:indices length:sizeof(indices) options:MTLResourceStorageModeShared];
}

-(void) createPipeline {
  // Create a basic Metal pipeline
  NSError *error = nil;

  // Load shaders
  NSString *vertexSource = [[NSString alloc] initWithUTF8String:VertexShaderMetal];
  NSString *fragmentSource = [[NSString alloc] initWithUTF8String:FragmentShaderMetal];

  id<MTLLibrary> library = [device newLibraryWithSource:vertexSource options:nil error:&error];
  if (error) {
      NSLog(@"Error creating library: %@", error);
      return;
  }

  id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
  id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];

  MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
  pipelineDescriptor.vertexFunction = vertexFunction;
  pipelineDescriptor.fragmentFunction = fragmentFunction;
  pipelineDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;

  // Create the vertex descriptor
  MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];

  // Define the position attribute
  vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
  vertexDescriptor.attributes[0].offset = 0;
  vertexDescriptor.attributes[0].bufferIndex = 0;

  // Define the texture coordinate attribute
  vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
  vertexDescriptor.attributes[1].offset = sizeof(float) * 2; // Offset by 2 floats (x, y)
  vertexDescriptor.attributes[1].bufferIndex = 0;

  // Define the layout for the vertex buffer
  vertexDescriptor.layouts[0].stride = sizeof(float) * 4;  // 4 floats (2 for position, 2 for texcoord)
  vertexDescriptor.layouts[0].stepRate = 1;
  vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

  // Set the vertex descriptor on the pipeline descriptor
  pipelineDescriptor.vertexDescriptor = vertexDescriptor;

  pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
  if (error) {
      NSLog(@"Error creating pipeline state: %@", error);
      return;
  }

  // Create a sampler state (for texture filtering)
  MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
  samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
  samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
  samplerState = [device newSamplerStateWithDescriptor:samplerDescriptor];
}

-(void) uploadTexture:(uint32_t*)bytes width:(uint)newWidth height:(uint)newHeight {
  if (newWidth <= 0 || newHeight <= 0) {
    currentTexture = nil;
    return;
  }

  // recreate texture if width/height changes:
  if (width != newWidth || height != newHeight) {
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
    textureDescriptor.width = newWidth;
    textureDescriptor.height = newHeight;
    textureDescriptor.pixelFormat = MTLPixelFormatRGBA8Unorm;
    textureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

    currentTexture = [device newTextureWithDescriptor:textureDescriptor];
    width = newWidth;
    height = newHeight;
  }

  assert(currentTexture);

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
  // Create a drawable
  CAMetalLayer *metalLayer = (CAMetalLayer *)view.layer;
  id<CAMetalDrawable> drawable = [metalLayer nextDrawable];

  if (!drawable) {
    printf("render: NO DRAWABLE\n");
    return;
  }

  // Create command buffer and render command encoder
  MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
  passDescriptor.colorAttachments[0].texture = drawable.texture;
  passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
  passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
  passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

  id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
  id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];

  // Set pipeline state and texture
  [renderEncoder setRenderPipelineState:pipelineState];
  [renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
  [renderEncoder setVertexBuffer:indexBuffer offset:0 atIndex:1];
  [renderEncoder setFragmentTexture:currentTexture atIndex:0];
  [renderEncoder setFragmentSamplerState:samplerState atIndex:0];

  // Draw the rectangle
  [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:6
                             indexType:MTLIndexTypeUInt16
                           indexBuffer:indexBuffer
                     indexBufferOffset:0];

  [renderEncoder endEncoding];

  [commandBuffer presentDrawable:drawable];
  [commandBuffer commit];
}

-(void) mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {}

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
    // TODO
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

      view = [[RubyMetalKitView alloc] initWith:this frame:context.bounds];
      [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
      view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;

      [context addSubview:view];
      [[view window] makeFirstResponder:view];

      // Explicit drawing: The view redraws its contents only when you explicitly call the draw method. In this case, set paused to true and enableSetNeedsDisplay to false. Use this mode to create your own custom workflow.
      [view setPaused:true];
      [view setEnableSetNeedsDisplay:false];

      [view lockFocus];

      renderer = [[RubyMetalRenderer alloc] initWith:this mtkView:view];

      [view unlockFocus];
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

  RubyMetalKitView* view = nullptr;
  RubyWindowMTL* window = nullptr;
  RubyMetalRenderer* renderer = nullptr;

  bool _ready = false;

  uint _width = 0;
  uint _height = 0;
  uint32_t *_buffer = nullptr;
};

@implementation RubyMetalKitView : MTKView

-(id) initWith:(VideoMTL*)videoPointer frame:(NSRect)frame {
  if(self = [super initWithFrame:frame]) {
    video = videoPointer;
  }
  return self;
}

-(void) reshape {
  video->output(0, 0);
}

-(BOOL) acceptsFirstResponder {
  return YES;
}

-(void) keyDown:(NSEvent*)event {
}

-(void) keyUp:(NSEvent*)event {
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
