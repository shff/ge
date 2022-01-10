@import UIKit;
@import Foundation;
@import Metal;
@import QuartzCore;
@import AudioToolbox;
#include "common.h"

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

@interface App : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@property(nonatomic, assign) id<MTLDevice> device;
@property(nonatomic, assign) id<MTLCommandQueue> queue;
@property(nonatomic, assign) CAMetalLayer *layer;
@property(nonatomic, assign) id<MTLTexture> depthTexture, albedoTexture;
@property(nonatomic, assign) id<MTLRenderPipelineState> quadShader, postShader;
@property(nonatomic, assign) MTLRenderPassDescriptor *quadPass, *postPass;
@property(nonatomic, assign) NSMutableDictionary *geometry;
@property(nonatomic, assign) double lag;
@property(nonatomic, assign) voice *voices;
@property(nonatomic, assign) float clickX, clickY, deltaX, deltaY;
@property(nonatomic, assign) float posX, posY, posZ, camX, camY;
@property(nonatomic, assign) int mouseMode;
@property(nonatomic, assign) double timerCurrent;
@end

@implementation App
- (void)applicationDidFinishLaunching:(UIApplication *)application
{
  _voices = (voice *)malloc(sizeof(voice) * 32);
  memset(_voices, 0, sizeof(voice) * 32);
  _posZ = 10.0f;

  // Prevent sleeping
  [UIApplication sharedApplication].idleTimerDisabled = YES;

  // Initialize Audio
  AudioUnit audioUnit;
  AudioComponentDescription compDesc = { .componentType = kAudioUnitType_Output,
                                         .componentSubType =
                                             kAudioUnitSubType_GenericOutput,
                                         .componentManufacturer =
                                             kAudioUnitManufacturer_Apple };
  AudioStreamBasicDescription audioFormat = {
    .mSampleRate = 44100.00,
    .mFormatID = kAudioFormatLinearPCM,
    .mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked |
                    kAudioFormatFlagIsNonInterleaved,
    .mBitsPerChannel = 16,
    .mChannelsPerFrame = 2,
    .mFramesPerPacket = 1,
    .mBytesPerFrame = 2,
    .mBytesPerPacket = 2
  };
  AURenderCallbackStruct callback = { audioCallback, _voices };
  AudioComponentInstanceNew(AudioComponentFindNext(0, &compDesc), &audioUnit);
  AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat,
                       kAudioUnitScope_Input, 0, &audioFormat,
                       sizeof(audioFormat));
  AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback,
                       kAudioUnitScope_Input, 0, &callback,
                       sizeof(AURenderCallbackStruct));
  AudioUnitInitialize(audioUnit);
  AudioOutputUnitStart(audioUnit);

  // Create Window
  _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [_window setRootViewController:[[UIViewController alloc] init]];
  [_window makeKeyAndVisible];

  // Initialize Metal
  _device = [MTLCreateSystemDefaultDevice() autorelease];
  _queue = [_device newCommandQueue];
  _layer = [[CAMetalLayer alloc] init];
  _layer.device = _device;
  [[_window.rootViewController.view layer] addSublayer:_layer];

  // Create Passes, Shaders and Buffers
  _geometry = [[NSMutableDictionary alloc] init];
  _postShader = [self createShader:@"post"];
  _postPass = [self createPass:1 with:MTLLoadActionLoad];
  _quadShader = [self createShader:@"quad"];
  _quadPass = [self createPass:1 with:MTLLoadActionClear];
  [self createBuffers];

  // Initialize state
  _timerCurrent = CACurrentMediaTime();
  _lag = 0.0;
  _mouseMode = 2;
  _clickX = _clickY = _deltaX = _deltaY = 0.0f;

  // Add test geometry - TODO: Move to Gamecode
  float tris[] = { 0.0, 0.8, 0.0, -0.8, -0.8, 0.0, 0.8, -0.8, 0.0 };
  _geometry[@"tri"] = [_device newBufferWithBytes:tris
                                           length:9 * sizeof(float)
                                          options:MTLResourceStorageModeShared];

  // Add gesture recognizers
  [_window.rootViewController.view
      addGestureRecognizer:[[UITapGestureRecognizer alloc]
                               initWithTarget:self
                                       action:@selector(onTap:)]];
  [_window.rootViewController.view
      addGestureRecognizer:[[UIPanGestureRecognizer alloc]
                               initWithTarget:self
                                       action:@selector(onDrag:)]];

  // Re-create buffers when rotating the device
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(createBuffers)
             name:UIDeviceOrientationDidChangeNotification
           object:nil];

  // Initialize loop
  [[CADisplayLink displayLinkWithTarget:self selector:@selector(render:)]
      addToRunLoop:[NSRunLoop currentRunLoop]
           forMode:NSDefaultRunLoopMode];
}

- (void)render:(CADisplayLink *)displayLink
{
  @autoreleasepool
  {
    // Update Timer
    double timerNext = CACurrentMediaTime();
    double timerDelta = timerNext - _timerCurrent;
    _timerCurrent = timerNext;

    // Fixed updates
    for (_lag += timerDelta; _lag >= 1.0 / 60.0; _lag -= 1.0 / 60.0)
    {
    }

    // Update Camera - TODO: Move to Gamecode
    _camX += _deltaX * 0.01f;
    _camY += _deltaY * 0.01f;

    // Reset Deltas
    _clickX = _clickY = _deltaX = _deltaY = 0.0f;

    // Get Viewport Size
    CGSize size = [_window frame].size;

    // Matrices
    matrix P = getProjectionMatrix(size.width, size.height, 65.f, 1.f, 1000.f);
    matrix M = getViewMatrix(_camX, _camY, _posX, _posY, _posZ);
    id PBuffer = [[_device newBufferWithBytes:&P length:sizeof(P)
                                      options:0] autorelease];
    id MBuffer = [[_device newBufferWithBytes:&M length:sizeof(M)
                                      options:0] autorelease];

    // Initialize Renderer
    id<CAMetalDrawable> drawable = [_layer nextDrawable];
    id buffer = [_queue commandBuffer];

    // Geometry Pass
    _quadPass.colorAttachments[0].texture = _albedoTexture;
    _quadPass.depthAttachment.texture = _depthTexture;
    id encoder1 = [buffer renderCommandEncoderWithDescriptor:_quadPass];
    [encoder1 setRenderPipelineState:_quadShader];
    for (id buffer in _geometry.objectEnumerator)
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
    _postPass.colorAttachments[0].texture = drawable.texture;
    _postPass.depthAttachment.texture = _depthTexture;
    id encoder2 = [buffer renderCommandEncoderWithDescriptor:_postPass];
    [encoder2 setRenderPipelineState:_postShader];
    [encoder2 setFragmentTexture:_albedoTexture atIndex:0];
    [encoder2 drawPrimitives:MTLPrimitiveTypeTriangleStrip
                 vertexStart:0
                 vertexCount:4];
    [encoder2 endEncoding];

    // Render
    [buffer presentDrawable:drawable];
    [buffer commit];
  }
}

- (void)onTap:(UITapGestureRecognizer *)recognizer
{
  if (_mouseMode != 1 && recognizer.state == UIGestureRecognizerStateRecognized)
  {
    _clickX = [recognizer locationInView:recognizer.view].x;
    _clickY = [recognizer locationInView:recognizer.view].y;
  }
}

- (void)onDrag:(UIPanGestureRecognizer *)recognizer
{
  if (_mouseMode != 0)
  {
    _deltaX += [recognizer velocityInView:recognizer.view].x * 0.01f;
    _deltaY += [recognizer velocityInView:recognizer.view].y * 0.01f;
  }
}

- (void)createBuffers
{
  CGRect bounds = [_window frame];
  _layer.frame = bounds;

  MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
  desc.storageMode = MTLStorageModePrivate;
  desc.usage = MTLTextureUsageRenderTarget;
  desc.width = bounds.size.width;
  desc.height = bounds.size.height;

  desc.pixelFormat = MTLPixelFormatRGBA8Unorm_sRGB;
  _albedoTexture = [_device newTextureWithDescriptor:desc];

  desc.pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
  _depthTexture = [_device newTextureWithDescriptor:desc];
}

- (id<MTLRenderPipelineState>)createShader:(NSString *)name
{
  NSString *shaderPath = [[NSBundle mainBundle] pathForResource:@"shaders"
                                                         ofType:@"metal"];
  NSData *shaderData = [NSData dataWithContentsOfFile:shaderPath];
  id source = [[NSString alloc] initWithData:shaderData encoding:4];
  id library = [_device newLibraryWithSource:source options:nil error:NULL];

  MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
  desc.vertexFunction =
      [library newFunctionWithName:[@"v_" stringByAppendingString:name]];
  desc.fragmentFunction =
      [library newFunctionWithName:[@"f_" stringByAppendingString:name]];
  desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
  desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
  return [_device newRenderPipelineStateWithDescriptor:desc error:NULL];
}

- (MTLRenderPassDescriptor *)createPass:(int)textures with:(MTLLoadAction)action
{
  MTLRenderPassDescriptor *pass = [[MTLRenderPassDescriptor alloc] init];
  for (int i = 0; i < textures; i++)
  {
    pass.colorAttachments[i].loadAction = action;
    pass.colorAttachments[i].storeAction = MTLStoreActionStore;
    pass.colorAttachments[i].clearColor = MTLClearColorMake(1, 0, 0, 1);
  }
  pass.depthAttachment.clearDepth = 1.0;
  pass.depthAttachment.loadAction = action;
  pass.depthAttachment.storeAction = MTLStoreActionStore;
  return pass;
}

- (NSData *)loadResource:(NSString *)name type:(NSString *)type
{
  NSString *path = [[NSBundle mainBundle] pathForResource:name ofType:type];
  return [NSData dataWithContentsOfFile:path];
}
@end

int main(int argc, char *argv[])
{
  @autoreleasepool
  {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([App class]));
  }
}
