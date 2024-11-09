
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

struct VideoMTL;

@interface RubyMetalKitView : MTKView {
@public
  VideoMTL* video;
}
-(id) initWith:(VideoMTL*)video;
-(void) reshape;
-(BOOL) acceptsFirstResponder;
@end

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
}
-(id)   initWith:(VideoMTL*)video mtkView:(MTKView*)mtkView;
-(void) draw:(MTKView*)view;
-(void) mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size;
@end

@implementation RubyMetalRenderer : NSObject

-(id) initWith:(VideoMTL*)videoPointer mtkView:(MTKView*)mtkView {
  self = [super init];

  video = videoPointer;
  view = mtkView;
  [view setClearColor: MTLClearColorMake(0.5, 0.5, 0.5, 1)];

  device = MTLCreateSystemDefaultDevice();
  commandQueue = [device newCommandQueue];

  [view setDelegate: self];
  [view setDevice: device];

  return self;
}

-(void) draw:(MTKView*)view {

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
    return true;
  }

  auto release() -> void override {
  }

  auto output(uint width, uint height) -> void override {
    uint windowWidth, windowHeight;
    size(windowWidth, windowHeight);

    @autoreleasepool {
      if([view lockFocusIfCanDraw]) {
        // TODO

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

      view = [[RubyMetalKitView alloc] initWith:this];
      [view setFrame:NSMakeRect(0, 0, size.width, size.height)];
      [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

      // Explicit drawing: The view redraws its contents only when you explicitly call the draw method. In this case, set paused to true and enableSetNeedsDisplay to false. Use this mode to create your own custom workflow.
      [view setPaused:true];
      [view setEnableSetNeedsDisplay:false];

      auto renderer = [[RubyMetalRenderer alloc] initWith:this mtkView:view];

      [context addSubview:view];

      [[view window] makeFirstResponder:view];
      [view lockFocus];
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

  bool _ready = false;
};

@implementation RubyMetalKitView : MTKView

-(id) initWith:(VideoMTL*)videoPointer {
  if(self = [super initWithFrame:NSMakeRect(0, 0, 0, 0)]) {
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
