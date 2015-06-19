#include "Kinect.h"
#include "Encoder.h"
#include "ScreenCapture.h"

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

	char * fileDest; // Hold the name of the screenshot
	
	u8 *data = (u8 *)malloc(data_size);

	//make a white frame
	//memset(data, 0xFF, data_size);

	Kinect kinect;

	// Color data buffer
	unsigned char * kinectFrame = (unsigned char *)malloc(data_size);

	gf_sys_init(GF_FALSE);

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
		kinect.update(&kinectFrame, &now, i);
		pts = gf_sys_clock_high_res() - sys_start;

		// OLD int res = muxer_encode(muxer, kinectFrame, data_size, pts);
		int res = muxer_encode(muxer, kinectFrame, data_size, i);
		
		// fileDest = "C:\\Users\\Martin\\ColorBasics-D2D\\output\\screen\\i.png";
		//sprintf(fileDest, "C:\\Users\\Martin\\ColorBasics-D2D\\output\\screen\\i%d.png", i);
		//ScreenCapture(0, 0, 1920, 1080, fileDest);

		//if frame is OK, write it
		if (res) {
			//need to start the segment (this will generate the init segment on the first pass)
			if (!muxer->segment_started) {
				muxer_open_segment(muxer, "output", "segJ", seg_num);
			}

			res = muxer_write_frame(muxer, i);
			//function returns 1 if segment should be closed (duration exceeded) done with segment, close it
			if (res == 1) {
				muxer_close_segment(muxer);
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
