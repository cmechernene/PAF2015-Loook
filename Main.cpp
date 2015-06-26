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

	u64 now = 0;
	u64 timeref = 0;
	u64 timeScreenshot = 0;

	std::string ipAddr = "137.194.23.204";

	std::ostringstream destStream;

	std::ostringstream vidListStream;
	std::ofstream vidPlaylist;
	std::string tmp;

	std::ostringstream testStream;
	std::ofstream testFile;

	std::ostringstream finalStream;

	std::ostringstream imListStream;
	int im_num = 0;
	int im_refJSON = 1;

	BOOL printPNG = false;

	std::ostringstream skelListStream;

	vidListStream << "],\n\"Video_Segment\": \"seg_init_gpac.mp4\"}\n";
	testStream << "http://" << ipAddr << ":8080/output/seg_init_gpac.mp4\n";

	BOOL resKinect = false;
	INPUT newSlideInput;

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
	nb_test_frames = 30 * 30;

	sys_start = gf_sys_clock_high_res();

	for (i = 0; i<nb_test_frames; i++) {
		u64 pts;

		// Update kinectFrafsincemeData 
		resKinect = kinect.process(&kinectFrame, &now, i);

		// If gesture is detected, switching slides, prepare image playlist
		if (resKinect){
			printf("Changed Slide\n");

			im_num++;

			newSlideInput.type = INPUT_KEYBOARD;
			newSlideInput.ki.wScan = 0;
			newSlideInput.ki.time = 0;
			newSlideInput.ki.dwExtraInfo = 0;

			// PRESS right key
			newSlideInput.ki.wVk = VK_RIGHT; // right arrow key
			newSlideInput.ki.dwFlags = 0; // key press
			SendInput(1, &newSlideInput, sizeof(INPUT));

			// RELEASE right key
			newSlideInput.ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(1, &newSlideInput, sizeof(INPUT));

			// Make sure we really switched slides before capturing screen
			//Sleep(100);
			destStream.str("");
			destStream << "C:\\Users\\Martin\\ColorBasics-D2D\\output\\public\\output\\im_" << im_num << ".png";
			ScreenCapture(0, 0, 1920, 1080, (char *)destStream.str().c_str());

			timeScreenshot = gf_sys_clock_high_res() - sys_start;
			printf("\t\tTIME SCREENSHOT : %llu\n", timeScreenshot);

			
			if (im_refJSON == 1){
				imListStream << "{\"" << im_refJSON << "\":" << "\"im_" << im_num << ".png\", \"Since_open\": \"" << timeScreenshot - timeref << "\"}";
			}
			else{
				imListStream << ", {\"" << im_refJSON << "\":" << "\"im_" << im_num << ".png\", \"Since_open\": \"" << timeScreenshot - timeref << "\"}";
			}
			im_refJSON++;
		}

		if (printPNG){
			testStream << "http://" << ipAddr << ":8080/output/im_" << im_num << ".png\n";
			printPNG = false;
		}

		if ((i % 30 + 1) == 30){
			skelListStream << "{\"" << i % 30 + 1 << "\": \"Coordinates_" << i << ".json\"}\n\t\t\t\t";
		}
		else{
			skelListStream << "{\"" << i % 30 + 1 << "\": \"Coordinates_" << i << ".json\"},\n\t\t\t\t";
		}
		pts = gf_sys_clock_high_res() - sys_start;

		// OLD int res = muxer_encode(muxer, kinectFrame, data_size, pts);
		int res = muxer_encode(muxer, kinectFrame, data_size, i);

		//if frame is OK, write it
		if (res) {
			//need to start the segment (this will generate the init segment on the first pass)
			if (!muxer->segment_started) {
				//muxer_open_segment(muxer, "C:/wamp/www/LOOOK/output", "seg", seg_num);
				muxer_open_segment(muxer, "output/public/output", "seg", seg_num);
				timeref = gf_sys_clock_high_res() - sys_start;
				printf("\t\t\t\tOpening segment time : %llu\n", timeref);
				printPNG = true;
			}

			res = muxer_write_frame(muxer, i);
			//function returns 1 if segment should be closed (duration exceeded) done with segment, close it
			if (res == 1) {
				muxer_close_segment(muxer);

				// Write playlist
				tmp = vidListStream.str();
				vidListStream.seekp(0);
				vidListStream << "\n\t\t{ \n\t\t\"Open_segment_time\":" << "\"" << timeref << "\",";
				vidListStream << "\n\t\t\"Video_Segment\": \"seg_" << seg_num << "_gpac.m4s\",\n\t\t\"Skel_Segments\":";
				vidListStream << "[" << skelListStream.str() << "],\n\t\t";
				vidListStream << "\"Slides\": [" << imListStream.str() << "]\n";
				
				if (seg_num == 1){
					vidListStream << "\t\t}\n";
				}
				else{
					vidListStream << "\t\t},\n";
				}

				vidListStream << tmp;

				finalStream << "{\"Playlist\":\n\t[" << vidListStream.str();
				vidPlaylist.open("output\\playlist.txt");
				vidPlaylist << finalStream.str();
				vidPlaylist.close();

				finalStream.str("");
				imListStream.str("");
				skelListStream.str("");

				
				testStream << "http://" << ipAddr << ":8080/output/seg_" << seg_num << "_gpac.m4s\n";
				testFile.open("output/public/output/playlist.txt");
				if (testFile.is_open()){
					testFile << testStream.str();
				}
				
				testFile.close();

				// if we made a screenshot during the previous segment, report it in the next segment
				if (im_refJSON > 1){
					im_refJSON = 1;
					imListStream << "{\"" << im_refJSON << "\":" << "\"im_" << im_num << ".png\", \"Since_open\": \"" << 0.0 << "\"}";
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
	free(kinectFrame);

	system("PAUSE");
	gf_sys_close();
}
