#include "Kinect.h"
#include "Encoder.h"
/*
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

	float time = 0.0f;

	u8 *data = (u8 *)malloc(data_size);
	//make a white frame
	//memset(data, 0xFF, data_size);

	Kinect kinect;

	// Color data buffer
	unsigned char * kinectFrame = (unsigned char *)malloc(data_size);

	// Init DASHOutputFile : frame_per_segment, frame_dur, alloc avframe, alloc buffer, codec context, ...
	muxer = muxer_init(seg_dur_in_ms, 33333, 1000000, 30, width, height, bitrate, GF_TRUE);
	if (!muxer) {
		fprintf(stderr, "Error initializing muxer\n");
		return 1;
	}
	//try to generate 4 seconds (eg 4 segments)
	nb_test_frames = 30 * 4;

	for (i = 0; i<nb_test_frames; i++) {
		//u64 now = gf_sys_clock_high_res();

		//PTS is the frame number in this example

		if (i == 0){
			time = 0.0f;
		}
		else{
			time = clock()/CLOCKS_PER_SEC;
		}

		// Update kinectFrameData 
		kinect.update(&kinectFrame);

		int res = muxer_encode(muxer, kinectFrame, data_size, i); // i <-> time?

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
		//time = clock() - time;
		printf("%f seconds\n", (float)time / CLOCKS_PER_SEC);
	}

	if (muxer->segment_started) {
		muxer_close_segment(muxer);
	}
	muxer_delete(muxer);
	free(data);
	free(kinectFrame);

	system("PAUSE");
}
*/