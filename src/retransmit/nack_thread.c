#include<pjmedia/clock.h>
#include<pjmedia/vid_stream.h>
#include<jitter_buffer/jitter_buffer_interface.h>
#include <retransmit/nack_thread.h>
#include <retransmit/nack.h>
#include<pj/assert.h>
#include<pj/os.h>
#include<pj/log.h>
#include<pj/compat/string.h>

#define THIS_FILE "nack_thread.c"
#if defined(RTP_RETRANSMIT) && (RTP_RETRANSMIT!=0) && defined(NEW_JITTER_BUFFER) && (NEW_JITTER_BUFFER != 0)
#define NACK_INTERVAL 10
#define MAX_NACK_SIZE 253
void printNackList(pj_uint16_t *nack_list, pj_uint32_t nack_size) {
    PJ_LOG(4, (THIS_FILE, "nack size:%u", nack_size));
    char buf[256];
    int i ;
    int len , rest = 255;
    char * ptr = buf;
    for(i = 0; i < nack_size; ++i) {
        len = snprintf(ptr, rest, "%u ", nack_list[i]);
        if(len <= 0)
            break;
        ptr += len;
        rest -= len;
    }
    *ptr = '\0';
    PJ_LOG(4, (THIS_FILE, "%s\n", buf));
}
static void nack_proc(const pj_timestamp *ts, void *user_data) {
    	pjmedia_vid_stream *stream = (pjmedia_vid_stream*) user_data;
        pj_uint32_t nack_list_size ;
    	pj_uint16_t *nack_list;
    	unsigned avg_rtt;
    	pj_bool_t allow_nack;
    	pj_timestamp now;
		static pj_timestamp lastSendTime;
        static pj_uint16_t lastSeqNum = 0;
        static pj_bool_t first_proc = PJ_TRUE;
        allow_nack = PJ_FALSE; /** no nack request */
        nack_list_size = 0;
        /**get nack list */
       int ret = jitter_buffer_interface_createNackList(stream->jbi, stream->rtcp.stat.rtt.last / 1000, 
                            &nack_list, &nack_list_size);
        if(nack_list_size != 0) {
            //printNackList(nack_list, nack_list_size);
            allow_nack = PJ_TRUE;
            if(nack_list_size == 0xffff)
                nack_list_size = 0;
            else {
            nack_list_size = nack_list_size > MAX_NACK_SIZE ? MAX_NACK_SIZE : nack_list_size;
            }
            
        }

		
        /** check if we should send nack request now */
        if(allow_nack) {
        avg_rtt = stream->rtcp.stat.rtt.mean / 1000; /* rtt in usec */
        avg_rtt = 5 + ((avg_rtt * 3) >> 1); /** 5 + 1.5 * RTT */
        if(avg_rtt == 5) {
            avg_rtt = 100; /**During startup we don't have an RTT */
        }
        pj_get_timestamp(&now);
        if(!first_proc && pj_elapsed_msec(&lastSendTime, &now) < avg_rtt) {
            /** too quickly */
        } else {
            if(nack_list_size >= 1 && 
            lastSeqNum == nack_list[nack_list_size - 1]) {
                /** no new seq */
            } else {
                
                if(nack_list_size >= 1)
                    lastSeqNum = nack_list[nack_list_size - 1];
				pjmedia_send_nack(stream->transport, 0, 0, nack_list_size,
                          nack_list);
                /**update send time */
                pj_get_timestamp(&lastSendTime);
            }
        }

        }
        if(first_proc) {
            first_proc = PJ_FALSE;
            pj_get_timestamp(&lastSendTime); 
        } 
}

PJ_DEF(pj_status_t) pjmedia_nack_thread_create(pjmedia_vid_stream *stream, 
						pjmedia_clock **clock) {
	pjmedia_clock_param param;
	pj_status_t status;

        param.usec_interval = NACK_INTERVAL * 1000;
        param.clock_rate = 90000; /** timestamp not used */
        status = pjmedia_clock_create2(stream->own_pool, &param,
                                       PJMEDIA_CLOCK_NO_HIGHEST_PRIO,
					                    nack_proc,
                                       stream, clock);
	return status;
}


PJ_DEF(pj_status_t) pjmedia_nack_thread_start(pjmedia_clock *clock) {
	pj_assert(clock != NULL);
	return pjmedia_clock_start(clock);
}

PJ_DEF(pj_status_t) pjmedia_nack_thread_destroy(pjmedia_clock *clock) {
	if(clock) {
		pjmedia_clock_destroy(clock);
		clock = NULL;
	}
    return PJ_SUCCESS;
}
#endif
