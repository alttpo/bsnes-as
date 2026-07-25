#if defined(Hiro_Application)

@implementation CocoaDelegate : NSObject

-(NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication*)sender {
  using hiro::Application;
  if(Application::state().cocoa.onQuit) Application::Cocoa::doQuit();
  else Application::quit();
  return NSTerminateCancel;
}

-(BOOL) applicationShouldHandleReopen:(NSApplication*)application hasVisibleWindows:(BOOL)flag {
  using hiro::Application;
  if(Application::state().cocoa.onActivate) Application::Cocoa::doActivate();
  return NO;
}

-(void) handleOpenDocumentsEvent:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)replyEvent {
  using hiro::Application;
  NSAppleEventDescriptor* fileList = [event paramDescriptorForKeyword:keyDirectObject];
  for(NSInteger i = 1; i <= [fileList numberOfItems]; i++) {
    NSAppleEventDescriptor* item = [fileList descriptorAtIndex:i];
    NSURL* url = [NSURL URLWithString:[item stringValue]];
    string path = url && [url isFileURL] ? [[url path] UTF8String] : [[item stringValue] UTF8String];
    if(path) Application::Cocoa::doOpen(path);
  }
}

-(void) run:(NSTimer*)timer {
  using hiro::Application;
  if(Application::state().onMain) Application::doMain();
}

-(void) updateInDock:(NSTimer*)timer {
  NSArray* windows = [NSApp windows];
  for(uint n = 0; n < [windows count]; n++) {
    NSWindow* window = [windows objectAtIndex:n];
    if([window isMiniaturized]) {
      [window updateInDock];
    }
  }
}

@end

CocoaDelegate* cocoaDelegate = nullptr;
NSTimer* applicationTimer = nullptr;

namespace hiro {

auto pApplication::exit() -> void {
  quit();
  ::exit(EXIT_SUCCESS);
}

auto pApplication::modal() -> bool {
  return Application::state().modal > 0;
}

auto pApplication::run() -> void {
//applicationTimer = [NSTimer scheduledTimerWithTimeInterval:0.1667 target:cocoaDelegate selector:@selector(updateInDock:) userInfo:nil repeats:YES];

  if(Application::state().onMain) {
    applicationTimer = [NSTimer scheduledTimerWithTimeInterval:0.0 target:cocoaDelegate selector:@selector(run:) userInfo:nil repeats:YES];

    //below line is needed to run application during window resize; however it has a large performance penalty on the resize smoothness
    //[[NSRunLoop currentRunLoop] addTimer:applicationTimer forMode:NSEventTrackingRunLoopMode];
  }

  @autoreleasepool {
    [NSApp run];
  }
}

auto pApplication::pendingEvents() -> bool {
  bool result = false;
  @autoreleasepool {
    NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:NO];
    if(event != nil) result = true;
  }
  return result;
}

auto pApplication::processEvents() -> void {
  @autoreleasepool {
    while(!Application::state().quit) {
      NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
      if(event == nil) break;
      [event retain];
      [NSApp sendEvent:event];
      [event release];
    }
  }
}

auto pApplication::quit() -> void {
  @autoreleasepool {
    [applicationTimer invalidate];
    [NSApp stop:nil];
    NSEvent* event = [NSEvent otherEventWithType:NSApplicationDefined location:NSMakePoint(0, 0) modifierFlags:0 timestamp:0.0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:event atStart:true];
  }
}

auto pApplication::setScreenSaver(bool screenSaver) -> void {
  //TODO: not implemented
}

auto pApplication::initialize() -> void {
  @autoreleasepool {
    [NSApplication sharedApplication];
    cocoaDelegate = [[CocoaDelegate alloc] init];
    [NSApp setDelegate:cocoaDelegate];

    //register as early as possible (before any of the application's own, potentially slow,
    //startup work runs) so a cold-launch "open document" Apple Event isn't missed: on a cold
    //launch (double-click / Finder "Open With"), macOS can deliver this event before the app
    //has finished constructing its UI, and it will not be redelivered if missed
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:cocoaDelegate
                                                        andSelector:@selector(handleOpenDocumentsEvent:withReplyEvent:)
                                                      forEventClass:kCoreEventClass
                                                         andEventID:kAEOpenDocuments];

    //give the just-registered handler a chance to receive any open-document event that is
    //already queued for this process before proceeding into the application's slow startup path
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
  }
}

}

#endif
