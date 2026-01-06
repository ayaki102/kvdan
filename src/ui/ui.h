#ifndef UI_H
#define UI_H

#include <Adafruit_SH110X.h>

extern Adafruit_SH1106G display;

void drawHeader(const char *title, int current = -1, int total = -1);
void drawDecorativeLine();
void drawNavigationDots();
void drawSelectionBox(int x, int y, int w, int h);
void slideAnimation(void (*drawCurrent)(int), void (*drawPrevious)(int),
                    int current, int previous, bool slideRight);
void scanAnimation();
void startupAnimation();
void submenuEnterAnimation();

#endif
