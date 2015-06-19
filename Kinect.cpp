#include "Kinect.h"
#include <sstream>
#include <strsafe.h>

using namespace std;

double RightHandX;
double RightHandY;

NUI_IMAGE_FRAME imageFrame;

Kinect::Kinect() : m_hNextColorFrameEvent(INVALID_HANDLE_VALUE), //sert a detecter un evenement camera video
m_pColorStreamHandle(INVALID_HANDLE_VALUE),
m_hNextSkeletonEvent(INVALID_HANDLE_VALUE), //sert a detecter un evenement squelette
m_pSkeletonStreamHandle(INVALID_HANDLE_VALUE),
m_pNuiSensor(NULL), //permet de choisir le capteur à utiliser (camera IR ou couleur)
m_trackedSkeleton(NUI_SKELETON_INVALID_TRACKING_ID), //Background
m_pBackgroundRemovalStream(NULL) //Background

{
	createFirstConnected();
}


Kinect::~Kinect()
{
	if (m_pNuiSensor)
		m_pNuiSensor->NuiShutdown(); // éteint la caméra de la kinect

	if (m_hNextColorFrameEvent != INVALID_HANDLE_VALUE)
		CloseHandle(m_hNextColorFrameEvent);

	if (m_hNextSkeletonEvent && (m_hNextSkeletonEvent != INVALID_HANDLE_VALUE))
		CloseHandle(m_hNextSkeletonEvent);

	m_pNuiSensor = NULL;
}

HRESULT Kinect::update(unsigned char ** dest, u64 * time, int i)
{
	HRESULT hr = true;
	if (m_pNuiSensor == NULL)
		return hr = E_FAIL;
	printf("********************* update\n");
	//if (WaitForSingleObject(m_hNextColorFrameEvent, 0))
	//{
		printf("debut process (kinect)\n");
		hr = process(dest, time, i);//permet de mettre à jour le process (detection de la video en couleur et squelette)
		printf("sortie de process (kinect)\n");
	//}
	return hr;
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
		// Initialize the Kinect and specify that we'll be using color AND SKELETON
		hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_SKELETON); //Added NUI_INITIALIZE_FLAG_USES_SKELETON
		
		
		// if preivous line not working, use this :
		// HRESULT hr = m_pSensorChooser->GetSensor(NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_COLOR, &m_pNuiSensor);
		
		if (SUCCEEDED(hr))
		{
			// Create an event that will be signaled when color data is available
			m_hNextColorFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			
			//From Skeleton basics. Create an event that'll be signaled when skeleton data is available
			m_hNextSkeletonEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

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

#ifdef BACKGROUND
			//Open depth image stream
			hr = m_pNuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
				cDepthResolution,
				0,
				2,
				m_hNextDepthFrameEvent,
				&m_pDepthStreamHandle);

			if (SUCEEDED(hr)){
				//Open a color image stream
				hr = m_pNuiSensor->NuiImageStreamOpen(
					NUI_IMAGE_TYPE_COLOR,
					cColorResolution,
					0,
					2,
					m_hNextColorFrameEvent,
					&m_pColorStreamHandle);
			}
			
			if (SUCEEDED(hr)){
				hr = m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonFrameEvent, NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE);
			}
#endif

		}
		else
		{
			ResetEvent(m_hNextColorFrameEvent);
			ResetEvent(m_hNextSkeletonEvent);
			// BACKGROUND ResetEvent(m_hNextDepthFrameEvent);
		}
	}

	if (m_pNuiSensor == NULL || FAILED(hr))
	{
		cout << "Pas de Kinect détectée." << endl;
		return E_FAIL;
	}

	return hr;
}

HRESULT Kinect::processColor(unsigned char ** dest, u64 * time){

	printf("\t\tProcess Color\n");
	HRESULT hr;
	// Glob var to skeleton conversion to rgb coordinates
	//NUI_IMAGE_FRAME imageFrame;

	LARGE_INTEGER colorTimeStamp; // Background : part of the color frame with removed background

	// Attempt to get the color frame
	hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pColorStreamHandle, 40, &imageFrame);
	if (FAILED(hr))
	{
		static int nbFail = 0;
		nbFail++;
		printf("\t\t\tfailed to process kinect color [total failed : %d]\n", nbFail);
		return hr;
	}

	INuiFrameTexture* pTexture = imageFrame.pFrameTexture;

	// Length of the kinect color data buffer and number of bytes/line
	//printf("Length : %d, Pitch : %d\n", pTexture->BufferLen(), pTexture->Pitch());

	// Lock the frame data so the Kinect knows not to modify it while we're reading it
	pTexture->LockRect(0, &m_LockedRect, NULL, 0);

	if (m_LockedRect.Pitch != 0)
	{
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

		// Background specific :
//		HRESULT bg = m_pBackgroundRemovalStream->ProcessColor(cColorWidth*cColorHeight);
	}

	else
		cout << "Erreur de récupération de l'image." << endl;

	// We're done with the texture so unlock it
	pTexture->UnlockRect(0);

	// Release the frame
	m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);

	// Device lost, need to recreate the render target
	// We'll dispose it now and retry drawing
	m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);
}

HRESULT Kinect::processSkeleton(int k){
	HRESULT hr;

	NUI_SKELETON_FRAME skeletonFrame;
	hr = m_pNuiSensor->NuiSkeletonGetNextFrame(30, &skeletonFrame);
	if (FAILED(hr)){
		printf("\t\t\tFAILED SKELETON\n");
	}

	// Smooth the skeleton data ?
	m_pNuiSensor->NuiTransformSmooth(&skeletonFrame, NULL);

	for (int i = 0; i < NUI_SKELETON_COUNT; i++){

		// Tests which skeleton is tracked -> sometimes we may not enter in the if block
		NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;
		if (NUI_SKELETON_TRACKED == trackingState){
			//Draw the tracked skeleton
			printf("\t\t\t1 Skeleton tracked\n");
			SaveSkeletonToFile(skeletonFrame.SkeletonData[i], k);
			hr = 0;
		}
	}
	return hr;
}


HRESULT Kinect::process(unsigned char ** dest, u64 * time, int i)
{
	HRESULT hr = true;
	
	printf("\tProcess\n");
	processColor(dest, time);
	hr = processSkeleton(i);

	return hr;
}

void Kinect:: skelCoordToColorCoord(Vector4 skelCoords, LONG ** dest){
	FLOAT  depthX=0;
	FLOAT  depthY=0;
	LONG  colorX=0;
	LONG  colorY=0;
	NuiTransformSkeletonToDepthImage(skelCoords, &depthX, &depthY);

	NuiImageGetColorPixelCoordinatesFromDepthPixel(NUI_IMAGE_RESOLUTION_640x480, &(imageFrame.ViewArea),
		depthX, depthY, 0, &colorX, &colorY);

	(*dest)[0] = colorX;
	(*dest)[1] = colorY;
	return;
}

void Kinect::SaveSkeletonToFile(const NUI_SKELETON_DATA & skel, int j)
{

	FLOAT depthX =0;
	FLOAT depthY=0;
	LONG colorX=0;
	LONG colorY=0;

	std::string dest = "";
	std::ostringstream destination;

	printf("\t\t\tSave Skeleton\n");
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
		printf("\t\t\twrote coordinates %d\n", j);
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

/*
void rgbaDataToYuv(unsigned char ** oldData, unsigned char ** newData){
	for (int i = 0; i < 640 * 480 * 4; i += 4){
		*newData[i] = (unsigned char)((0.299 * (*oldData[i]) + 0.587 * (*oldData[i+1]) + 0.114 * (*oldData[i+2])) * 219/255)+16;
		*newData[i + 1] = (unsigned char)((-0.299 * *oldData[i] - 0.587 * *oldData[i+1] + 0.886 * *oldData[i+2]) * 224/1.772/255) + 128;
		*newData[i + 2] = (unsigned char)((0.701 * *oldData[i] - 0.587 * *oldData[i+1] - 0.114 * *oldData[i+2]) * 224/1.402/255) + 128;
	}
	return;
}
*/

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
		if (FAILED(hr))
		{
			return hr;
		}

		m_trackedSkeleton = closestSkeleton;
	}

	return hr;
}