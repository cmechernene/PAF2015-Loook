#include "ScreenCapture.h"

HRESULT ScreenCapture(int x, int y, int width, int height, char *filename){

	printf("\t\tCapturing screen\n");
	// get a Device Context
	HDC dCHandler = CreateCompatibleDC(0);

	// create a BMP in memory to store the capture. 1st param : handle to DC
	HBITMAP hBmp = CreateCompatibleBitmap(GetDC(0), width, height);

	// Join both handlers
	SelectObject(dCHandler, hBmp);

	// copy from the screen to my bitmap
	// 1 : destination device context
	// 2,3 : x & y coordinates of the upper left of the destination rectangle
	// 4, 5 : width, height of the source and destination rectangles
	// 6 : handle to the source device context
	// 7, 8 : x & y coordinates of the upper left of the source rectangle
	// SRCCOPY : copies the source rectangle directly to the destination rectangle
	BitBlt(dCHandler, 0, 0, width, height, GetDC(0), x, y, SRCCOPY);

	CImage resultImage;
	resultImage.Attach(hBmp);
	CString str = (CString)filename;
	HRESULT hr = resultImage.Save(str);

	// free the bitmap mem
	DeleteObject(hBmp);

	return hr;
}
/*
void main(){
	float time = clock();
	char * fileDest = "C:\\Users\\Martin\\ColorBasics-D2D\\test.png";
	ScreenCapture(0, 0, 1920, 1080, fileDest);
	time = (clock() - time) / CLOCKS_PER_SEC;
	printf("wrote to %s\n\tIn %f s\n\n", fileDest, time);
	system("PAUSE");
	return;
}
*/