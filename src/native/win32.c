#include "common.h"
#include <d3d11.h>
#include <dsound.h>
#include <windows.h>
#include <xinput.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "dsound")
#pragma comment(lib, "xinput")

gameState *state;
float mouseX, mouseY;
WINDOWPLACEMENT placement = {0};

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam,
                            LPARAM lParam)
{
  if (message == WM_LBUTTONDOWN)
  {
    mouseX = LOWORD(lParam);
    mouseY = HIWORD(lParam);
  }
  else if (message == WM_LBUTTONUP && state->deltaX + state->deltaY > 0.0f)
  {
    if (state->mouseMode != 1)
    {
      ClipCursor(NULL);
      SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
    }
    state->clickX = LOWORD(lParam);
    state->clickY = HIWORD(lParam);
  }
  else if (message == WM_MOUSEMOVE && wParam == state->mouseMode - 1)
  {
    RECT rect;
    GetWindowRect(window, &rect);
    ClipCursor(&rect);
    SetCursor(NULL);
    mouseX += (state->deltaX = LOWORD(lParam) - mouseX);
    mouseY += (state->deltaY = HIWORD(lParam) - mouseY);
  }
  else if (message == WM_DESTROY)
  {
    PostQuitMessage(0);
    return 0;
  }

  // Toggle fullscreen
  else if (message == WM_SYSKEYDOWN && wParam == VK_RETURN &&
           HIWORD(lParam) & KF_ALTDOWN)
  {
    DWORD windowStyle = GetWindowLong(window, GWL_STYLE);
    if (windowStyle & WS_OVERLAPPEDWINDOW)
    {
      GetWindowPlacement(window, &placement);
      SetWindowLong(window, GWL_STYLE, windowStyle & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(window, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                   GetSystemMetrics(SM_CYSCREEN),
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    else
    {
      SetWindowPlacement(window, &placement);
      SetWindowLong(window, GWL_STYLE, windowStyle | WS_OVERLAPPEDWINDOW);
      SetWindowPos(window, NULL, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                       SWP_FRAMECHANGED);
    }
  }
  return DefWindowProc(window, message, wParam, lParam);
}

int main(int argc, char const *argv[])
{
  state = malloc(sizeof(gameState));

  // Disable Screensaver
  SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);

  // Create Window
  HINSTANCE instance = GetModuleHandleW(NULL);
  RegisterClass(&(WNDCLASS){.lpfnWndProc = WindowProc,
                            .hInstance = GetModuleHandle(NULL),
                            .lpszClassName = "App"});
  HWND window = CreateWindowEx(0, "App", "App", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               CW_USEDEFAULT, NULL, NULL, instance, NULL);
  ShowWindow(window, SW_SHOWNORMAL);

  // Create DirectSound Device
  LPDIRECTSOUND dsound;
  DirectSoundCreate(0, &dsound, 0);
  dsound->lpVtbl->SetCooperativeLevel(dsound, window, DSSCL_PRIORITY);

  // Create Primary Audio Buffer
  DSBUFFERDESC bufferDesc1 = {.dwSize = sizeof(DSBUFFERDESC),
                              .dwFlags = DSBCAPS_PRIMARYBUFFER};
  LPDIRECTSOUNDBUFFER primaryBuffer;
  dsound->lpVtbl->CreateSoundBuffer(dsound, &bufferDesc1, &primaryBuffer, 0);
  WAVEFORMATEX format = {
      .wFormatTag = WAVE_FORMAT_PCM,
      .nChannels = 2,
      .nSamplesPerSec = 44100,
      .wBitsPerSample = 16,
      .nBlockAlign = 2 * 16 / 8,
      .nAvgBytesPerSec = 44100 * 2 * 16 / 8,
      .cbSize = 0,
  };
  primaryBuffer->lpVtbl->SetFormat(primaryBuffer, &format);

  // Create Secondary Audio Buffer
  DSBUFFERDESC bufferDesc2 = {
      .dwSize = sizeof(DSBUFFERDESC),
      .dwBufferBytes = 1024,
      .lpwfxFormat = &format,
  };
  LPDIRECTSOUNDBUFFER secondary_buffer;
  dsound->lpVtbl->CreateSoundBuffer(dsound, &bufferDesc2, &secondary_buffer, 0);

  // Create Direct3D Device and Swap-Chain
  DXGI_SWAP_CHAIN_DESC desc = {
      .BufferCount = 1,
      .BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .OutputWindow = window,
      .SampleDesc.Count = 4,
      .Windowed = TRUE,
  };
  IDXGISwapChain *swapchain = NULL;
  ID3D11Device *dev = NULL;
  ID3D11DeviceContext *context = NULL;
  D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL,
                                0, D3D11_SDK_VERSION, &desc, &swapchain, &dev,
                                NULL, &context);

  // Create G-Buffer
  D3D11_TEXTURE2D_DESC gBufferTexDesc = {
      .Width = 800,
      .Height = 600,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .SampleDesc.Count = 1,
      .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
  };
  D3D11_RENDER_TARGET_VIEW_DESC gBufferDesc = {
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
  };
  ID3D11Texture2D *gBufferTex = NULL;
  ID3D11RenderTargetView *gBuffer = NULL;
  dev->lpVtbl->CreateTexture2D(dev, &gBufferTexDesc, NULL, &gBufferTex);
  dev->lpVtbl->CreateRenderTargetView(dev, (ID3D11Resource *)gBufferTex,
                                      &gBufferDesc, &gBuffer);

  // Create Z-Buffer
  D3D11_TEXTURE2D_DESC zBufferTexDesc = {
      .Width = 800,
      .Height = 600,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
      .SampleDesc.Count = 1,
      .BindFlags = D3D11_BIND_DEPTH_STENCIL,
  };
  const D3D11_DEPTH_STENCIL_VIEW_DESC zBufferDesc = {
      .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
      .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
  };
  ID3D11Texture2D *zBufferTex = NULL;
  ID3D11DepthStencilView *zBuffer = NULL;
  dev->lpVtbl->CreateTexture2D(dev, &zBufferTexDesc, NULL, &zBufferTex);
  dev->lpVtbl->CreateDepthStencilView(dev, (ID3D11Resource *)zBufferTex,
                                      &zBufferDesc, &zBuffer);

  // Create the Backbuffer
  ID3D11Texture2D *bufferTex = NULL;
  ID3D11RenderTargetView *buffer = NULL;
  swapchain->lpVtbl->GetBuffer(swapchain, 0, &IID_ID3D11Texture2D,
                               (void **)&bufferTex);
  dev->lpVtbl->CreateRenderTargetView(dev, (ID3D11Resource *)bufferTex, NULL,
                                      &buffer);

  // Start the Timer
  long long timerResolution;
  QueryPerformanceFrequency((LARGE_INTEGER *)&timerResolution);
  QueryPerformanceCounter((LARGE_INTEGER *)&state->timerCurrent);
  state->lag = 0;

  // Reset Deltas
  state->deltaX = state->deltaY = state->clickX = state->clickY = 0.0f;
  mouseX = mouseY = 0.0f;

  MSG msg = {0};
  while (msg.message != WM_QUIT)
  {
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // Get joystick input
    XINPUT_STATE xState = {0};
    if (XInputGetState(0, &xState) == ERROR_SUCCESS)
    {
      state->deltaX += xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
      state->deltaX -= xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
      state->deltaY += xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
      state->deltaY -= xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
    }

    // Update Timer
    long long timerNext;
    QueryPerformanceCounter((LARGE_INTEGER *)&timerNext);
    double timerDelta =
        (timerNext - state->timerCurrent) * 10E8 / timerResolution;
    state->timerCurrent = timerNext;

    // Fixed updates
    for (state->lag += timerDelta; state->lag >= 1.0 / 60.0;
         state->lag -= 1.0 / 60.0)
    {
      update(state);
    }

    // Reset Deltas
    state->deltaX = state->deltaY = state->clickX = state->clickY = 0.0f;

    // Set Viewport and Blank Colors
    RECT rect;
    GetWindowRect(window, &rect);
    D3D11_VIEWPORT viewport = {0, 0, rect.right, rect.bottom, 1, 1000};
    float blankColor[4] = {0.0f, 0.2f, 0.4f, 1.0f};

    // Geometry Pass
    context->lpVtbl->OMSetRenderTargets(context, 1, &gBuffer, zBuffer);
    context->lpVtbl->RSSetViewports(context, 1, &viewport);
    context->lpVtbl->ClearRenderTargetView(context, gBuffer, blankColor);
    context->lpVtbl->ClearDepthStencilView(context, zBuffer, D3D11_CLEAR_DEPTH,
                                           1.0f, 0);

    // Final Pass
    context->lpVtbl->OMSetRenderTargets(context, 1, &buffer, zBuffer);
    context->lpVtbl->RSSetViewports(context, 1, &viewport);
    swapchain->lpVtbl->Present(swapchain, 0, 0);
  }

  swapchain->lpVtbl->Release(swapchain);
  gBuffer->lpVtbl->Release(gBuffer);
  gBufferTex->lpVtbl->Release(gBufferTex);
  zBuffer->lpVtbl->Release(zBuffer);
  zBufferTex->lpVtbl->Release(zBufferTex);
  buffer->lpVtbl->Release(buffer);
  bufferTex->lpVtbl->Release(bufferTex);
  dev->lpVtbl->Release(dev);
  context->lpVtbl->Release(context);

  // Re-enable Screensaver
  SetThreadExecutionState(ES_CONTINUOUS);

  return 0;
}
