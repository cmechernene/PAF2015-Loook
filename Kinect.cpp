#include "Kinect.h"
#include <sstream>
#include <strsafe.h>
#include <comdef.h>

extern u64 sys_start;

using namespace std;

NUI_IMAGE_FRAME imageFrame;

std::ostringstream skelString;
std::string temp;
std::ofstream skelPlaylist;

FLOAT x1R, y1R, z1R, x2R, y2R, z2R, x1L, y1L, z1L, x2L, y2L, z2L;
FLOAT dotR[3];
FLOAT dotL[3];
FLOAT distElbow = 0;

LPDWORD info;

//#define BACKGROUND

Kinect::Kinect() : 
	m_hNextColorFrameEvent(INVALID_HANDLE_VALUE), //sert a detecter un evenement camera video
	
	m_pColorStreamHandle(INVALID_HANDLE_VALUE),
	
	m_hNextSkeletonEvent(INVALID_HANDLE_VALUE), //sert a detecter un evenement squelette
	
	m_pSkeletonStreamHandle(INVALID_HANDLE_VALUE),
	
	m_pNuiSensor(NULL), //permet de choisir le capteur à utiliser (camera IR ou couleur)
	
	m_trackedSkeleton(NUI_SKELETON_INVALID_TRACKING_ID), //Background
	
	m_pBackgroundRemovalStream(NULL), //Background
	
	m_pDepthStreamHandle(INVALID_HANDLE_VALUE), // Background
	
	m_hNextDepthFrameEvent(INVALID_HANDLE_VALUE), // Background
	
	m_hNextBackgroundRemovedFrameEvent(INVALID_HANDLE_VALUE) // Background

{
	m_depthWidth = 320;
	m_depthHeight = 240;

	recentlyChanged = 1000000;

	printf("Creating events [CreateFirstConnected]\n");

	// Background. Event that will be signaled when depth data is available
	m_hNextDepthFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// Create an event that will be signaled when color data is available
	m_hNextColorFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	//From Skeleton basics. Create an event that'll be signaled when skeleton data is available
	m_hNextSkeletonEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	
	// Background : Event that will be signaled when the segmentation frame is ready
	m_hNextBackgroundRemovedFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// create heap storage for depth pixel data in RGBX format
	m_outputRGBX = new BYTE[640 * 480 * 4];
	m_backgroundRGBX = new BYTE[640 * 480 * 4];

	// Background create white picture
	for (int i = 0; i < 640 * 480 * 4; i += 4){
		m_backgroundRGBX[i] = 255;
		m_backgroundRGBX[i + 1] = 255;
		m_backgroundRGBX[i + 2] = 255;
		m_backgroundRGBX[i + 3] = 120;
	}

	createFirstConnected();

#ifdef BACKGROUND
	createBackgroundRemovedColorStream(); // Background removal
#endif
}


Kinect::~Kinect()
{

	// clean up arrays
	delete[] m_outputRGBX;
	delete[] m_backgroundRGBX;

	if (m_pNuiSensor)
		m_pNuiSensor->NuiShutdown(); // éteint la caméra de la kinect

	if (m_hNextColorFrameEvent != INVALID_HANDLE_VALUE)
		CloseHandle(m_hNextColorFrameEvent);

	if (m_hNextSkeletonEvent && (m_hNextSkeletonEvent != INVALID_HANDLE_VALUE))
		CloseHandle(m_hNextSkeletonEvent);

	CloseHandle(m_hNextBackgroundRemovedFrameEvent);
	CloseHandle(m_hNextDepthFrameEvent);

	m_pNuiSensor = NULL;
	m_pBackgroundRemovalStream = NULL;
}

BOOL Kinect::update(unsigned char ** dest, u64 * time, int i)
{
	BOOL b = false;
	if (m_pNuiSensor == NULL)
		return b = false;
	//printf("\n********************* update\n");

		//printf("debut process (kinect)\n");
		b = process(dest, time, i);
		//printf("sortie de process (kinect)\n");
	return b;
}

HRESULT Kinect::createFirstConnected()
{
	INuiSensor * pNuiSensor;
	HRESULT hr;

	int iSensorCount = 0;
	hr = NuiGetSensorCount(&iSensorCount); //compter le nombre de capteurs de la kinect et les mettre dans iSensorCount
	if (FAILED(hr))
	{
		return hr;
	}

	// Look at each Kinect sensor
	for (int i = 0; i < iSensorCount; ++i)
	{
		// Create the sensor so we can check status, if we can't create it, move on to the next
		hr = NuiCreateSensorByIndex(i, &pNuiSensor);
		if (FAILED(hr))
		{
			continue;
		}

		// Get the status of the sensor, and if connected, then we can initialize it
		hr = pNuiSensor->NuiStatus();
		if (hr == S_OK)
		{
			m_pNuiSensor = pNuiSensor;
			break;
		}

		// This sensor wasn't OK, so release it since we're not using it
		pNuiSensor->Release();
	}

	if (m_pNuiSensor != NULL)
	{
		printf("Sensor found [CreateFirstConnected]\n");

		// Initialize the Kinect and specify that we'll be using color AND SKELETON

		// NO BACKGROUND
#ifndef BACKGROUND
		hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_SKELETON); //Added NUI_INITIALIZE_FLAG_USES_SKELETON
#endif

		//WITH BACKGROUND
		//hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_SKELETON | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX);
#ifdef BACKGROUND		
		hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_SKELETON);
#endif

		if (SUCCEEDED(hr))
		{			
#ifndef BACKGROUND
			// Open a color image stream to receive color frames
			hr = m_pNuiSensor->NuiImageStreamOpen(
			NUI_IMAGE_TYPE_COLOR, // signifie qu'on utilise le RGB. Autres possibilités: https://msdn.microsoft.com/en-us/library/nuiimagecamera.nui_image_type.aspx
			NUI_IMAGE_RESOLUTION_640x480, //resolution fixee a 640*480. Autres possibilites (le 1280*960 ne marche pas sur la kinect de Telecom): https://msdn.microsoft.com/en-us/library/nuiimagecamera.nui_image_resolution.aspx
			0,
			2,
			m_hNextColorFrameEvent,
			&m_pColorStreamHandle);

			// Open a skeleton stream to receive skeleton data
			hr = m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, 0);
#endif
			
#ifdef BACKGROUND
			//Open depth image stream
			hr = m_pNuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
				NUI_IMAGE_RESOLUTION_320x240,
				0,
				2,
				m_hNextDepthFrameEvent,
				&m_pDepthStreamHandle);

			if (SUCCEEDED(hr)){
				//Open a color image stream
				printf("Opening a color image stream\n");
				hr = m_pNuiSensor->NuiImageStreamOpen(
					NUI_IMAGE_TYPE_COLOR,
					NUI_IMAGE_RESOLUTION_640x480,
					0,
					2,
					m_hNextColorFrameEvent,
					&m_pColorStreamHandle);

				if (SUCCEEDED(hr)){
					printf("Enabling the skeleton tracking\n");
					hr = m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE);
				}
			}

#endif

		}
	}
		if (NULL == m_pNuiSensor || FAILED(hr))
		{
			ResetEvent(m_hNextColorFrameEvent);
			ResetEvent(m_hNextSkeletonEvent);
			ResetEvent(m_hNextDepthFrameEvent); // BACKGROUND 
			cout << "Pas de Kinect détectée." << endl;
			return E_FAIL;
		}

	return hr;
}

HRESULT Kinect::createBackgroundRemovedColorStream(){
	HRESULT hr;
	if (NULL == m_pNuiSensor){
		return E_FAIL;
	}

	hr = NuiCreateBackgroundRemovedColorStream(m_pNuiSensor, &m_pBackgroundRemovalStream);
	if (FAILED(hr)){
		printf("Failed to createBackgroundRemovedColorStream\n");
		return hr;
	}

	hr = m_pBackgroundRemovalStream->Enable(NUI_IMAGE_RESOLUTION_640x480, NUI_IMAGE_RESOLUTION_320x240, m_hNextBackgroundRemovedFrameEvent);

	GetHandleInformation(m_pBackgroundRemovalStream, info);

	if (FAILED(hr)){
		printf("\t\t*** FAILED TO ENABLE BACKGROUND ***\n");
		return hr;
	}
	printf("Background REMOVAL ENABLED\n");
	return hr;
}

// Background : Compose the background removed color image with the background image
HRESULT Kinect::ComposeImage(){
	HRESULT hr;
	NUI_BACKGROUND_REMOVED_COLOR_FRAME removedFrame;

	// Gets the next frame of data from the background removed color stream
	hr = m_pBackgroundRemovalStream->GetNextFrame(1000, &removedFrame);

	GetHandleInformation(m_pBackgroundRemovalStream, info);

	if (FAILED(hr)){
		printf("FAILED getting the next background removed color frame\n");
		return hr;
	}

	// Gets a pointer to the first byte of the background removed color frame
	const BYTE* pBackgroundRemovedColor = removedFrame.pBackgroundRemovedColorData;

	// 4 = bytes per pixel
	int dataLength = static_cast<int>(cColorWidth)* static_cast<int>(cColorHeight)* 4;
	BYTE alpha = 0;
	
	for (int i = 0; i < dataLength; ++i){
		if (i % 4 == 0){
			// gets the alpha value of the current pixel
			alpha = pBackgroundRemovedColor[i + 3]; 
		}
		if (i % 4 != 3){
			//blending
			m_outputRGBX[i] = static_cast<BYTE>((UCHAR_MAX - alpha)*m_backgroundRGBX[i] 
				+ alpha*pBackgroundRemovedColor[i]) / UCHAR_MAX;
		}
	}

	printf("Debut BMP\n");
	static int ctr = 0;
	wchar_t buffer[256];
	std::swprintf(buffer, sizeof(buffer) / sizeof(*buffer), L"backgroundRemoved_%d.bmp", ctr);
	SaveBitmapToFile(static_cast<BYTE *>(m_outputRGBX), cColorWidth, cColorHeight, 32, buffer);
	ctr++;
	printf("Fin BMP\n");

	hr = m_pBackgroundRemovalStream->ReleaseFrame(&removedFrame);

	GetHandleInformation(m_pBackgroundRemovalStream, info);

	if (FAILED(hr)){
		return hr;
	}

	//hr = m_pDrawBackgroundRemovalBasics->Draw(m_outputRGBX, cColorHeight * cColorWidth * 4);
	return hr;
}

HRESULT Kinect::processColor(unsigned char ** dest, u64 * time){

	//printf("\t\tProcess Color\n");
	HRESULT hr;
	HRESULT bghr S_OK;

	// Glob var to skeleton conversion to rgb coordinates
	//NUI_IMAGE_FRAME imageFrame;

	// Background : part of the color frame with removed background
	LARGE_INTEGER colorTimeStamp;


	// Attempt to get the color frame
	hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pColorStreamHandle, 20, &imageFrame);
	if (FAILED(hr))
	{
		static int nbFail = 0;
		nbFail++;
		printf("\t\t\tfailed to process kinect color [total failed : %d]\n", nbFail);
		return hr;
	}

	colorTimeStamp = imageFrame.liTimeStamp;

	INuiFrameTexture* pTexture = imageFrame.pFrameTexture;

	// Length of the kinect color data buffer and number of bytes/line
	//printf("Length : %d, Pitch : %d\n", pTexture->BufferLen(), pTexture->Pitch());

	// Lock the frame data so the Kinect knows not to modify it while we're reading it
	pTexture->LockRect(0, &m_LockedRect, NULL, 0);

	if (m_LockedRect.Pitch != 0)
	{

#ifdef BACKGROUND
		bghr = m_pBackgroundRemovalStream->ProcessColor(cColorWidth*cColorHeight * 4, m_LockedRect.pBits, colorTimeStamp);		

		GetHandleInformation(m_pBackgroundRemovalStream, info);

		if (FAILED(bghr)){
			printf("FAILED processing color data\n");
			return bghr;
		}

#endif
		unsigned char * currFrame = (unsigned char *)m_LockedRect.pBits;
		*time = gf_sys_clock_high_res();

		// Convert RGBA to RGB
		int j = 0;
		for (int i = 0; i < cColorWidth * cColorHeight * 4; i += 4){
			(*dest)[i - j] = currFrame[i + 2];
			(*dest)[i - j + 1] = currFrame[i + 1];
			(*dest)[i - j + 2] = currFrame[i];
			j++;
		}

		// Conversion RGB -> YUV
		//unsigned char * yuvData = (unsigned char *)malloc(cColorWidth * cColorHeight * 3);
		//rgbaDataToYuv(&curr, &yuvData);

		//enregistrement frame en BMP
		/*
		printf("Debut BMP\n");
		static int ctr = 0;
		wchar_t buffer[256];
		std::swprintf(buffer, sizeof(buffer) / sizeof(*buffer), L"image_%d.bmp", ctr);
		SaveBitmapToFile(static_cast<BYTE *>(m_LockedRect.pBits), cColorWidth, cColorHeight, 32, buffer);
		ctr++;
		printf("Fin BMP\n");
		*/

		/*printf("début conversion...\n");
		static int nb = 0;
		char name[1024];
		sprintf(name, "image_%d.png", nb);
		if (!stbi_write_png(name, 640, 480, 3, curr, 4*640)){
		fprintf(stderr, "ERROR: could not write screenshot file %d\n", nb);
		}
		nb++;
		printf("nb = %d\n", nb);
		printf("fin conversion\n");
		*/
	}

	else
		cout << "Erreur de récupération de l'image." << endl;

	// We're done with the texture so unlock it
	pTexture->UnlockRect(0);

	// Release the frame
	hr = m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);
	if (FAILED(hr)){
		printf("Failed to release COLOR FRAME\n");
		return hr;
	}

	//printf("\t\tColor OK\n");
	return hr;
}

BOOL Kinect::processSkeleton(int k){
	HRESULT hr;
	BOOL savedSkelCoord = false;
	ofstream file;
	ostringstream destFile;

	NUI_SKELETON_FRAME skeletonFrame;
	hr = m_pNuiSensor->NuiSkeletonGetNextFrame(30, &skeletonFrame);
	u64 time = gf_sys_clock_high_res();
	
	if (FAILED(hr)){
		//printf("\t\t\tFAILED SKELETON\n");
		destFile.str("");
		destFile << "output\\skelcoord\\Coordinates_" << k << ".json";
		file.open(destFile.str());
		file.close();
		return false;
	}

	// Smooth the skeleton data ?
	m_pNuiSensor->NuiTransformSmooth(&skeletonFrame, NULL);

	// Holds skeleton data
	NUI_SKELETON_DATA * skeletonData = skeletonFrame.SkeletonData;

	//Choose the skeleton that we need to remove the background. May replace the for loop?
#ifdef BACKGROUND
	hr = ChooseSkeleton(skeletonData);
#endif

	if (FAILED(hr)){
		printf("FAILED To choose skeleton\n");
		return hr;
	}

	// Background removal processing
#ifdef BACKGROUND
	hr = m_pBackgroundRemovalStream->ProcessSkeleton(NUI_SKELETON_COUNT, skeletonData, skeletonFrame.liTimeStamp);

	GetHandleInformation(m_pBackgroundRemovalStream, info);
	
	if (FAILED(hr)){
		printf("\t\tFailed to Process skeleton BACKGROUND\n");
		return hr;
	}
#endif

	// Saving skel coordinates
	for (int i = 0; i < NUI_SKELETON_COUNT; i++){

		// Tests which skeleton is tracked -> sometimes we may not enter in the if block
		NUI_SKELETON_TRACKING_STATE trackingState = skeletonData[i].eTrackingState;
		if (NUI_SKELETON_TRACKED == trackingState){
			savedSkelCoord = true;
			//Draw the tracked skeleton
			//printf("\t\t\t1 Skeleton tracked\n");	
			SaveSkeletonToFile(skeletonData[i], k, time);
			hr = 0;

			// Gesture recognition
			x1R = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].x - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].x;
			y1R = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].y;
			z1R = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].z - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT].z;

			x1L = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].x - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].x;
			y1L = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].y - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].y;
			z1L = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].z - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT].z;

			x2R = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].x - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].x;
			y2R = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].y - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y;
			z2R = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].z - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].z;

			x2L = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].x - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].x;
			y2L = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].y - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].y;
			z2L = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].z - skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].z;

			
			dotR[0] = y1R*z2R - z1R*y2R;
			dotR[1] = z1R*x2R - x1R*z2R;
			dotR[2] = x1R*y2R - y1R*x2R;

			dotR[0] = y1L*z2L - z1L*y2L;
			dotR[1] = z1L*x2L - x1L*z2L;
			dotR[2] = x1L*y2L - y1L*x2L;

			//printf("Produit DROIT : %f, %f, %f\n\n", dotR[0], dotR[1], dotR[2]);
			/*if ((-0.0258 < dotR[0]) && (dotR[0] < 0.0258) && (-0.0258 < dotR[1]) && (dotR[1] < 0.0258) && (-0.0258 < dotR[2]) && (dotR[2] < 0.0258)){
				printf("\tProduit NUL\n\n");

				if ((-0.0258 < dotR[0]) && (dotR[0] < 0.0258) && (-0.0258 < dotR[1]) && (dotR[1] < 0.0258) && (-0.0258 < dotR[2]) && (dotR[2] < 0.0258)){

				}
			}
			else{
				printf("\n=/= 0\n");
			}
			*/

			Vector4 leftElbow = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT];
			Vector4 rightElbow = skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT];
			
			FLOAT distElbow = sqrt((leftElbow.x - rightElbow.x)*(leftElbow.x - rightElbow.x)
				+ (leftElbow.y - rightElbow.y)*(leftElbow.y - rightElbow.y)
				+ (leftElbow.z - rightElbow.z)*(leftElbow.z - rightElbow.z));

			/* MOVEMENT 1 ref
			if ((skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].x > skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].x)
				&& (skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].y > skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y)
				&& (skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].x < skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].x)
				&& (skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].y > skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].y)){
			*/	

			/* MOVEMENT 2 ref */
			printf("DIST %f\n", distElbow);
			if ((distElbow < 0.12)
				&& (skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT].y > skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y)
				&& (skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT].y > skeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT].y)
				){
				
				printf("SINCE LAST CHANGE : %llu\n", gf_sys_clock_high_res() - recentlyChanged);
				if ((gf_sys_clock_high_res() - recentlyChanged) > 300000){
					printf("\tCHANGE SLIDE\n");
					recentlyChanged = gf_sys_clock_high_res();
					return true;
				}
			}
			else{
				printf("STAY\n");
			}
		}
	}

	if (!savedSkelCoord){
		destFile.str("");
		destFile << "output\\skelcoord\\Coordinates_" << k << ".json";
		file.open(destFile.str());
		file.close();
	}

	return false;
}

// Background removal function
HRESULT Kinect::processDepth(){
	HRESULT hr;
	HRESULT bghr = S_OK;
	NUI_IMAGE_FRAME imageFrame;

	// Getting the depth frame
	LARGE_INTEGER depthTimeStamp;
	hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pDepthStreamHandle, 30, &imageFrame);
	if (FAILED(hr)){
		printf("FAILED getting the depth frame\n");
		return hr;
	}

	depthTimeStamp = imageFrame.liTimeStamp;
	INuiFrameTexture * texture;

	// Getting the extended depth texture;

	BOOL nearMode = false;
	hr = m_pNuiSensor->NuiImageFrameGetDepthImagePixelFrameTexture(
		m_pDepthStreamHandle, &imageFrame, &nearMode, &texture);
	if (FAILED(hr)){
		printf("FAILED getting the extended depth texture\n");
		return hr;
	}

	NUI_LOCKED_RECT lockedRect;
	
	//Lock the frame data to read it and don't let the kinect modify it
	texture->LockRect(0, &lockedRect, NULL, 0);

	// Make sure it is valid data before presenting it to the background removed color stream
	if (lockedRect.Pitch != 0){
		// Processes data from the kinect depth frame
		bghr = m_pBackgroundRemovalStream->ProcessDepth(
			m_depthWidth * m_depthHeight * 4, lockedRect.pBits, depthTimeStamp);
	}

	GetHandleInformation(m_pBackgroundRemovalStream, info);

	// Unlocking textures
	texture->UnlockRect(0);
	texture->Release();

	// Release the frame
	hr = m_pNuiSensor->NuiImageStreamReleaseFrame(m_pDepthStreamHandle, &imageFrame);
	
	if (FAILED(hr)){
		return hr;
	}

	if (FAILED(bghr)){
		printf("FAILED processing the kinect depth frame BACKGROUND\n");
		return bghr;
	}

	return hr;
}


BOOL Kinect::process(unsigned char ** dest, u64 * time, int i)
{
	BOOL b = true;

	if (NULL == m_pNuiSensor)
	{
		return E_FAIL;;
	}

	
	//printf("\tProcess\n");


	//if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextColorFrameEvent, 0)){
		processColor(dest, time);
	//}

	//printf("\t\tWait skeleton\n");
	//if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextSkeletonEvent, 0)){
		b = processSkeleton(i);
	//}

#ifdef BACKGROUND

	//printf("\t\tWait depth\n");
	//if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextDepthFrameEvent, 0)){
		processDepth(); // Background
	//}

	printf("\t\tWait Background\n");
	if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextBackgroundRemovedFrameEvent, 500)){
		ComposeImage(); // Background
	}

#endif	
	
	return b;
}

void Kinect:: skelCoordToColorCoord(Vector4 skelCoords, LONG ** dest){
	FLOAT  depthX=0;
	FLOAT  depthY=0;
	LONG  colorX=0;
	LONG  colorY=0;
	NuiTransformSkeletonToDepthImage(skelCoords, &depthX, &depthY);

	NuiImageGetColorPixelCoordinatesFromDepthPixel(NUI_IMAGE_RESOLUTION_640x480, &(imageFrame.ViewArea),
		(LONG)depthX, (LONG) depthY, 0, &colorX, &colorY);

	(*dest)[0] = colorX;
	(*dest)[1] = colorY;
	return;
}

void Kinect::SaveSkeletonToFile(const NUI_SKELETON_DATA & skel, int j, u64 time)
{

	FLOAT depthX =0;
	FLOAT depthY=0;
	LONG colorX=0;
	LONG colorY=0;

	std::string dest = "";
	std::ostringstream destination;

	//printf("\t\t\tSave Skeleton\n");
	std::string boneNames[20];
	std::ostringstream tmp;
	std::ostringstream coordString;
	std::ostringstream colorString;
	std::ostringstream resultStream;
	LONG * res = (LONG *)malloc(2 * sizeof(LONG));

	boneNames[0] = "Hip_Center";
	boneNames[1] = "Spine";
	boneNames[2] = "Shoulder_Center";
	boneNames[3] = "Head";
	boneNames[4] = "Shoulder_Left";
	boneNames[5] = "Elbow_Left";
	boneNames[6] = "Wrist_Left";
	boneNames[7] = "Hand_Left";
	boneNames[8] = "Shoulder_Right";
	boneNames[9] = "Elbow_Right";
	boneNames[10] = "Wrist_Right";
	boneNames[11] = "Hand_Right";
	boneNames[12] = "Hip_Left";
	boneNames[13] = "Knee_Left";
	boneNames[14] = "Ankle_Left";
	boneNames[15] = "Foot_Left";
	boneNames[16] = "Hip_Right";
	boneNames[17] = "Knee_Right";
	boneNames[18] = "Ankle_Right";
	boneNames[19] = "Foot_Right";

	resultStream << "{\"Skeleton\":[\n\t";

	for (int i = 0; i < 20; i++){
		coordString.str("");
		tmp.str("");
		colorString.str("");
		skelCoordToColorCoord(skel.SkeletonPositions[i], &res);

		colorString << "{\"X: \"" << res[0] << "\", \"Y\" : \"" << res[1] << "\"}";
		coordString << "{\"X\": \"" << skel.SkeletonPositions[i].x << "\", \"Y\": \"" << skel.SkeletonPositions[i].y << "\", \"Z\": \"" << skel.SkeletonPositions[i].z << "\"}";
		tmp << "\n\t{\n\t\"Name\":\"" << boneNames[i] << "\",\n\t\"Coordinates\":" << coordString.str() << ",\n\t\"Screen_Coordinates\":" << colorString.str() << "\n\t}\n\t";
		resultStream << tmp.str();
	}
	resultStream << "\n]}";
	ofstream myfile;
	destination.str("");
	destination << "output\\skelcoord\\Coordinates_" << j << ".json";
	dest = destination.str();

	myfile.open(dest);
	if (myfile.is_open()){
		myfile << resultStream.str();
		//printf("\t\t\twrote coordinates %d\n", j);
		myfile.close();
	}
	else{
		printf("\nERROR OPENING FILE\n");
	}
	free(res);
}


HRESULT Kinect::SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath)
{
	DWORD dwByteCount = lWidth * lHeight * (wBitsPerPixel / 8);

	BITMAPINFOHEADER bmpInfoHeader = { 0 };

	bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);  // Size of the header
	bmpInfoHeader.biBitCount = wBitsPerPixel;             // Bit count
	bmpInfoHeader.biCompression = BI_RGB;                    // Standard RGB, no compression
	bmpInfoHeader.biWidth = lWidth;                    // Width in pixels
	bmpInfoHeader.biHeight = -lHeight;                  // Height in pixels, negative indicates it's stored right-side-up
	bmpInfoHeader.biPlanes = 1;                         // Default
	bmpInfoHeader.biSizeImage = dwByteCount;               // Image size in bytes

	BITMAPFILEHEADER bfh = { 0 };

	bfh.bfType = 0x4D42;                                           // 'M''B', indicates bitmap
	bfh.bfOffBits = bmpInfoHeader.biSize + sizeof(BITMAPFILEHEADER);  // Offset to the start of pixel data
	bfh.bfSize = bfh.bfOffBits + bmpInfoHeader.biSizeImage;        // Size of image + headers

	// Create the file on disk to write to
	HANDLE hFile = CreateFileW(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	// Return if error opening file
	if (NULL == hFile)
	{
		return E_ACCESSDENIED;
	}

	DWORD dwBytesWritten = 0;

	// Write the bitmap file header
	if (!WriteFile(hFile, &bfh, sizeof(bfh), &dwBytesWritten, NULL))
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Write the bitmap info header
	if (!WriteFile(hFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &dwBytesWritten, NULL))
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Write the RGB Data
	if (!WriteFile(hFile, pBitmapBits, bmpInfoHeader.biSizeImage, &dwBytesWritten, NULL))
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Close the file
	CloseHandle(hFile);
	return S_OK;
}

void GetScreenshotFileName(wchar_t *screenshotName, UINT screenshotNameSize)
{
	wchar_t *knownPath = NULL;
	knownPath = L"C";


	static int ctr = 0;

		StringCchPrintfW(screenshotName, screenshotNameSize, L"%s\\KinectSnapshot-%d.bmp", knownPath, (++ctr));

	CoTaskMemFree(knownPath);
	return;
}

// BACKGROUND Determine the player whom the background removed color stream should consider as foreground
HRESULT Kinect::ChooseSkeleton(NUI_SKELETON_DATA* pSkeletonData){
	HRESULT hr = S_OK;

	// First we go through the stream to find the closest skeleton, and also check whether our current tracked
	// skeleton is still visibile in the stream
	float closestSkeletonDistance = FLT_MAX;
	DWORD closestSkeleton = NUI_SKELETON_INVALID_TRACKING_ID;
	BOOL isTrackedSkeletonVisible = false;
	for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
	{
		NUI_SKELETON_DATA skeleton = pSkeletonData[i];
		if (NUI_SKELETON_TRACKED == skeleton.eTrackingState)
		{
			if (m_trackedSkeleton == skeleton.dwTrackingID)
			{
				isTrackedSkeletonVisible = true;
				break;
			}

			if (skeleton.Position.z < closestSkeletonDistance)
			{
				closestSkeleton = skeleton.dwTrackingID;
				closestSkeletonDistance = skeleton.Position.z;
			}
		}
	}

	// Now we choose a new skeleton unless the currently tracked skeleton is still visible
	if (!isTrackedSkeletonVisible && closestSkeleton != NUI_SKELETON_INVALID_TRACKING_ID)
	{
		hr = m_pBackgroundRemovalStream->SetTrackedPlayer(closestSkeleton);

		GetHandleInformation(m_pBackgroundRemovalStream, info);

		if (FAILED(hr))
		{
			printf("FAILED CHOOSING SKELETON\n");
			return hr;
		}

		m_trackedSkeleton = closestSkeleton;
	}

	return hr;
}