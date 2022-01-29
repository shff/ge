// #include "common.h"
// #include <EGL/egl.h>
// #include <GLES3/gl3.h>
// #include <SLES/OpenSLES_Android.h>
// #include <stdint.h>

// #include <android/configuration.h>
#include <android/native_activity.h>
// #include <pthread.h>
// #include <sched.h>

// // Android Native App Glue
// struct android_app;
// struct android_poll_source
// {
//   int32_t id;
//   struct android_app *app;
//   void (*process)(struct android_app *app, struct android_poll_source *source);
// };
// struct android_app
// {
//   void *userData;
//   void (*onAppCmd)(struct android_app *app, int32_t cmd);
//   int32_t (*onInputEvent)(struct android_app *app, AInputEvent *event);
//   ANativeActivity *activity;
//   AConfiguration *config;
//   void *savedState;
//   size_t savedStateSize;
//   ALooper *looper;
//   AInputQueue *inputQueue;
//   ANativeWindow *window;
//   ARect contentRect;
//   int activityState;
//   int destroyRequested;

//   pthread_mutex_t mutex;
//   pthread_cond_t cond;
//   int msgread;
//   int msgwrite;
//   pthread_t thread;
//   struct android_poll_source cmdPollSource;
//   struct android_poll_source inputPollSource;
//   int running;
//   int stateSaved;
//   int destroyed;
//   int redrawNeeded;
//   AInputQueue *pendingInputQueue;
//   ANativeWindow *pendingWindow;
//   ARect pendingContentRect;
// };

// EGLDisplay display;
// EGLSurface surface;
// SLEngineItf audioInterface;
// SLObjectItf audioOutput;
// unsigned int gbuffer;
// int32_t prevId;
// float prevX, prevY, deltaX, deltaY, clickX, clickY;
// uint64_t timerCurrent;

// static void engine_handle_cmd(struct android_app *app, int32_t cmd)
// {
//   if (cmd == 1 /* APP_CMD_INIT_WINDOW */)
//   {
//     // Initialize Display
//     display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
//     eglInitialize(display, 0, 0);

//     // Set Format
//     const EGLint attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
//                                EGL_BLUE_SIZE,    8,
//                                EGL_GREEN_SIZE,   8,
//                                EGL_RED_SIZE,     8,
//                                EGL_NONE };
//     EGLConfig config;
//     EGLint numConfigs, format;
//     eglChooseConfig(display, attribs, &config, 1, &numConfigs);
//     eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
//     ANativeWindow_setBuffersGeometry(app->window, 0, 0, format);
//     surface = eglCreateWindowSurface(display, config, app->window, NULL);

//     // Initialize OpenGL
//     EGLContext context = eglCreateContext(display, config, NULL, NULL);
//     eglMakeCurrent(display, surface, surface, context);

//     // Get Surface Size
//     EGLint width = 0, height = 0;
//     eglQuerySurface(display, surface, EGL_WIDTH, &width);
//     eglQuerySurface(display, surface, EGL_HEIGHT, &height);

//     // Create G-Buffer
//     unsigned int backbuffer;
//     glGenTextures(1, &backbuffer);
//     glBindTexture(GL_TEXTURE_2D, backbuffer);
//     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT,
//                  0);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

//     // Create Z-Buffer
//     unsigned int depthbuffer;
//     glGenTextures(1, &depthbuffer);
//     glBindTexture(GL_TEXTURE_2D, depthbuffer);
//     glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0,
//                  GL_DEPTH_COMPONENT, GL_FLOAT, 0);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

//     // Create Framebuffer
//     glGenFramebuffers(1, &gbuffer);
//     glBindFramebuffer(GL_FRAMEBUFFER, gbuffer);
//     glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
//                            backbuffer, 0);
//     glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
//                            depthbuffer, 0);
//     glDrawBuffers(2, (GLenum[]){ GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT });

//     // OpenGL Configuration
//     glEnable(GL_CULL_FACE);
//     glEnable(GL_DEPTH_TEST);
//     glClearColor(0.9, 0.9, 0.9, 1);

//     // Initialize Audio
//     SLObjectItf engine;
//     const SLboolean req[1] = { 0 };
//     slCreateEngine(&engine, 0, 0, 0, 0, 0);
//     (*engine)->Realize(engine, 0);
//     (*engine)->GetInterface(engine, SL_IID_ENGINE, &audioInterface);
//     (*audioInterface)->CreateOutputMix(audioInterface, &audioOutput, 1, 0, req);
//     (*audioOutput)->Realize(audioOutput, 0);
//   }
// }

// static int32_t engine_handle_input(struct android_app *app, AInputEvent *e)
// {
//   if (AInputEvent_getType(e) != AINPUT_EVENT_TYPE_MOTION) return 0;

//   int32_t action = AMotionEvent_getAction(e) & AMOTION_EVENT_ACTION_MASK;
//   float deltaXa = AMotionEvent_getX(e, 0) - prevX;
//   float deltaYa = AMotionEvent_getY(e, 0) - prevY;
//   int32_t isOne = AMotionEvent_getPointerCount(e) == 1;
//   int32_t isMove = deltaXa * deltaXa + deltaYa * deltaYa > 8 * 8;
//   int32_t isSame = prevId == AMotionEvent_getPointerId(e, 0);
//   int64_t isTap =
//       AMotionEvent_getEventTime(e) - AMotionEvent_getDownTime(e) <= 1.8E8;

//   if (action == AMOTION_EVENT_ACTION_MOVE && !isTap && isSame && isMove &&
//       isOne)
//   {
//     deltaX += deltaXa;
//     deltaY += deltaYa;
//   }
//   if (action == AMOTION_EVENT_ACTION_UP && isTap && isSame && !isMove && isOne)
//   {
//     clickX = deltaXa;
//     clickY = deltaYa;
//   }
//   if (action == AMOTION_EVENT_ACTION_DOWN)
//   {
//     prevId = AMotionEvent_getPointerId(e, 0);
//     prevX = AMotionEvent_getX(e, 0);
//     prevY = AMotionEvent_getY(e, 0);
//   }
//   return 0;
// }

// void android_main(struct android_app *app)
// {
//   app->onAppCmd = engine_handle_cmd;
//   app->onInputEvent = engine_handle_input;

//   // Start the Timer
//   struct timespec time;
//   clock_gettime(CLOCK_MONOTONIC, &time);
//   timerCurrent = (time.tv_sec * 10E8 + time.tv_nsec);
//   uint64_t lag = 0.0;

//   // Reset Deltas
//   clickX = clickY = deltaX = deltaY = 0.0f;

//   int events = 0;
//   struct android_poll_source *source;
//   while (!app->destroyRequested)
//   {
//     while (ALooper_pollAll(1, 0, &events, (void **)&source) >= 0)
//     {
//       if (source) source->process(app, source);
//     }

//     // Update Timer
//     clock_gettime(CLOCK_MONOTONIC, &time);
//     uint64_t timerNext = (time.tv_sec * 10E8 + time.tv_nsec);
//     uint64_t timerDelta = timerNext - timerCurrent;
//     timerCurrent = timerNext;

//     // Fixed updates
//     for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
//     {
//     }

//     // Reset Deltas
//     clickX = clickY = deltaX = deltaY = 0.0f;

//     // Renderer
//     glBindFramebuffer(GL_FRAMEBUFFER, gbuffer);
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//     glBindFramebuffer(GL_FRAMEBUFFER, 0);
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//     eglSwapBuffers(display, surface);
//   }
// }

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity,
                                        void *savedState, size_t savedStateSize)
{
  // Initialize State
  EGLDisplay display;
  EGLSurface surface;
  SLEngineItf audioInterface;
  SLObjectItf audioOutput;
  unsigned int gbuffer;
  int32_t prevId;
  float prevX = 0.0f, prevY = 0.0f, deltaX = 0.0f, deltaY = 0.0f, clickX = 0.0f, clickY = 0.0f;
  
  // Start the Timer
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  uint64_t timerCurrent = (time.tv_sec * 10E8 + time.tv_nsec);
  uint64_t lag = 0.0;

  activity->callbacks->onStart = [](ANativeActivity *activity){ };
  activity->callbacks->onResume = [](ANativeActivity *activity){ };
  activity->callbacks->onPause = [](ANativeActivity *activity){ };
  activity->callbacks->onStop = [](ANativeActivity *activity){ };
  activity->callbacks->onDestroy = [](ANativeActivity *activity){ };
  activity->callbacks->onNativeWindowCreated = [](ANativeActivity *activity, ANativeWindow *window){ };
  activity->callbacks->onNativeWindowDestroyed = [](ANativeActivity *activity, ANativeWindow *window){ };
  activity->callbacks->onNativeWindowRedrawNeeded = [](ANativeActivity *activity, ANativeWindow *window){ };
  activity->callbacks->onNativeWindowResized = [](ANativeActivity *activity, ANativeWindow *window){ };
  activity->callbacks->onInputQueueCreated = [](ANativeActivity *activity, AInputQueue *queue){ };
  activity->callbacks->onInputQueueDestroyed = [](ANativeActivity *activity, AInputQueue *queue){ };
  activity->callbacks->onWindowFocusChanged = [](ANativeActivity *activity, int focused){ };
  activity->callbacks->onContentRectChanged = [](ANativeActivity *activity, const ARect *rect){ };
  activity->callbacks->onConfigurationChanged = [](ANativeActivity *activity){ };
  activity->callbacks->onSaveInstanceState = [](ANativeActivity *activity, size_t *outLen){ };
  activity->callbacks->onLowMemory = [](ANativeActivity *activity){ };

  // Keep the screen turned on and bright
  // ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON, AWINDOW_FLAG_KEEP_SCREEN_ON);

  // eglInitialize(states->display, NULL, NULL);
  // getScreenSizeInPixels(activity, &states->screenSize.x, &states->screenSize.y);

  // Launch the main thread

}
