#ifndef __NACK_THREAD_H__
#define __NACK_THREAD_H__
#include<pjmedia/vid_stream.h>
#include<pjmedia/clock.h>
#include<pj/types.h>
PJ_BEGIN_DECL
/**
  * create nack thread
  */
PJ_DECL(pj_status_t) pjmedia_nack_thread_create(pjmedia_vid_stream *stream,
                        pjmedia_clock **clock);
/**
  * start nack thread.
  * @stream video stream, jitter buffer in the stream will be operated in this thread
  * @clock  thread clock
  */
PJ_DECL(pj_status_t) pjmedia_nack_thread_start(pjmedia_clock *clock);
/**
  * destroy nack thread
  */
PJ_DECL(pj_status_t) pjmedia_nack_thread_destroy(pjmedia_clock *clock);
PJ_END_DECL

#endif
