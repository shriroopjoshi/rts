/* based_task.c -- A basic real-time task skeleton. 
 *
 * This (by itself useless) task demos how to setup a 
 * single-threaded LITMUS^RT real-time task.
 */

/* First, we include standard headers.
 * Generally speaking, a LITMUS^RT real-time task can perform any
 * system call, etc., but no real-time guarantees can be made if a
 * system call blocks. To be on the safe side, only use I/O for debugging
 * purposes and from non-real-time sections.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

/* Second, we include the LITMUS^RT user space library header.
 * This header, part of liblitmus, provides the user space API of
 * LITMUS^RT.
 */
#include "litmus.h"

/* Next, we define period and execution cost to be constant. 
 * These are only constants for convenience in this example, they can be
 * determined at run time, e.g., from command line parameters.
 *
 * These are in milliseconds.
 */
#define PERIOD            100
#define RELATIVE_DEADLINE 100
#define EXEC_COST         10

/* Catch errors.
 */
#define CALL( exp ) do { \
		int ret; \
		ret = exp; \
		if (ret != 0) \
			fprintf(stderr, "%s failed: %m\n", #exp);\
		else \
			fprintf(stderr, "%s ok.\n", #exp); \
	} while (0)


AVFormatContext *pFormatCtx = NULL;
AVCodecContext *pCodecCtx = NULL;
AVCodec *pCodec = NULL;
AVFrame *pFrame = NULL, *pFrameRGB = NULL;
AVPacket packet;
uint8_t *buffer = NULL;
static struct SwsContext *img_convert_ctx = NULL;
int ind = 0, frameFinished;
int vsi;

/* Declare the periodically invoked job. 
 * Returns 1 -> task should exit.
 *         0 -> task should continue.
 */
int job(void);

/* typically, main() does a couple of things: 
 * 	1) parse command line parameters, etc.
 *	2) Setup work environment.
 *	3) Setup real-time parameters.
 *	4) Transition to real-time mode.
 *	5) Invoke periodic or sporadic jobs.
 *	6) Transition to background mode.
 *	7) Clean up and exit.
 *
 * The following main() function provides the basic skeleton of a single-threaded
 * LITMUS^RT real-time task. In a real program, all the return values should be 
 * checked for errors.
 */
int main(int argc, char** argv)
{
	int do_exit;
	struct rt_task param;

	int i;
	int size;

	/* Setup task parameters */
	init_rt_task_param(&param);
	param.exec_cost = ms2ns(EXEC_COST);
	param.period = ms2ns(PERIOD);
	param.relative_deadline = ms2ns(RELATIVE_DEADLINE);

	/* What to do in the case of budget overruns? */
	param.budget_policy = NO_ENFORCEMENT;

	/* The task class parameter is ignored by most plugins. */
	param.cls = RT_CLASS_SOFT;

	/* The priority parameter is only used by fixed-priority plugins. */
	param.priority = LITMUS_LOWEST_PRIORITY;

	/* The task is in background mode upon startup. */


	/*****
	 * 1) Command line paramter parsing would be done here.
	 */
	if(argc < 2) {
		fprintf(stderr, "USAGE: base_task /path/to/video/file");
		return 0;
	}

	/*****
	 * 2) Work environment (e.g., global data structures, file data, etc.) would
	 *    be setup here.
	 */
	av_register_all();
	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
		fprintf(stderr, "Unable to open video file\n");
		return -1;
	}
	if(avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		fprintf(stderr, "Unable to open video stream\n");
		return -1;
	}
	vsi = -1;
	for(i = 0; i < pFormatCtx -> nb_streams; i++) {
		if(pFormatCtx -> stream[i] -> codec -> codec_type == AVMEDIA_TYPE_VIDEO) {
			vsi = i;
			break;
		}
	}
	if(vsi = -1) {
		fprintf(stderr, "No stream found\n");
		return -1;
	}
	pCodecCtx = pFormatCtx -> streams[vsi] -> codec;
	pCodec = avcodec_find_decoder(pCodecCtx -> codec_id);
	if(pCodec == NULL) {
		fprintf(stderr, "Unsupported codec\n");
		return -1;
	}
	if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		return -1;
	}
	pFrame = av_frame_alloc();
	pFrameRGB = av_frame_alloc();
	if(pFrameRGB == NULL) {
		return -1;
	}
	size = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx -> width, pCodecCtx -> height);
	buffer = (uint8_t *) av_malloc(size * sizeof(uint8_t));
	avpicture_fill((AVPicture *) pFrameRGB, buffer, AV_PIX_FMT_RGB24, pCodecCtx -> width, pCodecCtx -> height);
	/*****
	 * 3) Setup real-time parameters. 
	 *    In this example, we create a sporadic task that does not specify a 
	 *    target partition (and thus is intended to run under global scheduling). 
	 *    If this were to execute under a partitioned scheduler, it would be assigned
	 *    to the first partition (since partitioning is performed offline).
	 */
	CALL( init_litmus() );

	/* To specify a partition, do
	 *
	 * param.cpu = CPU;
	 * be_migrate_to(CPU);
	 *
	 * where CPU ranges from 0 to "Number of CPUs" - 1 before calling
	 * set_rt_task_param().
	 */
	CALL( set_rt_task_param(gettid(), &param) );


	/*****
	 * 4) Transition to real-time mode.
	 */
	CALL( task_mode(LITMUS_RT_TASK) );

	/* The task is now executing as a real-time task if the call didn't fail. 
	 */



	/*****
	 * 5) Invoke real-time jobs.
	 */
	do {
		/* Wait until the next job is released. */
		sleep_next_period();
		/* Invoke job. */
		do_exit = job();		
	} while (!do_exit);


	
	/*****
	 * 6) Transition to background mode.
	 */
	CALL( task_mode(BACKGROUND_TASK) );



	/***** 
	 * 7) Clean up, maybe print results and stats, and exit.
	 */
	av_free(buffer);
	av_free(pFrameRGB);
	av_free(pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 0;
}


int job(void) 
{
	/* Do real-time calculation. */
	int i = ind;
	for(; av_read_frame(pFormatCtx, &packet) >= 0 && i < ind + 10; i++) {
		if(packet.stream_index == vsi) {
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			if(frameFinished) {
				i++;
				sws_scale(img_convert_ctx, (const uint8_t * const *) pFrame -> data,
						pFrame -> linesize, 0, pCodecCtx -> height,
						pFrmaeRGB -> data, pFrameRGB -> linesize);
			}
		}
		av_free_packet(&packet);
	}
	index += 10;
	/* Don't exit. */
	return 0;
}
