#include "Kinect.h"
#include "Encoder.h"
#include "ScreenCapture.h"
#include <sstream>
#include <windows.h>

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
	u64 sys_start;
	u64 now;

	std::ostringstream destStream;
	std::string destName;

	std::ostringstream vidListStream;
	std::ofstream vidPlaylist;
	std::string tmp;

	std::ostringstream imListStream;
	std::ofstream imPlaylist;

	vidListStream << "seg_init_gpac.mp4\n";
	
	u8 *data = (u8 *)malloc(data_size);

	//make a white frame
	//memset(data, 0xFF, data_size);

	Kinect kinect;

	// Color data buffer
	unsigned char * kinectFrame = (unsigned char *)malloc(data_size);

	gf_sys_init(GF_FALSE);
	//Sleep(500);

	// Init DASHOutputFile : frame_per_segment, frame_dur, alloc avframe, alloc buffer, codec context, ...
	// OLD muxer = muxer_init(seg_dur_in_ms, 33333, 1000000, 30, width, height, bitrate, GF_TRUE);

	muxer = muxer_init(seg_dur_in_ms, 1, 1000000, 30, width, height, bitrate, GF_TRUE);

	if (!muxer) {
		fprintf(stderr, "Error initializing muxer\n");
		return 1;
	}
	//try to generate 4 seconds (eg 4 segments)
	nb_test_frames = 30 * 3;

	sys_start = gf_sys_clock_high_res();

	for (i = 0; i<nb_test_frames; i++) {
		u64 pts;

		// Update kinectFrameData 
		HRESULT hr = kinect.update(&kinectFrame, &now, i);
		pts = gf_sys_clock_high_res() - sys_start;

		// OLD int res = muxer_encode(muxer, kinectFrame, data_size, pts);
		int res = muxer_encode(muxer, kinectFrame, data_size, i);

		//if frame is OK, write it
		if (res) {
			//need to start the segment (this will generate the init segment on the first pass)
			if (!muxer->segment_started) {
				muxer_open_segment(muxer, "output", "seg", seg_num);
			}

			res = muxer_write_frame(muxer, i);
			//function returns 1 if segment should be closed (duration exceeded) done with segment, close it
			if (res == 1) {
				muxer_close_segment(muxer);
				destStream.str("");
				destStream << "C:\\Users\\Martin\\ColorBasics-D2D\\output\\screen\\im_" << seg_num << ".png";
				destName = destStream.str();
				ScreenCapture(0, 0, 1920, 1080, (char *)destName.c_str());

				// Writing playlists
				tmp = imListStream.str();
				imListStream.str("");
				imListStream << "im_" << seg_num << ".png\n";
				imListStream << tmp;
				imPlaylist.open("output\\imPlaylist.txt");
				imPlaylist << imListStream.str();
				imPlaylist.close();

				tmp = vidListStream.str();
				vidListStream.seekp(0);
				vidListStream << "seg_" << seg_num << "_gpac.m4s\n";
				vidListStream << tmp;
				vidPlaylist.open("output\\playlist.txt");
				vidPlaylist << vidListStream.str();
				vidPlaylist.close();

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
