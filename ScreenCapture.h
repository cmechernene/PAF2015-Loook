#ifndef _SCREEN_H_
#define _SCREEN_H_

#include <windows.h>
#include <stdio.h>
#include <atlimage.h>
#include <time.h>

HRESULT ScreenCapture(int x, int y, int width, int height, char *filename);
#endif _SCREEN_H_