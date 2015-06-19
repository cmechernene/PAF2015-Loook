#include "Encoder.h"

/**
* A function which takes FFmpeg H264 extradata (SPS/PPS) and bring them ready to be pushed to the MP4 muxer.
* @param extradata
* @param extradata_size
* @param dstcfg
* @returns GF_OK is the extradata was parsed and is valid, other values otherwise.
*/
static GF_Err import_avc_extradata(const u8 *extradata, const u64 extradata_size, GF_AVCConfig *dstcfg)
{
	u8 nal_size;
	AVCState avc;
	GF_BitStream *bs;
	if (!extradata || (extradata_size < sizeof(u32)))
		return GF_BAD_PARAM;
	bs = gf_bs_new((const char *)extradata, extradata_size, GF_BITSTREAM_READ);
	if (!bs)
		return GF_BAD_PARAM;
	if (gf_bs_read_u32(bs) != 0x00000001) {
		gf_bs_del(bs);
		return GF_BAD_PARAM;
	}

	//SPS
	{
		s32 idx;
		char *buffer = NULL;
		const u64 nal_start = 4;
		nal_size = gf_media_nalu_next_start_code_bs(bs);
		if (nal_start + nal_size > extradata_size) {
			gf_bs_del(bs);
			return GF_BAD_PARAM;
		}
		buffer = (char*)gf_malloc(nal_size);
		gf_bs_read_data(bs, buffer, nal_size);
		gf_bs_seek(bs, nal_start);
		if ((gf_bs_read_u8(bs) & 0x1F) != GF_AVC_NALU_SEQ_PARAM) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		idx = gf_media_avc_read_sps(buffer, nal_size, &avc, 0, NULL);
		if (idx < 0) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		dstcfg->configurationVersion = 1;
		dstcfg->profile_compatibility = avc.sps[idx].prof_compat;
		dstcfg->AVCProfileIndication = avc.sps[idx].profile_idc;
		dstcfg->AVCLevelIndication = avc.sps[idx].level_idc;
		dstcfg->chroma_format = avc.sps[idx].chroma_format;
		dstcfg->luma_bit_depth = 8 + avc.sps[idx].luma_bit_depth_m8;
		dstcfg->chroma_bit_depth = 8 + avc.sps[idx].chroma_bit_depth_m8;

		{
			GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			slc->size = nal_size;
			slc->id = idx;
			slc->data = buffer;
			gf_list_add(dstcfg->sequenceParameterSets, slc);
		}
	}

	//PPS
	{
		s32 idx;
		char *buffer = NULL;
		const u64 nal_start = 4 + nal_size + 4;
		gf_bs_seek(bs, nal_start);
		nal_size = gf_media_nalu_next_start_code_bs(bs);
		if (nal_start + nal_size > extradata_size) {
			gf_bs_del(bs);
			return GF_BAD_PARAM;
		}
		buffer = (char*)gf_malloc(nal_size);
		gf_bs_read_data(bs, buffer, nal_size);
		gf_bs_seek(bs, nal_start);
		if ((gf_bs_read_u8(bs) & 0x1F) != GF_AVC_NALU_PIC_PARAM) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		idx = gf_media_avc_read_pps(buffer, nal_size, &avc);
		if (idx < 0) {
			gf_bs_del(bs);
			gf_free(buffer);
			return GF_BAD_PARAM;
		}

		{
			GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			slc->size = nal_size;
			slc->id = idx;
			slc->data = buffer;
			gf_list_add(dstcfg->pictureParameterSets, slc);
		}
	}

	gf_bs_del(bs);
	return GF_OK;
}

static GF_Err muxer_write_config(DASHOutputFile *dasher, u32 *di, u32 track)
{
	GF_Err ret;
	if (dasher->codec_ctx->codec_id == CODEC_ID_H264) {
		GF_AVCConfig *avccfg;
		avccfg = gf_odf_avc_cfg_new();
		if (!avccfg) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot create AVCConfig\n"));
			return GF_OUT_OF_MEM;
		}

		ret = import_avc_extradata(dasher->codec_ctx->extradata, dasher->codec_ctx->extradata_size, avccfg);
		if (ret != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot parse AVC/H264 SPS/PPS\n"));
			gf_odf_avc_cfg_del(avccfg);
			return ret;
		}

		ret = gf_isom_avc_config_new(dasher->isof, track, avccfg, NULL, NULL, di);
		if (ret != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_avc_config_new\n", gf_error_to_string(ret)));
			return ret;
		}

		gf_odf_avc_cfg_del(avccfg);

	}
	return GF_OK;
}

int muxer_create_init_segment(DASHOutputFile *dasher, char *filename)
{
	GF_Err ret;
	AVCodecContext *video_codec_ctx = dasher->codec_ctx;
	u32 di, track;

	//TODO: For the moment it is fixed
	//u32 sample_dur = dasher->codec_ctx->time_base.den;

	//int64_t profile = 0;
	//av_opt_get_int(dasher->codec_ctx->priv_data, "level", AV_OPT_SEARCH_CHILDREN, &profile);

	dasher->isof = gf_isom_open(filename, GF_ISOM_OPEN_WRITE, NULL);
	if (!dasher->isof) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open iso file %s\n", filename));
		return -1;
	}
	//gf_isom_store_movie_config(dasher->isof, 0);
	track = gf_isom_new_track(dasher->isof, 0, GF_ISOM_MEDIA_VISUAL, video_codec_ctx->time_base.den);
	dasher->trackID = gf_isom_get_track_id(dasher->isof, track);

	dasher->timescale = video_codec_ctx->time_base.den;
	if (!dasher->frame_dur)
		dasher->frame_dur = video_codec_ctx->time_base.num;

	if (!track) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot create new track\n"));
		return -1;
	}

	ret = gf_isom_set_track_enabled(dasher->isof, track, 1);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_set_track_enabled\n", gf_error_to_string(ret)));
		return -1;
	}

	ret = muxer_write_config(dasher, &di, track);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: muxer_write_config\n", gf_error_to_string(ret)));
		return -1;
	}

	gf_isom_set_visual_info(dasher->isof, track, di, video_codec_ctx->width, video_codec_ctx->height);
	gf_isom_set_sync_table(dasher->isof, track);

	ret = gf_isom_setup_track_fragment(dasher->isof, track, 1, (u32)dasher->frame_dur, 0, 0, 0, 0);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_setup_track_fragment\n", gf_error_to_string(ret)));
		return -1;
	}

	ret = gf_isom_finalize_for_fragment(dasher->isof, track);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_finalize_for_fragment\n", gf_error_to_string(ret)));
		return -1;
	}

	ret = gf_media_get_rfc_6381_codec_name(dasher->isof, track, dasher->codec6381, GF_FALSE, GF_FALSE);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_finalize_for_fragment\n", gf_error_to_string(ret)));
		return -1;
	}

	return 0;
}

int muxer_write_video_frame(DASHOutputFile *dasher)
{
	GF_Err ret;
	AVCodecContext *video_codec_ctx = dasher->codec_ctx;

	u32 sc_size = 0;
	u32 nalu_size = 0;

	u32 buf_len = dasher->encoded_frame_size;
	u8 *buf_ptr = dasher->vbuf;

	GF_BitStream *out_bs = gf_bs_new(NULL, 2 * buf_len, GF_BITSTREAM_WRITE);
	nalu_size = gf_media_nalu_next_start_code(buf_ptr, buf_len, &sc_size);
	if (nalu_size != 0) {
		gf_bs_write_u32(out_bs, nalu_size);
		gf_bs_write_data(out_bs, (const char*)buf_ptr, nalu_size);
	}
	if (sc_size) {
		buf_ptr += (nalu_size + sc_size);
		buf_len -= (nalu_size + sc_size);
	}

	while (buf_len) {
		nalu_size = gf_media_nalu_next_start_code(buf_ptr, buf_len, &sc_size);
		if (nalu_size != 0) {
			gf_bs_write_u32(out_bs, nalu_size);
			gf_bs_write_data(out_bs, (const char*)buf_ptr, nalu_size);
		}

		buf_ptr += nalu_size;

		if (!sc_size || (buf_len < nalu_size + sc_size))
			break;
		buf_len -= nalu_size + sc_size;
		buf_ptr += sc_size;
	}

	gf_bs_get_content(out_bs, &dasher->sample->data, &dasher->sample->dataLength);
	//dasher->sample->data = //(char *) (dasher->vbuf + nalu_size + sc_size);
	//dasher->sample->dataLength = //dasher->encoded_frame_size - (sc_size + nalu_size);

	dasher->sample->DTS = video_codec_ctx->coded_frame->pkt_dts;
	dasher->sample->CTS_Offset = (s32)(video_codec_ctx->coded_frame->pts - dasher->sample->DTS);
	dasher->sample->IsRAP = video_codec_ctx->coded_frame->key_frame ? RAP : RAP_NO;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("Isom Write: RAP %d , DTS "LLD" CTS offset %d \n", dasher->sample->IsRAP, dasher->sample->DTS, dasher->sample->CTS_Offset));

	ret = gf_isom_fragment_add_sample(dasher->isof, dasher->trackID, dasher->sample, 1, (u32)dasher->frame_dur, 0, 0, (Bool)0);
	if (ret != GF_OK) {
		gf_bs_del(out_bs);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_fragment_add_sample\n", gf_error_to_string(ret)));
		return -1;
	}

	//free data but keep sample structure alive
	gf_free(dasher->sample->data);
	dasher->sample->data = NULL;
	dasher->sample->dataLength = 0;

	gf_bs_del(out_bs);
	return 0;
}


GF_Err muxer_open_segment(DASHOutputFile *dasher, char *directory, char *id_name, int seg)
{
	GF_Err ret;
	char name[GF_MAX_PATH];

	if (seg == 1) {
		snprintf(name, sizeof(name), "%s/%s_init_gpac.mp4", directory, id_name);
		muxer_create_init_segment(dasher, name);
		dasher->first_dts_in_fragment = 0;
	}
	snprintf(name, sizeof(name), "%s/%s_%d_gpac.m4s", directory, id_name, seg);

	ret = gf_isom_start_segment(dasher->isof, name, (Bool)1);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_start_segment\n", gf_error_to_string(ret)));
		return ret;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DashCast] Opening new segment %s at UTC "LLU" ms\n", name, gf_net_get_utc()));
	return GF_OK;
}

int muxer_write_frame(DASHOutputFile *dasher, int frame_nb)
{
	GF_Err ret;

	if (!dasher->fragment_started) {
		dasher->fragment_started = GF_TRUE;
		ret = gf_isom_start_fragment(dasher->isof, (Bool)1);
		if (ret < 0)
			return -1;

		dasher->first_dts_in_fragment = dasher->codec_ctx->coded_frame->pkt_dts;
		if (!dasher->segment_started) {
			dasher->pts_at_segment_start = dasher->codec_ctx->coded_frame->pts;
			dasher->segment_started = GF_TRUE;
			if (!dasher->nb_segments) {
				dasher->pts_at_first_segment = dasher->pts_at_segment_start;
			}
		}
		gf_isom_set_traf_base_media_decode_time(dasher->isof, dasher->trackID, dasher->first_dts_in_fragment);
	}

	if (muxer_write_video_frame(dasher) < 0) {
		return -1;
	}
	dasher->last_pts = dasher->codec_ctx->coded_frame->pts;
	dasher->last_dts = dasher->codec_ctx->coded_frame->pkt_dts;

	//we may have rounding errors on the input PTS :( add half frame dur safety

	//flush segments based on the cumultated duration , to avoid drift
	if (1000 * (dasher->last_pts - dasher->pts_at_first_segment + 3 * dasher->frame_dur / 2) / dasher->timescale >= (dasher->nb_segments + 1)*dasher->seg_dur) {
		return 1;
	}
	return 0;
}

int muxer_close_segment(DASHOutputFile *dasher)
{
	GF_Err ret;
	dasher->fragment_started = dasher->segment_started = GF_FALSE;
	dasher->nb_segments++;

	ret = gf_isom_close_segment(dasher->isof, 0, 0, 0, 0, 0, GF_FALSE, GF_TRUE, 0, NULL, NULL);
	if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_close_segment\n", gf_error_to_string(ret)));
		return -1;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DashCast] Rep %s Closing segment at UTC "LLU" ms\n", dasher->rep_id, gf_net_get_utc()));

	return 0;
}



/**
* A function which pushes argument to a libav codec using its private data.
* param priv_data
* param options a list of space separated and ':' affected options (e.g. "a:b c:d e:f"). @options be non NULL.
*/
static void build_dict(void *priv_data, const char *options) {
	char *opt = gf_strdup(options);
	char *tok = strtok(opt, "=");
	char *tokval = NULL;
	while (tok && (tokval = strtok(NULL, " "))) {
		if (av_opt_set(priv_data, tok, tokval, 0) < 0)
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Unknown custom option \"%s\" with value \"%s\" in %s\n", tok, tokval, options));
		tok = strtok(NULL, "=");
	}
	gf_free(opt);
}

DASHOutputFile *muxer_init(u32 seg_dur_in_ms, u32 frame_dur, u32 timescale, u32 gop_size, u32 width, u32 height, u32 bitrate, Bool input_rgb)
{
	DASHOutputFile *dasher;
	GF_SAFEALLOC(dasher, DASHOutputFile);

	dasher->sample = gf_isom_sample_new();
	dasher->isof = NULL;

	dasher->seg_dur = seg_dur_in_ms;

	dasher->gop_size = gop_size;
	dasher->frame_dur = frame_dur;

	dasher->avframe = av_frame_alloc();

	//alloc a FFMPEG buffer to store encoded data
	dasher->vbuf_size = 9 * width * height + 10000;
	dasher->vbuf = (uint8_t *)av_malloc(dasher->vbuf_size);

	av_register_all();
	avcodec_register_all();

	dasher->codec = avcodec_find_encoder_by_name("libx264");
	if (dasher->codec == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Output video codec libx264 not found\n"));
		av_free(dasher->vbuf);
		av_free(dasher->avframe);
		gf_isom_sample_del(&dasher->sample);
		gf_free(dasher);
		return NULL;
	}

	dasher->codec_ctx = avcodec_alloc_context3(dasher->codec);

	dasher->codec_ctx->codec_id = dasher->codec->id;
	dasher->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	dasher->codec_ctx->bit_rate = bitrate;
	dasher->codec_ctx->width = width;
	dasher->codec_ctx->height = height;
	dasher->codec_ctx->sample_aspect_ratio.num = dasher->codec_ctx->sample_aspect_ratio.den = 1;


	//PAF_TODO: mettre les bonnes valeurs ici 
	dasher->codec_ctx->time_base.num = frame_dur; 
	dasher->codec_ctx->time_base.den = timescale; 

	dasher->codec_ctx->pix_fmt = PIX_FMT_YUV420P;
	dasher->codec_ctx->gop_size = 30;

	av_opt_set(dasher->codec_ctx->priv_data, "vprofile", "baseline", 0);
	av_opt_set(dasher->codec_ctx->priv_data, "preset", "ultrafast", 0);
	av_opt_set(dasher->codec_ctx->priv_data, "tune", "zerolatency", 0);
	av_opt_set(dasher->codec_ctx->priv_data, "x264opts", "no-mbtree:sliced-threads:sync-lookahead=0", 0);

	//the global header gives access to the extradata (SPS/PPS) in ffmpeg extra data
	dasher->codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	/* open the video codec - options are passed thru dasher->codec_ctx->priv_data */
	if (avcodec_open2(dasher->codec_ctx, dasher->codec, NULL) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open output video codec\n"));
		av_free(dasher->codec_ctx);
		av_free(dasher->vbuf);
		av_free(dasher->avframe);
		gf_isom_sample_del(&dasher->sample);
		gf_free(dasher);
		return NULL;
	}

	if (input_rgb) {
		dasher->rgb_yuv_ctx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
			width, height, AV_PIX_FMT_YUV420P,
			0, 0, 0, 0);

		dasher->yuv_buffer = (u8 *)av_malloc(width*height * 3 / 2);
	}

	return dasher;
}

int muxer_encode(DASHOutputFile *dasher, u8 *frame, u32 frame_size, u64 PTS)
{
	// AVPacket stores compressed data. Contain one compressed frame
	AVPacket pkt;
	int got_packet;

	av_init_packet(&pkt); // initialize optional field of a packet with default values
	pkt.data = dasher->vbuf;
	pkt.size = dasher->vbuf_size;
	pkt.pts = pkt.dts = dasher->avframe->pts = dasher->avframe->pkt_dts = dasher->avframe->pkt_pts = PTS;


	// AVFrame : decoded/raw video data
	dasher->avframe->width = dasher->codec_ctx->width;
	dasher->avframe->height = dasher->codec_ctx->height;

	if (dasher->rgb_yuv_ctx) {
		uint8_t * inData[1] = { frame }; // RGB24 have one plane
		int inLinesize[1] = { 3 * dasher->codec_ctx->width }; // RGB stride

		uint8_t * outData[3];
		int outLinesize[3];

		outLinesize[0] = dasher->codec_ctx->width;
		outLinesize[1] = dasher->codec_ctx->width / 2;
		outLinesize[2] = dasher->codec_ctx->width / 2;
		outData[0] = dasher->yuv_buffer;
		outData[1] = dasher->yuv_buffer + dasher->codec_ctx->width*dasher->codec_ctx->height;
		outData[2] = dasher->yuv_buffer + 5 * dasher->codec_ctx->width*dasher->codec_ctx->height / 4;

		sws_scale(dasher->rgb_yuv_ctx, inData, inLinesize, 0, dasher->codec_ctx->height, outData, outLinesize);
		frame = dasher->yuv_buffer;
	}

	// Size in bytes for each picture line
	dasher->avframe->linesize[0] = dasher->codec_ctx->width;
	dasher->avframe->linesize[1] = dasher->codec_ctx->width / 2;
	dasher->avframe->linesize[2] = dasher->codec_ctx->width / 2;

	// frame intervient ici
	dasher->avframe->format = PIX_FMT_YUV420P;
	dasher->avframe->data[0] = frame;
	dasher->avframe->data[1] = frame + dasher->codec_ctx->width * dasher->codec_ctx->height;
	dasher->avframe->data[2] = frame + 5 * dasher->codec_ctx->width * dasher->codec_ctx->height / 4;


	dasher->avframe->pict_type = AV_PICTURE_TYPE_NONE;

	//if segment is not started, force a new frame
	if (!dasher->segment_started)
		dasher->avframe->pict_type = AV_PICTURE_TYPE_I; // frame intra

	// encodage? Takes raw video data from frame and writes the next output packet
	// Param 1 : codec context
	// Param 2 : output AVPacket
	// Param 3 : [in] AVFrame containing raw video data to be encoded
	// Param 4 : [out] 1 if output packet non-empty and 0 if empty

	//Return 0 on success, negative error code on failure
	dasher->encoded_frame_size = avcodec_encode_video2(dasher->codec_ctx, &pkt, dasher->avframe, &got_packet);


	if (dasher->encoded_frame_size >= 0)
		dasher->encoded_frame_size = pkt.size;

	if (dasher->encoded_frame_size >= 0) {
		if (got_packet) {
			dasher->codec_ctx->coded_frame->pts = dasher->codec_ctx->coded_frame->pkt_pts = pkt.pts;
			dasher->codec_ctx->coded_frame->pkt_dts = pkt.dts;
			dasher->codec_ctx->coded_frame->key_frame = (pkt.flags & AV_PKT_FLAG_KEY) ? 1 : 0;
		}
	}
	if (dasher->encoded_frame_size < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error occured while encoding video frame.\n"));
		return -1;
	}

	return dasher->encoded_frame_size;
}

void muxer_delete(DASHOutputFile *dasher)
{
	av_free(dasher->vbuf);
	avcodec_close(dasher->codec_ctx);
	av_free(dasher->codec_ctx);
	av_free(dasher->avframe);

	if (dasher->isof != NULL) {
		gf_isom_close(dasher->isof);
	}
	gf_isom_sample_del(&dasher->sample);
	gf_free(dasher);
}
