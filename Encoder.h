#define _CRT_SECURE_NO_WARNINGS // erreur sur snprintf

#if defined(WIN32) && !defined(__MINGW32__)

#define EMULATE_INTTYPES
#define EMULATE_FAST_INT
#ifndef inline
#define inline __inline
#endif

#if defined(__SYMBIAN32__)
#define EMULATE_INTTYPES
#endif


#ifndef __MINGW32__
#define __attribute__(s)
#endif

#endif


#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)

#define _TOSTR(_val) #_val
#define TOSTR(_val) _TOSTR(_val)

#endif


#if defined(WIN32)
#  define INT64_C(x)  (x ## i64)
#  define UINT64_C(x)  (x ## Ui64)
#endif


extern "C" {

#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>


#include <gpac/isomedia.h>
#include <gpac/internal/media_dev.h>
#include <gpac/network.h>
#include <gpac/constants.h>
#include <gpac/setup.h>

}


typedef struct {
	AVCodecContext *codec_ctx;
	AVCodec *codec;
	struct SwsContext *rgb_yuv_ctx;
	u8 *yuv_buffer;

	GF_ISOFile *isof;
	GF_ISOSample *sample;

	u32 trackID;

	/* Variables that encoder needs to encode data */
	uint8_t *vbuf;
	int vbuf_size;
	int encoded_frame_size;

	int frame_per_segment;

	int seg_dur;

	u64 first_dts_in_fragment;

	int gop_size;

	u64 pts_at_segment_start, pts_at_first_segment;
	u64 last_pts, last_dts;
	u64 frame_dur;
	u32 timescale;
	u32 nb_segments;

	Bool fragment_started, segment_started;
	const char *rep_id;

	/* RFC6381 codec name, only valid when VIDEO_MUXER == GPAC_INIT_VIDEO_MUXER_AVC1 */
	char codec6381[GF_MAX_PATH];

	AVFrame *avframe; // raw, decoded video data
} DASHOutputFile;

/* Init muxer */
DASHOutputFile *muxer_init(u32 frame_per_segment, u32 seg_dur_in_ms, u32 frame_dur, u32 gop_size, u32 width, u32 height, u32 bitrate, Bool input_rgb);

/* Encode frame */
int muxer_encode(DASHOutputFile *dasher, u8 *frame, u32 frame_size, u64 PTS);

/* Open new segment */
GF_Err muxer_open_segment(DASHOutputFile *dasher, char *directory, char *id_name, int seg);

/* Write frame in current segment */
int muxer_write_frame(DASHOutputFile *dasher, int frame_nb);

/* Close segment */
int muxer_close_segment(DASHOutputFile *dasher);

void muxer_delete(DASHOutputFile *dasher);


static GF_Err import_avc_extradata(const u8 *extradata, const u64 extradata_size, GF_AVCConfig *dstcfg);
static GF_Err muxer_write_config(DASHOutputFile *dasher, u32 *di, u32 track);

int muxer_create_init_segment(DASHOutputFile *dasher, char *filename);
int muxer_write_video_frame(DASHOutputFile *dasher);

static void build_dict(void *priv_data, const char *options);