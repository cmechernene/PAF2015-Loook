#pragma once

#include <Windows.h>
//#include <glew.h>
#include <strsafe.h>
#include <iostream>
#include <fstream>
#include "resource.h"
#include <NuiApi.h>
#include <KinectBackgroundRemoval.h> //Background

extern "C"{
#include <gpac/tools.h>
}

class Kinect
{
public:
	Kinect();
	~Kinect();

	/// <summary>
	/// Main processing function
	/// </summary>
	void                    update(unsigned char ** des, u64 * time);

	/// <summary>
	/// Create the first connected Kinect found 
	/// </summary>
	/// <returns>S_OK on success, otherwise failure code</returns>
	HRESULT                 createFirstConnected();

	/// <summary>
	/// Handle new color and skeleton data
	/// </summary>
	HRESULT                 process(unsigned char ** dest, u64 * time);
	HRESULT					processColor(unsigned char ** dest, u64 * time);
	HRESULT					processSkeleton();

	void skelCoordToColorCoord(Vector4 skelCoords, LONG ** dest);
	void SaveSkeletonToFile(const NUI_SKELETON_DATA & skel, int windowWidth, int windowHeight);
	HRESULT SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath);
	void GetScreenshotFileName(wchar_t *screenshotName, UINT screenshotNameSize);
	void rgbaDataToYuv(unsigned char ** oldData, unsigned char ** newData);

	//Background specific added functions
	HRESULT ChooseSkeleton(NUI_SKELETON_DATA* pSkeletonData);

private:

	NUI_LOCKED_RECT m_LockedRect;

	//bool					m_SeatedMode;

	// Current Kinect
	INuiSensor*             m_pNuiSensor;

	HANDLE                  m_pColorStreamHandle;
	HANDLE                  m_hNextColorFrameEvent;
	//From Skeleton basics
	HANDLE                  m_pSkeletonStreamHandle;
	HANDLE                  m_hNextSkeletonEvent;

	//Background Removal
	DWORD					m_trackedSkeleton;
	INuiBackgroundRemovedColorStream* m_pBackgroundRemovalStream;

	static const int        cColorWidth = 640;
	static const int        cColorHeight = 480;
	static const int        cScreenWidth = 320;
	static const int        cScreenHeight = 240;

	static const int        cStatusMessageMaxLen = MAX_PATH * 2;
};

