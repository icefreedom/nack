#include <retransmit/nack.h>
#include <pjmedia/types.h>
#include <pjmedia/vid_stream.h>
#include <pj/types.h>
#include <pjmedia/rtp_utility.h>
#include <pj/log.h>
#include <pjmedia/rtcp.h>
#include <pjmedia-tbuf/send_buf.h>
#include <pj/assert.h>

#define THIS_FILE "nack.c"
#if defined(RTP_RETRANSMIT) && (RTP_RETRANSMIT!=0)
/**
  * if nackSize = 0, nackList =NULL, send an empty nack to 
  * request key frame 
  */
PJ_DEF(pj_status_t) BuildNack(pj_uint8_t *rtcpBuffer,
                            int* pos_origin,
                            const pj_uint32_t nackSize,
                            const pj_uint16_t *nackList,
                            const pj_uint32_t ownSSRC,
                            const pj_uint32_t remoteSSRC) 
{
    pj_uint8_t FMT = 1;
    int pos = *pos_origin;
    int nackSizePos, i, numOfNackFields;
    int pos_tmp;
    //sanity
    if(pos + 16 > PJMEDIA_MAX_MTU ) {
        PJ_LOG(2, (THIS_FILE, "longer than max mtu"));
        return PJ_ENOMEM;
    }
    rtcpBuffer[pos++] = (pj_uint8_t)0x80 + FMT;
    rtcpBuffer[pos++] = (pj_uint8_t)RTCP_NACK; //pt
    
    //nack item size
    rtcpBuffer[pos++] = (pj_uint8_t)0;
    nackSizePos = pos;
    rtcpBuffer[pos++] = (pj_uint8_t)(3); //setting it to one kNACK signal as default
    
    //add our own SSRC
    AssignUWord32ToBuffer(rtcpBuffer + pos, ownSSRC);
    pos += 4;
    
    //add the remote SSRC
    AssignUWord32ToBuffer(rtcpBuffer + pos, remoteSSRC);
    pos += 4;
    
    i = 0;
    numOfNackFields = 0;
    pos_tmp = pos;
    /** if nackSize == 0xffff, request key frame */
    while(nackSize > i && numOfNackFields < 253 && nackList != NULL) {
        pj_uint16_t nack = nackList[i];
        //put down our sequence number
        AssignUWord16ToBuffer(rtcpBuffer + pos, nack);
        pos += 2;
        
        i++;
        numOfNackFields++;
        if(nackSize > i)  {
            pj_bool_t moreThan16Away = (nack + 16 < nackList[i])?PJ_TRUE:PJ_FALSE;
            /*if(!moreThan16Away) {
                //check for a wrap
                if((pj_uint16_t)(nack + 16) > 0xff00 && nackList[i] < 0x0fff) {
                    //wrap
                    moreThan16Away = PJ_TRUE;
                }
            }*/
            if(moreThan16Away) {
                //next is more than 16 away
                rtcpBuffer[pos++] = (pj_uint8_t) 0;
                rtcpBuffer[pos++] = (pj_uint8_t) 0;
            } else {
                //build our bitmask
                pj_uint16_t bitmask = 0;
                
                pj_bool_t within16Away  = (nack + 16 > nackList[i])?PJ_TRUE:PJ_FALSE;
                /*if(within16Away) {
                    //check for a wrap 
                    if((pj_uint16_t)(nack + 16) > 0xff00 && nackList[i] < 0x0fff) {
                        //wrap
                        within16Away = PJ_FALSE;
                    }
                }*/
                
                while(nackSize > i && within16Away) {
                    pj_uint16_t shift = nackList[i] - nack - 1;
                    if(nackList[i] <= nack) {
                        /** this should not happen, but ... */
                        break;
                    }
                    pj_assert(!(shift > 15) && !(shift < 0));
                    
                    bitmask += (1 << shift);
                    i++;
                    if(nackSize > i) {
                        within16Away = (nack + 16 > nackList[i])?PJ_TRUE:PJ_FALSE;
                        /*if(within16Away) {
                            //check for a wrap 
                            if((pj_uint16_t)(nack + 16) > 0xff00 && nackList[i] < 0x0fff) {
                            //wrap
                            within16Away = PJ_FALSE;
                            }
                        }*/
                    }
                }
                AssignUWord16ToBuffer(rtcpBuffer + pos, bitmask);
                pos += 2;
            }
            if (pos + 4 > PJMEDIA_MAX_MTU) {
                PJ_LOG(2, (THIS_FILE, "longer than max mtu"));
                return PJ_ENOMEM;
            }
        } else {
            //no more in the list
            rtcpBuffer[pos++] = (pj_uint8_t) 0;
            rtcpBuffer[pos++] = (pj_uint8_t) 0;
        }
    }
    rtcpBuffer[nackSizePos] = (pj_uint8_t) (2 + numOfNackFields);
    *pos_origin = pos;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) ParseNack(pj_uint8_t* dataBuffer, pj_uint32_t* sendSSRC,
                            pj_uint32_t* mediaSSRC, pj_uint16_t* nackSize,
                            pj_uint16_t* nackList) {
        pj_uint8_t *ptr = dataBuffer + 4;
        int i , j, t;
        pj_uint16_t nack, mask ;
        pjmedia_rtcp_common *common = (pjmedia_rtcp_common*) dataBuffer;
        //nack pt
        pj_assert(common->pt == RTCP_NACK );
        *nackSize = *(dataBuffer + 3)  - 2;
        //send ssrc
        *sendSSRC = *ptr++ << 24;
        *sendSSRC += *ptr++ << 16;
        *sendSSRC += *ptr++ << 8;
        *sendSSRC += *ptr++;
        //media ssrc
        *mediaSSRC = *ptr++ << 24;
        *mediaSSRC += *ptr++ << 16;
        *mediaSSRC += *ptr++ << 8;
        *mediaSSRC += *ptr++;
        j = 0;
        for (i = 0; i < *nackSize; i++ ) {
            nack = *ptr++ << 8;
            nack += *ptr++;
            nackList[j++] = nack;
            mask = *ptr++ << 8;
            mask += *ptr++;
            for(t = 0; t < 16; t++ ) {
                if(mask & (1 << t)) {
                    nackList[j++] = nack + (t + 1);
                }
            }
        }
        *nackSize = j;
        return PJ_SUCCESS;
}

PJ_DEF(void) pjmedia_send_nack(pjmedia_transport *transport, pj_uint32_t ownSSRC,
                                pj_uint32_t remoteSSRC, const pj_uint32_t nackSize,
                                const pj_uint16_t *nackList) {
    pj_uint8_t nackBuffer[PJMEDIA_MAX_MTU];
    int pos = 0 ;
    if(BuildNack(nackBuffer, &pos, nackSize, nackList, ownSSRC, remoteSSRC) != PJ_SUCCESS)
    {
        PJ_LOG(2, (THIS_FILE, "failed to build nack"));
        return;
    }
    if(pjmedia_transport_send_rtcp(transport, nackBuffer, pos) != PJ_SUCCESS) {
        PJ_LOG(2, (THIS_FILE, "failed to send nack"));
        return;
    }
}

PJ_DEF(void) pjmedia_receive_nack(pj_uint8_t* buffer, int len,
                                 pj_uint32_t* sendSSRC,
                            pj_uint32_t* mediaSSRC, pj_uint16_t* nackSize,
                            pj_uint16_t* nackList) {
    //parse nack
    ParseNack(buffer, sendSSRC, mediaSSRC, nackSize, nackList);
}
/**
  * find an old packet, in vid_stream.c, it will be resend,
  * lock internal in case of  this packet modified  by other
  */
static pj_bool_t FindOldPacket(void *tb, pj_uint16_t seq, char* packet, pj_uint32_t *len) {
    int i;
    pj_uint32_t offset;
    tbuf_seq_bucket* bucket_em;
    tbuf_framelist_t *frame_em;
    pjmedia_tbuf *tbuf = (pjmedia_tbuf*) tb;
    pj_mutex_lock(tbuf->tb_mutex);
    /** old packets: insert_index ~ tx_index */
    i = tbuf->insert_index;
    /** this is cost time operate because sequence number maybe not continuous */
    while( i != tbuf->tx_index) {
        offset = i * tbuf->bucket.per_seq_em_size;
        bucket_em = (tbuf_seq_bucket *)((char *)(tbuf->bucket.seq_bucketlist) + offset);
        if(bucket_em->seq_num == seq) {
            /* found */
            offset = i  * tbuf->per_frame_em_size;
            frame_em = (tbuf_framelist_t *)((char *)tbuf->tb_framelist + offset);
            *len = frame_em->content_len;
            if(*len > PJMEDIA_MAX_MTU)
                break;
            memcpy(packet,  (char *)tbuf->tb_framelist + offset + sizeof(tbuf_framelist_t), *len);
            pj_mutex_unlock(tbuf->tb_mutex);
            return PJ_TRUE;
        }
        i =  (i + 1 + tbuf->max_count) % tbuf->max_count;
    }

    pj_mutex_unlock(tbuf->tb_mutex);
    return PJ_FALSE;
}
typedef struct tx_rtp_index {
    pj_uint16_t seq;
    pj_timestamp send_ts;
} tx_rtp_index;
#define MAX_RTP_INDEX_SIZE 256
static pj_timestamp* find_send_ts(pj_uint16_t seq_dst, tx_rtp_index* rtp_index, int start, int end) {
    //binary search
    int mid;
    pj_uint16_t seq;
    if(start == -1)   //no element
        return NULL;
    do {
        if(end < start)
            mid = (end + start + MAX_RTP_INDEX_SIZE) / 2 % MAX_RTP_INDEX_SIZE;
        else
            mid = (end + start) / 2;
        seq = rtp_index[mid].seq;
        if(seq == seq_dst)
            return &rtp_index[mid].send_ts;
        else if(seq > seq_dst) {
            end = mid;
        } else 
            start = (mid + 1) == MAX_RTP_INDEX_SIZE ? 0: (mid + 1);
        
    } while(end != start);
    //not found
    return NULL;
}


static void insert_send_ts(pj_uint16_t seq_dst, pj_timestamp* ts, tx_rtp_index * rtp_index, int* start, int* end) {
    int pos1 = *start, pos2;
    if(pos1 == -1)
        return;
    do {
        if(rtp_index[pos1].seq == 0 || rtp_index[pos1].seq >= seq_dst)
            break;
        pos1 = (pos1 + 1) % MAX_RTP_INDEX_SIZE;
    }while(pos1 != *end);
    if(rtp_index[pos1].seq == seq_dst ) //exists
    {
        pj_memcpy(&rtp_index[pos1].send_ts, ts, sizeof(pj_timestamp));
        return;
    }

    if(rtp_index[pos1].seq == 0 || pos1 == *start) //blank or at the begining
    {
        pj_memcpy(&rtp_index[pos1].send_ts, ts, sizeof(pj_timestamp));
        return;
    }
    // if full, remove the first one
    pos2 = *end;
    if(*end == *start)
        *start = (*start + 1) %MAX_RTP_INDEX_SIZE;
    *end = (*end + 1) % MAX_RTP_INDEX_SIZE;
    while(pos2 != pos1) {
        int pre = (pos2 - 1) >= 0 ? (pos2 - 1) : (MAX_RTP_INDEX_SIZE - 1);
        pj_memcpy(&rtp_index[pos2], &rtp_index[pre], sizeof(tx_rtp_index));
        pos2 = pre;
    }
    rtp_index[pos1].seq = seq_dst;
    pj_memcpy(&rtp_index[pos1].send_ts, ts, sizeof(pj_timestamp));
}
static void ResendPacket(pjmedia_vid_stream *stream, pj_uint16_t seq) {
    pj_uint32_t len = PJMEDIA_MAX_MTU;
    pj_uint8_t data_buffer[PJMEDIA_MAX_MTU];
    pj_uint8_t *buffer_to_send_ptr = data_buffer;
    pj_timestamp now;
    pj_timestamp *send_ts;
    static pj_uint32_t resend_count = 0;
    /**  get packet from packet history */
    int tb_seq_index;
    static int rtp_index_start = -1;
    static int rtp_index_end = 0;
    static tx_rtp_index rtp_index[MAX_RTP_INDEX_SIZE];    
#if defined(PJMEDIA_USE_TBUF) && (PJMEDIA_USE_TBUF!=0) 
    if(stream->tb == NULL)
        return;
    if(!FindOldPacket(stream->tb, seq, (char*)data_buffer, &len)) 
        return;
    pj_get_timestamp(&now);
    send_ts = find_send_ts(seq, rtp_index, rtp_index_start, rtp_index_end);
    if(send_ts == NULL) {
        insert_send_ts(seq, &now, rtp_index, &rtp_index_start, &rtp_index_end);
    } else {
        if(pj_elapsed_msec(send_ts, &now) < (pj_uint32_t)(5 + stream->rtcp.stat.rtt.mean / 1000)) {
            /* too early to resend */
            pj_memcpy(send_ts, &now, sizeof(pj_timestamp)); //update send timestamp
            return;
        }
    }
#else
    return;
#endif
       
//    /** rfc4588 and rfc 4585 */
//    if(stream->_RTX) {
//        pjmedia_rtp_hdr *rtp_header;
//        pj_uint32_t origin_seq;
//        buffer_to_send_ptr = data_buffer_rtx;
//        /** copy rtp header */
//        memcpy(data_buffer_rtx, data_buffer, sizeof(pjmedia_rtp_hdr));
//        rtp_header = (pjmedia_rtp_hdr*)data_buffer_rtx;
//        if(rtp_header->x) {
//            /** rtp header extension */
//            memcpy(data_buffer_rtx + sizeof(pjmedia_rtp_hdr), data_buffer + sizeof(pjmedia_rtp_hdr), 
//                sizeof(pjmedia_rtp_ext_hdr));
//        }
//        /** set rtx ssrc */
//        AssignUWord32ToBuffer(data_buffer_rtx + 8, stream->_ssrcRTX);
//        /** set rtx sequence number  and put origin sequence number behind 
//        rtp header */
//        origin_seq = rtp_header->seq;
//        PJ_LOG(4, (THIS_FILE, "origin_seq:%u, ntohs:%u", origin_seq, ntohs(rtp_header->seq)));
//        AssignUWord16ToBuffer(data_buffer_rtx + 2, stream->_sequenceNumberRTX++);
//        if(rtp_header->x) {
//            AssignUWord16ToBuffer(buffer_to_send_ptr + sizeof(pjmedia_rtp_hdr) + sizeof(pjmedia_rtp_ext_hdr), 
//                                    (pj_uint16_t) origin_seq);
//            /** add payload */
//            memcpy(buffer_to_send_ptr + sizeof(pjmedia_rtp_hdr) + sizeof(pjmedia_rtp_ext_hdr) + 2, 
//                    data_buffer + sizeof(pjmedia_rtp_hdr) + sizeof(pjmedia_rtp_ext_hdr), 
//                    len - sizeof(pjmedia_rtp_hdr) - sizeof(pjmedia_rtp_ext_hdr));
//        } else {
//            AssignUWord16ToBuffer(buffer_to_send_ptr + sizeof(pjmedia_rtp_hdr), 
//                                    (pj_uint16_t) origin_seq);
//            /** add payload */
//            memcpy(buffer_to_send_ptr + sizeof(pjmedia_rtp_hdr) + 2, data_buffer + sizeof(pjmedia_rtp_hdr), 
//                len - sizeof(pjmedia_rtp_hdr));
//        }
//        len += 2;       
//    }
//    PJ_LOG(4, (THIS_FILE, "resend now, len:%d, seq:%u", len, seq));
//    /** update statistics */
//    stream->enc->rtp.sent_retrans_packets++;
//    set_sent_retrans_packets(stream->enc->rtp.sent_retrans_packets);
    /** send now */
    resend_count++;
    pjmedia_transport_send_rtp(stream->transport, buffer_to_send_ptr, len);
}

PJ_DEF(void) on_rx_nack(void *data, pj_uint8_t * rtcp, int len) {
    pjmedia_vid_stream *stream = (pjmedia_vid_stream*) data;
    int i ;
    pj_uint32_t sendSSRC, mediaSSRC;
    pj_uint16_t nackSize = 256;
    pj_uint16_t nackList[256];
    pjmedia_receive_nack(rtcp, len, &sendSSRC, &mediaSSRC, 
                                &nackSize, nackList);
    if(nackSize == 0) {
        /** key frame request */
        //set_video_encoder_param(CODEC_PARAM_TYPE_KEYFRAME_REFRESH, 1);
    } else {
        for(i = 0; i < nackSize; ++i) {
           ResendPacket(stream, nackList[i]);
        }
    }
}

#endif
