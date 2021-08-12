typedef struct
{
  double timerCurrent;
  float clickX, clickY, deltaX, deltaY;
  int mouseMode;
} gameState;

typedef struct
{
  short *data;
  unsigned int state, position, length;
} voice;

void update(gameState *);
