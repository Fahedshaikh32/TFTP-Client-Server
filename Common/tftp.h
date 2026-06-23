/* Header file shared between server and client */

#ifndef TFTP_H
#define TFTP_H

#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

/* ---------- Terminal Color Codes ---------- */
#define CLR_RED         "\033[1;3;31m"
#define CLR_GREEN       "\033[1;32m"
#define CLR_YELLOW      "\033[0;3;33m"
#define CLR_BLUE        "\033[1;3;34m"
#define CLR_MAGENTA     "\033[1;3;35m"
#define CLR_CYAN        "\033[1;3;36m"
#define CLR_RESET       "\033[0m"

/* ---------- Configuration ---------- */
#define SERVER_PORT     6969
#define MAX_DATA_SIZE   512                  /* Max bytes per DATA block       */
#define PKT_BUFFER_SIZE (MAX_DATA_SIZE + 4)  /* Data + 4-byte TFTP header      */
#define WAIT_TIMEOUT    5                    /* Retransmit timeout in seconds  */

/* ---------- TFTP Operation Codes ---------- */
typedef enum {
    OP_RRQ   = 1,  /* Read  Request  */
    OP_WRQ   = 2,  /* Write Request  */
    OP_DATA  = 3,  /* Data  Packet   */
    OP_ACK   = 4,  /* Acknowledgment */
    OP_ERROR = 5   /* Error  Packet  */
} tftp_opcode;

/* ---------- Sentinel Block Numbers ---------- */
#define RET_FAIL    -1
#define RET_OK       0
#define BLK_RD   65000   /* Sent in ACK to signal "ready to send"    */
#define BLK_WR   65001   /* Sent in ACK to signal "ready to receive" */

/* ---------- Transfer Modes ---------- */
#define MODE_NORMAL    1   /* 512-byte block mode  */
#define MODE_OCTET     2   /* byte-by-byte mode    */
#define MODE_NETASCII  3   /* newline-conv mode    */

/* ---------- TFTP Packet Structure ---------- */
typedef struct {
    uint16_t opcode;
    union {
        struct {
            char fname[256];  /* Requested filename  */
            int  tmode;       /* Transfer mode (1/2/3) */
        } req_pkt;            /* Used by RRQ and WRQ */

        struct {
            uint16_t blk_num;
            char     payload[MAX_DATA_SIZE];
        } data_pkt;           /* Used by DATA packets */

        struct {
            uint16_t blk_num;
        } ack_pkt;            /* Used by ACK packets  */

        struct {
            uint16_t err_code;
            char     err_msg[MAX_DATA_SIZE];
        } err_pkt;            /* Used by ERROR packets */
    } body;
} tftp_packet;

/* ---------- Shared Function Prototypes ---------- */
void send_file(int sd, struct sockaddr_in peer_addr, socklen_t peer_len, char *fname);
void recv_file(int sd, struct sockaddr_in peer_addr, socklen_t peer_len, char *fname);

#endif /* TFTP_H */
