#ifndef SET_TZ_OFFSET_H
#define SET_TZ_OFFSET_H

#include <stdlib.h>
#include <mqueue.h>

#define _F_	__FILE__
#define _FU_	__FUNCTION__
#define _L_	__LINE__

#define IPC_QUEUE_NAME "/ipc_dispatch"
#define MMAP_INFO      "/tmp/mmap.info"

#define MID_P2P 1
#define MID_RMM 2
#define MID_CLOUD 4
#define MID_DISPATCH 4
#define MID_RCD 0x10

typedef enum {
    Y203C,
    Y23,
    Y25,
    Y30,
    H201C,
    H305R,
    H307,

    Y20GA,         // 9
    Y20GA_12,
    Y25GA,
    Y30QA,
    Y501GC,

    H30GA,         // 9
    H30GA_11,
    H51GA,
    H52GA,
    H60GA,        // 9
    H60GA_12,
    Q321BR_LSX,
    Q705BR,
    QG311R,
    R30GB,
    R35GB,
    R37GB,
    R40GA,
    Y211BA,
    Y211GA,        // 9
    Y211GA_12,
    Y213GA,
    Y21GA,         // 9
    Y21GA_12,
    Y28GA,
    Y291GA,        // 9,
    Y291GA_12,
    Y29GA,
    Y623,
    B091QP
} MODEL;

typedef enum {
    DISPATCH_SET_TZ_OFFSET = 0x8e,
} MSG_TYPE;

typedef struct
{
    int dstMid;
    int srcMid;
    short mainOp;
    short subOp;
    int msgLength;
} mqueue_msg_header_t;

void dump_string(char *source_file, const char *func, int line, char *text, ...);
int cloud_send_msg(mqd_t mqfd, MSG_TYPE msg_type, char *payload, int payload_len);
int cloud_set_tz_offset(int tz_offset);
int set_tz_offset_osd(int enable, int addr, int val);

#endif
