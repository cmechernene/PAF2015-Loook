#ifndef _KINECT_H_
#define _KINECT_H_

#pragma once

#include <Windows.h>
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
	/// Create the first connected Kinect found 
	/// </summary>
	/// <returns>S_OK on success, otherwise failure code</returns>
	HRESULT                 createFirstConnected();

	/// <summary>
	/// Handle new color and skeleton data
	/// </summary>
	BOOL				    process(unsigned char ** dest, u64 * time, int i);
	HRESULT					processColor(unsigned char ** dest, u64 * time);
	BOOL					processSkeleton(int i);

	void skelCoordToColorCoord(Vector4 skelCoords, LONG ** dest);
	void SaveSkeletonToFile(const NUI_SKELETON_DATA & skel, int i, u64 t, BOOL highlight);
	HRESULT SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath);
	void GetScreenshotFileName(wchar_t *screenshotName, UINT screenshotNameSize);

	//Background specific added functions
	HRESULT ChooseSkeleton(NUI_SKELETON_DATA* pSkeletonData);
	HRESULT Kinect::ComposeImage();
	HRESULT Kinect::processDepth();
	HRESULT Kinect::createBackgroundRemovedColorStream();

private:

	NUI_LOCKED_RECT m_LockedRect;

	// Current Kinect
	INuiSensor*             m_pNuiSensor;

	HANDLE                  m_pColorStreamHandle;
	HANDLE                  m_hNextColorFrameEvent;
	//From Skeleton basics
	HANDLE                  m_pSkeletonStreamHandle;
	HANDLE                  m_hNextSkeletonEvent;

	//Background Removal
	UINT                               m_depthWidth;
	UINT                               m_depthHeight;
	DWORD					m_trackedSkeleton;
	INuiBackgroundRemovedColorStream* m_pBackgroundRemovalStream;
	HANDLE					m_pDepthStreamHandle;
	HANDLE					m_hNextDepthFrameEvent;
	HANDLE					m_hNextBackgroundRemovedFrameEvent;

	BYTE*				    m_backgroundRGBX;
	BYTE*                   m_outputRGBX;
		
	u64						recentlyChanged;


	static const int        cColorWidth = 640;
	static const int        cColorHeight = 480;
	static const int        cScreenWidth = 320;
	static const int        cScreenHeight = 240;

	static const int        cStatusMessageMaxLen = MAX_PATH * 2;
};
#endif