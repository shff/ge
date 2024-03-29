@import Cocoa;
@import IOKit.pwr_mgt;
@import Metal;
@import QuartzCore;
@import AudioToolbox;
#include "common.h"
#include <dlfcn.h>

static OSStatus audioCallback(void *inRefCon,
                              AudioUnitRenderActionFlags *ioActionFlags,
                              const AudioTimeStamp *inTimeStamp,
                              UInt32 inBusNumber, UInt32 inNumberFrames,
                              AudioBufferList *ioData)
{
  voice *voices = (voice *)inRefCon;
  SInt16 *left = (SInt16 *)ioData->mBuffers[0].mData;
  SInt16 *right = (SInt16 *)ioData->mBuffers[1].mData;
  for (UInt32 frame = 0; frame < inNumberFrames; frame++)
  {
    left[frame] = right[frame] = 0;
    for (int i = 0; i < 32; i++)
    {
      if (voices[i].state == 0) continue;
      if (voices[i].position >= voices[i].length - 1) voices[i].state = 0;

      left[frame] += (voices[i].data)[voices[i].position] * 1.0f;
      right[frame] += (voices[i].data)[voices[i].position] * 1.0f;
      voices[i].position++;
    }
  }

  return 0;
}

id<MTLRenderPipelineState> createShader(id<MTLLibrary> library, NSString *name,
                                        id<MTLDevice> device)
{
  MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
  desc.vertexFunction =
      [library newFunctionWithName:[@"v_" stringByAppendingString:name]];
  desc.fragmentFunction =
      [library newFunctionWithName:[@"f_" stringByAppendingString:name]];
  desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
  desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
  return [device newRenderPipelineStateWithDescriptor:desc error:NULL];
}

MTLRenderPassDescriptor *createPass(int textures, MTLLoadAction action)
{
  MTLRenderPassDescriptor *pass = [[MTLRenderPassDescriptor alloc] init];
  for (int i = 0; i < textures; i++)
  {
    pass.colorAttachments[i].loadAction = action;
    pass.colorAttachments[i].storeAction = MTLStoreActionStore;
    pass.colorAttachments[i].clearColor = MTLClearColorMake(0.01, 0, 0, 1);
  }
  pass.depthAttachment.clearDepth = 1.0;
  pass.depthAttachment.loadAction = action;
  pass.depthAttachment.storeAction = MTLStoreActionStore;
  return pass;
}

int main()
{
  @autoreleasepool
  {
    id app = [NSApplication sharedApplication];

    // Get the Application Name
    id bundleName = [[NSProcessInfo processInfo] processName];
    id displayName =
        [[NSBundle mainBundle] infoDictionary][@"CFBundleDisplayName"];
    NSString *clientPath = [[NSBundle mainBundle] pathForResource:@"client"
                                                           ofType:@"so"];

    // Get client path and timestamp
    NSDate *clientDate = NULL;
    void *client = NULL;
    void (*updateFunction)(void) = NULL;

    // Prevent Sleeping
    IOPMAssertionID assertionID;
    IOPMAssertionCreateWithName(
        kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
        CFSTR("Application is an interactive game."), &assertionID);

    // Create the App Menu
    id appMenu = [[NSMenu new] autorelease];
    id servicesMenu = [[NSMenu alloc] autorelease];
    [[[appMenu addItemWithTitle:@"Services" action:NULL
                  keyEquivalent:@""] autorelease] setSubmenu:servicesMenu];
    [appMenu addItem:[[NSMenuItem separatorItem] autorelease]];
    [[appMenu addItemWithTitle:@"Hide"
                        action:@selector(hide:)
                 keyEquivalent:@"h"] autorelease];
    [[[appMenu addItemWithTitle:@"Hide Others"
                         action:@selector(hideOtherApplications:)
                  keyEquivalent:@"h"] autorelease]
        setKeyEquivalentModifierMask:NSEventModifierFlagOption |
                                     NSEventModifierFlagCommand];
    [[appMenu addItemWithTitle:@"Show All"
                        action:@selector(unhideAllApplications:)
                 keyEquivalent:@""] autorelease];
    [appMenu addItem:[[NSMenuItem separatorItem] autorelease]];
    [[appMenu addItemWithTitle:@"Quit"
                        action:@selector(terminate:)
                 keyEquivalent:@"q"] autorelease];
    [app setServicesMenu:servicesMenu];

    // Create the Window Menu
    id windowMenu = [[[NSMenu alloc] initWithTitle:@"Window"] autorelease];
    [[windowMenu addItemWithTitle:@"Minimize"
                           action:@selector(performMiniaturize:)
                    keyEquivalent:@"m"] autorelease];
    [[windowMenu addItemWithTitle:@"Zoom"
                           action:@selector(performZoom:)
                    keyEquivalent:@"n"] autorelease];
    [[[windowMenu addItemWithTitle:@"Full Screen"
                            action:@selector(toggleFullScreen:)
                     keyEquivalent:@"f"] autorelease]
        setKeyEquivalentModifierMask:NSEventModifierFlagControl |
                                     NSEventModifierFlagCommand];
    [[windowMenu addItemWithTitle:@"Close Window"
                           action:@selector(performClose:)
                    keyEquivalent:@"w"] autorelease];
    [windowMenu addItem:[[NSMenuItem separatorItem] autorelease]];
    [[windowMenu addItemWithTitle:@"Bring All to Front"
                           action:@selector(arrangeInFront:)
                    keyEquivalent:@""] autorelease];
    [app setWindowsMenu:windowMenu];

    // Create the Help Menu
    id helpMenu = [[[NSMenu alloc] initWithTitle:@"Help"] autorelease];
    [[helpMenu addItemWithTitle:@"Documentation"
                         action:@selector(docs:)
                  keyEquivalent:@""] autorelease];
    [app setHelpMenu:helpMenu];

    // Create the Menu Bar
    id menubar = [[NSMenu new] autorelease];
    [[[menubar addItemWithTitle:@"" action:NULL
                  keyEquivalent:@""] autorelease] setSubmenu:appMenu];
    [[[menubar addItemWithTitle:@"Window" action:NULL
                  keyEquivalent:@""] autorelease] setSubmenu:windowMenu];
    [[[menubar addItemWithTitle:@"Help" action:NULL
                  keyEquivalent:@""] autorelease] setSubmenu:helpMenu];
    [app setMainMenu:menubar];

    voice voices[32] = {};

    // Initialize Audio
    AudioUnit audioUnit;
    AudioComponentDescription compDesc = {
      .componentType = kAudioUnitType_Output,
      .componentSubType = kAudioUnitSubType_DefaultOutput,
      .componentManufacturer = kAudioUnitManufacturer_Apple
    };
    AudioStreamBasicDescription audioFormat = {
      .mSampleRate = 44100.00,
      .mFormatID = kAudioFormatLinearPCM,
      .mFormatFlags = kAudioFormatFlagIsSignedInteger |
                      kAudioFormatFlagIsPacked |
                      kAudioFormatFlagIsNonInterleaved,
      .mBitsPerChannel = 16,
      .mChannelsPerFrame = 2,
      .mFramesPerPacket = 1,
      .mBytesPerFrame = 2,
      .mBytesPerPacket = 2
    };
    AURenderCallbackStruct callback = { audioCallback, voices };
    AudioComponentInstanceNew(AudioComponentFindNext(0, &compDesc), &audioUnit);
    AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Input, 0, &audioFormat,
                         sizeof(audioFormat));
    AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Input, 0, &callback,
                         sizeof(AURenderCallbackStruct));
    AudioUnitInitialize(audioUnit);
    AudioOutputUnitStart(audioUnit);

    // Create the Window
    NSWindow *window =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1200, 720)
                                    styleMask:NSWindowStyleMaskTitled |
                                              NSWindowStyleMaskResizable |
                                              NSWindowStyleMaskClosable |
                                              NSWindowStyleMaskMiniaturizable
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    [window setTitle:(displayName ? displayName : bundleName)];
    [window setMinSize:NSMakeSize(300, 200)];
    [window setAcceptsMouseMovedEvents:YES];
    [window makeKeyAndOrderFront:nil];
    [window center];

    // Disable tabbing
    if ([window respondsToSelector:@selector(setTabbingMode:)])
      [window setTabbingMode:NSWindowTabbingModeDisallowed];

    // Create the Metal device
    id device = [MTLCreateSystemDefaultDevice() autorelease];
    id queue = [device newCommandQueue];
    id layer = [CAMetalLayer layer];
    [layer setDevice:device];
    [window.contentView setLayer:layer];

    // Initialize Shader Library
    NSString *shaderPath = [[NSBundle mainBundle] pathForResource:@"shaders"
                                                           ofType:@"metal"];
    NSData *shaderData = [NSData dataWithContentsOfFile:shaderPath];
    id source = [[NSString alloc] initWithData:shaderData encoding:4];
    id<MTLLibrary> library = [device newLibraryWithSource:source
                                                  options:nil
                                                    error:NULL];

    // Create Passes, Shaders and Buffers
    id postShader = createShader(library, @"post", device);
    MTLRenderPassDescriptor *postPass = createPass(1, MTLLoadActionLoad);
    id geometryShader = createShader(library, @"quad", device);
    MTLRenderPassDescriptor *geometryPass = createPass(1, MTLLoadActionClear);
    id<MTLTexture> depthTexture, albedoTexture;

    // Stencil
    MTLDepthStencilDescriptor *stencilDesc = [MTLDepthStencilDescriptor new];
    stencilDesc.depthCompareFunction = MTLCompareFunctionLess;
    stencilDesc.depthWriteEnabled = YES;
    id<MTLDepthStencilState> stencil =
        [device newDepthStencilStateWithDescriptor:stencilDesc];

    // Initialize state
    NSMutableDictionary *keysDown = [[NSMutableDictionary alloc] init];
    NSMutableDictionary *geometry = [[NSMutableDictionary alloc] init];
    float deltaX = 0.0f, deltaY = 0.0f;
    float posX = 0.0f, posY = 0.0f, posZ = 10.0f, camX = 0.0f, camY = 0.f;
    int mouseMode = 2;

    // Add test geometry - TODO: Move to Gamecode
    float tris[] = { 0.0, 0.8, 0.0, -0.8, -0.8, 0.0, 0.8, -0.8, 0.0 };
    geometry[@"tri"] = [device newBufferWithBytes:tris
                                           length:9 * sizeof(float)
                                          options:MTLResourceStorageModeShared];

    // Cursor Visibility Manager
    __block bool cursorVisible = true;
    void (^toggleMouse)(bool) = ^(bool mode) {
      if (mode == cursorVisible) return;

      if (!mode && mouseMode == 1)
        CGWarpMouseCursorPosition(CGPointMake(
            NSMidX([window frame]),
            [[window screen] frame].size.height - NSMidY([window frame])));

      mode ? [NSCursor unhide] : [NSCursor hide];
      CGAssociateMouseAndMouseCursorPosition(mode);
      cursorVisible = mode;
    };

    // Setup Closing Observer
    __block int running = 1;
    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSWindowWillCloseNotification
                    object:window
                     queue:nil
                usingBlock:^(NSNotification *notification) {
                  running = 0;
                }];

    // Finish loading
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [app activateIgnoringOtherApps:YES];
    [app finishLaunching];

    // Start the Timer
    double timerCurrent = CACurrentMediaTime();
    double ticks = 0.0;
    double lag = 0.0;
    unsigned int width = 0, height = 0;

    // Game Loop
    while (running)
    {
      @autoreleasepool
      {
        NSEvent *event;
        NSPoint click;
        while ((event = [app nextEventMatchingMask:NSEventMaskAny
                                         untilDate:[NSDate distantPast]
                                            inMode:NSDefaultRunLoopMode
                                           dequeue:YES]) != nil)
        {
          if ([event type] == NSEventTypeKeyDown &&
              [event modifierFlags] & NSEventModifierFlagCommand)
            [app sendEvent:event];
          else if ([event type] == NSEventTypeKeyDown)
            keysDown[[event charactersIgnoringModifiers]] = @YES;
          else if ([event type] == NSEventTypeKeyUp)
            [keysDown removeObjectForKey:[event charactersIgnoringModifiers]];
          // Mouse click start (mode 0) or click end (mode 1/2)
          else if (([event type] == NSEventTypeLeftMouseDown &&
                    [window.contentView hitTest:[event locationInWindow]] &&
                    mouseMode == 0) ||
                   ([event type] == NSEventTypeLeftMouseUp && mouseMode != 0 &&
                    [event clickCount] > 0))
            click = [event locationInWindow];
          // Mouse is outside the window, or we finished dragging (mode 2)
          else if (([event type] == NSEventTypeMouseMoved &&
                    ![window.contentView hitTest:[event locationInWindow]]) ||
                   ([event type] == NSEventTypeLeftMouseUp && mouseMode == 2))
            toggleMouse(true);
          // Mouse moved (mode 1) or dragged (mode 2)
          else if (([event type] == NSEventTypeMouseMoved && mouseMode == 1) ||
                   ([event type] == NSEventTypeLeftMouseDragged &&
                    [window.contentView hitTest:[event locationInWindow]] &&
                    mouseMode == 2))
            toggleMouse(false), deltaX += [event deltaX],
                deltaY += [event deltaY];
          else
            [app sendEvent:event];
        }

        // Load or reload client if necessary
        NSDate *newTimestamp = [[[NSFileManager defaultManager]
            attributesOfItemAtPath:clientPath
                             error:NULL] fileModificationDate];
        if (clientPath && (!clientDate || [newTimestamp compare:clientDate] ==
                                              NSOrderedDescending))
        {
          if (client) dlclose(client);
          client = dlopen([clientPath UTF8String], RTLD_LAZY);
          if (client) updateFunction = (void (*)(void))dlsym(client, "update");
          clientDate = newTimestamp;
        }

        // Update Timer
        double timerNext = CACurrentMediaTime();
        double timerDelta = timerNext - timerCurrent;
        timerCurrent = timerNext;

        // Fixed updates
        // TODO - Just calculate amount of frames, and pass it to Update()
        for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
        {
          ticks += 1.0 / 60.0;
        }

        // TODO - Call Update() around here.
        //  - It should receive keysDown, delta/click, timerDelta and screen
        //  size (?)
        //  - Maybe it should also receive the amount of fixedUpdates...
        //  - Do we really need to let it know the timerDelta? Or are fixed
        //  frames enough?
        //  - It should return/modify a render queue
        //  - How should it communicate which resources to load? (textures,
        //  shaders, etc)
        if (updateFunction) updateFunction();

        // Update Camera - TODO: Move to Gamecode
        camX += deltaX * 0.01f;
        camY += deltaY * 0.01f;
        if ([keysDown objectForKey:@"w"]) posZ -= 0.1f;
        if ([keysDown objectForKey:@"s"]) posZ += 0.1f;
        if ([keysDown objectForKey:@"a"]) posX -= 0.1f;
        if ([keysDown objectForKey:@"d"]) posX += 0.1f;

        // Reset Deltas
        deltaX = deltaY = 0.0f;
        (void)ticks;

        // Get Viewport Size
        // TODO - Should we pass this to the game layer? What if it needs to
        // know about mouse position?
        CGRect frame = [window.contentView frame];
        CGSize size = [window.contentView convertRectToBacking:frame].size;

        // Rebuild Textures if necessary
        if (size.width != width || size.height != height)
        {
          [layer setDrawableSize:size];

          // Setup Texture Descriptors
          // TODO - Handle configurable amounts of textures and passes
          MTLTextureDescriptor *desc =
              [[[MTLTextureDescriptor alloc] init] autorelease];
          desc.storageMode = MTLStorageModePrivate;
          desc.usage = MTLTextureUsageRenderTarget;
          desc.width = size.width;
          desc.height = size.height;

          // Create Albedo Texture
          desc.pixelFormat = MTLPixelFormatRGBA8Unorm_sRGB;
          albedoTexture = [[device newTextureWithDescriptor:desc] autorelease];

          // Create Depth Texture
          desc.pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
          depthTexture = [[device newTextureWithDescriptor:desc] autorelease];

          width = size.width;
          height = size.height;
        }

        // Setup Matrices
        matrix P =
            getProjectionMatrix(size.width, size.height, 65.0f, 1.0f, 1000.f);
        matrix M = getViewMatrix(camX, camY, posX, posY, posZ);
        id PBuffer = [[device newBufferWithBytes:&P length:sizeof(P)
                                         options:0] autorelease];
        id MBuffer = [[device newBufferWithBytes:&M length:sizeof(M)
                                         options:0] autorelease];

        // Initialize Renderer
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        id buffer = [queue commandBuffer];

        // Geometry Pass
        geometryPass.colorAttachments[0].texture = albedoTexture;
        geometryPass.depthAttachment.texture = depthTexture;
        id encoder1 = [buffer renderCommandEncoderWithDescriptor:geometryPass];
        [encoder1 setRenderPipelineState:geometryShader];
        [encoder1 setDepthStencilState:stencil];

        // TODO - Geometry should actually be a queue of objects
        for (id buffer in geometry.objectEnumerator)
        {
          [encoder1 setVertexBuffer:buffer offset:0 atIndex:0];
          [encoder1 setVertexBuffer:PBuffer offset:0 atIndex:1];
          [encoder1 setVertexBuffer:MBuffer offset:0 atIndex:2];
          [encoder1 drawPrimitives:MTLPrimitiveTypeTriangle
                       vertexStart:0
                       vertexCount:3];
        }
        [encoder1 endEncoding];

        // Post-processing Pass
        // TODO - Handle configurable amounts of textures and passes
        postPass.colorAttachments[0].texture = drawable.texture;
        postPass.depthAttachment.texture = depthTexture;
        id encoder2 = [buffer renderCommandEncoderWithDescriptor:postPass];
        [encoder2 setRenderPipelineState:postShader];
        [encoder2 setFragmentTexture:albedoTexture atIndex:0];
        [encoder2 setFragmentTexture:depthTexture atIndex:1];
        [encoder2 drawPrimitives:MTLPrimitiveTypeTriangleStrip
                     vertexStart:0
                     vertexCount:4];
        [encoder2 endEncoding];

        // Render
        [buffer presentDrawable:drawable];
        [buffer commit];
      }
    }

    // Terminate
    IOPMAssertionRelease(assertionID);
    [app terminate:nil];

    return 0;
  }
}
