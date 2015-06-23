#include "Kinect.h"
#include "Encoder.h"
#include "ScreenCapture.h"
#include <sstream>
#include <windows.h>

u64 sys_start;

int main()
{
	int i, nb_test_frames;
	DASHOutputFile *muxer;
	int width = 640;
	int height = 480;
	int frame_per_segment = 30;
	int frame_duration = 1;
	int segment_duration = 30;
	int bitrate = 500000;
	int seg_dur_in_ms = 1000;
	int seg_num = 1;
	u32 data_size = width * height * 3;

	u64 now;
	u64 timeref;
	u64 timeScreenshot =0.0;
	u64 lastScreenshot = 0.0;

	std::ostringstream destStream;
	std::string destName;

	std::ostringstream vidListStream;
	std::ofstream vidPlaylist;
	std::string tmp;

	std::ostringstream imListStream;
	int im_num = 0;
	int im_refJSON = 1;

	std::ostringstream skelListStream;

	vidListStream << "seg_init_gpac.mp4\n";

	BOOL resKinect;
	INPUT newSlideInput;
	
	u8 *data = (u8 *)malloc(data_size);

	//make a white frame
	//memset(data, 0xFF, data_size);

	Kinect kinect;

	// Color data buffer
	unsigned char * kinectFrame = (unsigned char *)malloc(data_size);

	gf_sys_init(GF_FALSE);
	//Sleep(3000);

	// Init DASHOutputFile : frame_per_segment, frame_dur, alloc avframe, alloc buffer, codec context, ...
	// OLD muxer = muxer_init(seg_dur_in_ms, 33333, 1000000, 30, width, height, bitrate, GF_TRUE);

	muxer = muxer_init(seg_dur_in_ms, 1, 1000000, 30, width, height, bitrate, GF_TRUE);

	if (!muxer) {
		fprintf(stderr, "Error initializing muxer\n");
		return 1;
	}
	//try to generate 4 seconds (eg 4 segments)
	nb_test_frames = 30 * 4;

	sys_start = gf_sys_clock_high_res();

	for (i = 0; i<nb_test_frames; i++) {
		u64 pts;

		// Update kinectFrameData 
		BOOL resKinect = kinect.update(&kinectFrame, &now, i);

		// If gesture is detected, switching slides, preparing image playlist
		if (resKinect){
			printf("Changed Slide\n");
			newSlideInput.type = INPUT_KEYBOARD;
			newSlideInput.ki.wScan = 0;
			newSlideInput.ki.time = 0;
			newSlideInput.ki.dwExtraInfo = 0;

			// PRESS right key
			newSlideInput.ki.wVk = 0x41;//VK_RIGHT; // right arrow key
			newSlideInput.ki.dwFlags = 0; // key press
			SendInput(1, &newSlideInput, sizeof(INPUT));

			// RELEASE right key
			newSlideInput.ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(1, &newSlideInput, sizeof(INPUT));

			// Make sure we really switched slides before capturing screen
			Sleep(100);
			destStream.str("");
			destStream << "C:\\Users\\Martin\\ColorBasics-D2D\\output\\screen\\im_" << im_num << ".png";
			destName = destStream.str();
			ScreenCapture(0, 0, 1920, 1080, (char *)destName.c_str());
			lastScreenshot = timeScreenshot;
			timeScreenshot = gf_sys_clock_high_res() - sys_start;
			im_num++;
			
			if (im_refJSON == 1){
				imListStream << " \"" << im_refJSON << "\":" << "im_" << im_num << ".png\", \"Time_after_segment\": \"" << timeScreenshot - lastScreenshot << "\"";
			}
			else{
				imListStream << ", \"" << im_refJSON << "\":" << "im_" << im_num << ".png\", \"Time_after_segment\": \"" << timeScreenshot - lastScreenshot << "\"";
			}
			im_refJSON++;
		}

		if ((i % 30 + 1) == 30){
			skelListStream << "\"" << i % 30 + 1 << "\": \"Coordinates_" << i << ".json\"\n\t\t\t\t";
		}
		else{
			skelListStream << "\"" << i % 30 + 1 << "\": \"Coordinates_" << i << ".json\",\n\t\t\t\t";
		}
		pts = gf_sys_clock_high_res() - sys_start;

		// OLD int res = muxer_encode(muxer, kinectFrame, data_size, pts);
		int res = muxer_encode(muxer, kinectFrame, data_size, i);

		//if frame is OK, write it
		if (res) {
			//need to start the segment (this will generate the init segment on the first pass)
			if (!muxer->segment_started) {
				muxer_open_segment(muxer, "output", "seg", seg_num);
				timeref = gf_sys_clock_high_res() - sys_start;
				//printf("\t\t\t\tOpening segment time : %llu\n", timeref);
			}

			res = muxer_write_frame(muxer, i);
			//function returns 1 if segment should be closed (duration exceeded) done with segment, close it
			if (res == 1) {
				muxer_close_segment(muxer);

				// Write playlist
				tmp = vidListStream.str();
				vidListStream.seekp(0);
				vidListStream << "\n\t[\n\t\t{ \n\t\t\"Open_segment_time\":" << "\"" << timeref << "\",";
				vidListStream << "\n\t\t\"Video_Segment\": seg_" << seg_num << "_gpac.m4s,\n\t\t\"Skel_Segments\":";
				vidListStream << "[{" << skelListStream.str() << "}],\n\t\t";
				vidListStream << "\"Slides\": [{" << imListStream.str() << "}]\n";
				vidListStream << "\t\t}\n\t]\n";

				vidListStream << tmp;
				vidPlaylist.open("output\\playlist.txt");
				vidPlaylist << vidListStream.str();
				vidPlaylist.close();

				imListStream.str("");
				skelListStream.str("");

				// if we made a screenshot during the previous segment, report it in the next segment
				if (im_refJSON > 1){
					im_refJSON = 1;
					imListStream << "\"" << im_refJSON << "\":" << "im_" << im_num << ".png\", \"Time_after_segment\": \"" << 0.0 << "\"";
					lastScreenshot = timeScreenshot;
					timeScreenshot = gf_sys_clock_high_res() - sys_start;
					im_refJSON++;
				}
				seg_num++;
			}
		}
	}

	fprintf(stderr, "Codec params : %s\n", muxer->codec6381);
	if (muxer->segment_started) {
		muxer_close_segment(muxer);
	}
	muxer_delete(muxer);
	free(data);
	free(kinectFrame);

	system("PAUSE");
	gf_sys_close();
}
