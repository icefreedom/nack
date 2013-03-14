#ifndef __NACK_H__
#define __NACK_H__
#include <pjmedia/types.h>
#include <pjmedia/transport.h>
#include <pj/types.h>
PJ_BEGIN_DECL
#define RTCP_NACK   205
PJ_DECL(void) pjmedia_send_nack(pjmedia_transport *transport, pj_uint32_t ownSSRC,
                                pj_uint32_t remoteSSRC, const pj_uint32_t nackSize,
                                const pj_uint16_t *nackList);
PJ_DECL(void) pjmedia_receive_nack(pj_uint8_t* buffer, int len,
                             pj_uint32_t* sendSSRC,
                            pj_uint32_t* mediaSSRC, pj_uint16_t* nackSize,
                            pj_uint16_t* nackList);
PJ_DECL(pj_status_t) ParseNack(pj_uint8_t* dataBuffer, pj_uint32_t* sendSSRC,
                            pj_uint32_t* mediaSSRC, pj_uint16_t* nackSize,
                            pj_uint16_t* nackList);
PJ_DECL(pj_status_t) BuildNack(pj_uint8_t *rtcpBuffer,
                            int* pos_origin,
                            const pj_uint32_t nackSize,
                            const pj_uint16_t *nackList,
                            const pj_uint32_t ownSSRC,
                            const pj_uint32_t remoteSSRC) ;
PJ_DECL(void) on_rx_nack(void *data, pj_uint8_t * rtcp, int len) ;
PJ_END_DECL

#endif
