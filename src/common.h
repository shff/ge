#include <math.h>

typedef struct
{
  float m[16];
} matrix;

typedef struct
{
  short *data;
  unsigned int state, position, length;
} voice;

matrix getProjectionMatrix(int w, int h, float fov, float near, float far)
{
  return { .m = { 1.0f / (tanf(fov * 3.14f / 180.0f / 2.0f) * w / h), 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f / tanf(fov * 3.14f / 180.0f / 2.0f),
                  0.0f, 0.0f, 0.0f, 0.0f, -(far + near) / (far - near), -1.0f,
                  0.0f, 0.0f, -(2.0f * far * near) / (far - near), 0.0f } };
}

matrix getViewMatrix(float camX, float camY, float posX, float posY, float posZ)
{
  return { .m = { cosf(camX), sinf(camX) * sinf(camY), sinf(camX) * cosf(camY),
                  0.0f, 0.0f, cosf(camY), -sinf(camY), 0.0f, -sinf(camX),
                  cosf(camX) * sinf(camY), cosf(camY) * cosf(camX), 0.0f,
                  -(cosf(camX) * posX - sinf(camX) * posZ),
                  -(sinf(camX) * sinf(camY) * posX + cosf(camY) * posY +
                    cosf(camX) * sinf(camY) * posZ),
                  -(sinf(camX) * cosf(camY) * posX - sinf(camY) * posY +
                    cosf(camY) * cosf(camX) * posZ),
                  1.0f } };
}
