#include "common.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dsound.h>
#include <windows.h>
#include <xinput.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "dsound")
#pragma comment(lib, "xinput")

float mouseX, mouseY, clickX, clickY, deltaX, deltaY;
int mouseMode = 2;
long long timerCurrent;
WINDOWPLACEMENT placement = { 0 };

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam,
                            LPARAM lParam)
{
  if (message == WM_LBUTTONDOWN)
  {
    mouseX = LOWORD(lParam);
    mouseY = HIWORD(lParam);
  }
  else if (message == WM_LBUTTONUP && deltaX + deltaY > 0.0f)
  {
    if (mouseMode != 1)
    {
      ClipCursor(NULL);
      SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
    }
    clickX = LOWORD(lParam);
    clickY = HIWORD(lParam);
  }
  else if (message == WM_MOUSEMOVE && wParam == mouseMode - 1)
  {
    RECT rect;
    GetWindowRect(window, &rect);
    ClipCursor(&rect);
    SetCursor(NULL);
    mouseX += (deltaX = LOWORD(lParam) - mouseX);
    mouseY += (deltaY = HIWORD(lParam) - mouseY);
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
  // Disable Screensaver
  SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);

  // Create Window
  HINSTANCE instance = GetModuleHandleW(NULL);
  WNDCLASS windowClass = {};
  windowClass.lpfnWndProc = WindowProc;
  windowClass.hInstance = GetModuleHandle(NULL);
  windowClass.lpszClassName = "App";
  RegisterClass(&windowClass);
  HWND window = CreateWindowEx(0, "App", "App", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               CW_USEDEFAULT, NULL, NULL, instance, NULL);
  ShowWindow(window, SW_SHOWNORMAL);

  // Create DirectSound Device
  LPDIRECTSOUND dsound;
  DirectSoundCreate(0, &dsound, 0);
  dsound->SetCooperativeLevel(window, DSSCL_PRIORITY);

  // Create Primary Audio Buffer
  DSBUFFERDESC bufferDesc1 = { .dwSize = sizeof(DSBUFFERDESC),
                               .dwFlags = DSBCAPS_PRIMARYBUFFER };
  LPDIRECTSOUNDBUFFER primaryBuffer;
  dsound->CreateSoundBuffer(&bufferDesc1, &primaryBuffer, 0);

  // Set Buffer Format
  WAVEFORMATEX format = {};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = 44100;
  format.wBitsPerSample = 16;
  format.nBlockAlign = 2 * 16 / 8;
  format.nAvgBytesPerSec = 44100 * 2 * 16 / 8;
  format.cbSize = 0;
  primaryBuffer->SetFormat(&format);

  // Create Secondary Audio Buffer
  DSBUFFERDESC bufferDesc2 = {};
  bufferDesc2.dwSize = sizeof(DSBUFFERDESC);
  bufferDesc2.dwBufferBytes = 1024;
  bufferDesc2.lpwfxFormat = &format;
  LPDIRECTSOUNDBUFFER secondary_buffer;
  dsound->CreateSoundBuffer(&bufferDesc2, &secondary_buffer, 0);

  // Create Direct3D Device and Swap-Chain
  DXGI_SWAP_CHAIN_DESC desc = {};
  desc.BufferCount = 1;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.OutputWindow = window;
  desc.SampleDesc.Count = 4;
  desc.Windowed = TRUE;
  IDXGISwapChain *swapchain = NULL;
  ID3D11Device *dev = NULL;
  ID3D11DeviceContext *context = NULL;
  D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL,
                                0, D3D11_SDK_VERSION, &desc, &swapchain, &dev,
                                NULL, &context);

  // Create G-Buffer Texture
  D3D11_TEXTURE2D_DESC gBufferTexDesc = {};
  gBufferTexDesc.Width = 800;
  gBufferTexDesc.Height = 600;
  gBufferTexDesc.MipLevels = 1;
  gBufferTexDesc.ArraySize = 1;
  gBufferTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  gBufferTexDesc.SampleDesc.Count = 1;
  gBufferTexDesc.BindFlags =
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  ID3D11Texture2D *gBufferTex = NULL;
  dev->CreateTexture2D(&gBufferTexDesc, NULL, &gBufferTex);

  // Create Z-Buffer Texture
  D3D11_TEXTURE2D_DESC zBufferTexDesc = {};
  zBufferTexDesc.Width = 800;
  zBufferTexDesc.Height = 600;
  zBufferTexDesc.MipLevels = 1;
  zBufferTexDesc.ArraySize = 1;
  zBufferTexDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  zBufferTexDesc.SampleDesc.Count = 1;
  zBufferTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  ID3D11Texture2D *zBufferTex = NULL;
  dev->CreateTexture2D(&zBufferTexDesc, NULL, &zBufferTex);

  // Create G-Buffer
  D3D11_RENDER_TARGET_VIEW_DESC gBufferDesc = {};
  gBufferDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  gBufferDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  ID3D11RenderTargetView *gBuffer = NULL;
  dev->CreateRenderTargetView((ID3D11Resource *)gBufferTex, &gBufferDesc,
                              &gBuffer);

  // Create Z-Buffer
  D3D11_DEPTH_STENCIL_VIEW_DESC zBufferDesc = {};
  zBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  zBufferDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
  ID3D11DepthStencilView *zBuffer = NULL;
  dev->CreateDepthStencilView((ID3D11Resource *)zBufferTex, &zBufferDesc,
                              &zBuffer);

  // Create the Backbuffer
  ID3D11Texture2D *bufferTex = NULL;
  ID3D11RenderTargetView *buffer = NULL;
  swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bufferTex);
  dev->CreateRenderTargetView((ID3D11Resource *)bufferTex, NULL, &buffer);

  // Create Post-Processing Vertex Shader
  ID3D11VertexShader *postVertexShader = NULL;
  ID3D10Blob *postVertexShaderBlob = NULL;
  D3DCompileFromFile(L"shaders.hlsl", NULL, NULL, "post_vs", "vs_4_0", 0, 0,
                     &postVertexShaderBlob, NULL);
  dev->CreateVertexShader(postVertexShaderBlob->GetBufferPointer(),
                          postVertexShaderBlob->GetBufferSize(), NULL,
                          &postVertexShader);

  // Create Post-Processing Pixel Shader
  ID3D11PixelShader *postPixelShader = NULL;
  ID3D10Blob *postPixelShaderBlob = NULL;
  D3DCompileFromFile(L"shaders.hlsl", NULL, NULL, "post_fs", "ps_4_0", 0, 0,
                     &postPixelShaderBlob, NULL);
  dev->CreatePixelShader(postPixelShaderBlob->GetBufferPointer(),
                         postPixelShaderBlob->GetBufferSize(), NULL,
                         &postPixelShader);

  // Create Post-Processing Input Layout
  D3D11_INPUT_ELEMENT_DESC postInputDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
      D3D11_INPUT_PER_VERTEX_DATA, 0 }
  };
  ID3D11InputLayout *postInputLayout = NULL;
  dev->CreateInputLayout(
      postInputDesc, 1, postVertexShaderBlob->GetBufferPointer(),
      postVertexShaderBlob->GetBufferSize(), &postInputLayout);

  // Start the Timer
  long long timerResolution;
  QueryPerformanceFrequency((LARGE_INTEGER *)&timerResolution);
  QueryPerformanceCounter((LARGE_INTEGER *)&timerCurrent);
  double lag = 0;

  // Reset Deltas
  deltaX = deltaY = clickX = clickY = 0.0f;
  mouseX = mouseY = 0.0f;

  MSG msg = { 0 };
  while (msg.message != WM_QUIT)
  {
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // Get joystick input
    XINPUT_STATE xState = { 0 };
    if (XInputGetState(0, &xState) == ERROR_SUCCESS)
    {
      deltaX += xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
      deltaX -= xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
      deltaY += xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
      deltaY -= xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
    }

    // Update Timer
    long long timerNext;
    QueryPerformanceCounter((LARGE_INTEGER *)&timerNext);
    double timerDelta = (timerNext - timerCurrent) * 10E8 / timerResolution;
    timerCurrent = timerNext;

    // Fixed updates
    for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
    {
    }

    // Reset Deltas
    deltaX = deltaY = clickX = clickY = 0.0f;

    // Set Viewport and Blank Colors
    RECT rect;
    GetWindowRect(window, &rect);
    D3D11_VIEWPORT viewport = { 0, 0, rect.right, rect.bottom, 1, 1000 };
    float blankColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

    // Geometry Pass
    context->OMSetRenderTargets(1, &gBuffer, zBuffer);
    context->RSSetViewports(1, &viewport);
    context->ClearRenderTargetView(gBuffer, blankColor);
    context->ClearDepthStencilView(zBuffer, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Final Pass
    context->OMSetRenderTargets(1, &buffer, zBuffer);
    context->RSSetViewports(1, &viewport);
    context->VSSetShader(postVertexShader, 0, 0);
    context->PSSetShader(postPixelShader, 0, 0);
    context->IASetInputLayout(postInputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->DrawIndexed(4, 0, 0);
    swapchain->Present(0, 0);
  }

  postVertexShader->Release();
  postPixelShader->Release();
  gBuffer->Release();
  gBufferTex->Release();
  zBuffer->Release();
  zBufferTex->Release();
  buffer->Release();
  bufferTex->Release();
  swapchain->Release();
  dev->Release();
  context->Release();

  // Re-enable Screensaver
  SetThreadExecutionState(ES_CONTINUOUS);

  return 0;
}
