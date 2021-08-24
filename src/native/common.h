#include <math.h>

typedef struct
{
  float m[16];
} matrix;

typedef struct
{
  double timerCurrent;
  float clickX, clickY, deltaX, deltaY;
  int mouseMode;
  float posX, posY, posZ, camX, camY;
} gameState;

typedef struct
{
  short *data;
  unsigned int state, position, length;
} voice;

void update(gameState *);

matrix getProjectionMatrix(int w, int h, float fov, float near, float far)
{
  return (
      matrix){ .m = { [0] = 1.0f / (tanf(fov * 3.14f / 180.0f / 2.0f) * w / h),
                      [5] = 1.0f / tanf(fov * 3.14f / 180.0f / 2.0f),
                      [10] = -(far + near) / (far - near),
                      [11] = -1.0f,
                      [14] = -(2.0f * far * near) / (far - near) } };
}

matrix getViewMatrix(gameState s)
{
  return (matrix){
    .m = { cosf(s.camX), sinf(s.camX) * sinf(s.camY),
           sinf(s.camX) * cosf(s.camY), 0.0f, 0.0f, cosf(s.camY), -sinf(s.camY),
           0.0f, -sinf(s.camX), cosf(s.camX) * sinf(s.camY),
           cosf(s.camY) * cosf(s.camX), 0.0f,
           -(cosf(s.camX) * s.posX - sinf(s.camX) * s.posZ),
           -(sinf(s.camX) * sinf(s.camY) * s.posX + cosf(s.camY) * s.posY +
             cosf(s.camX) * sinf(s.camY) * s.posZ),
           -(sinf(s.camX) * cosf(s.camY) * s.posX - sinf(s.camY) * s.posY +
             cosf(s.camY) * cosf(s.camX) * s.posZ),
           1.0f }
  };
}
