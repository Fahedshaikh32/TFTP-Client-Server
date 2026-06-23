/* tftp.c — Shared transfer logic (used by both server and client) */

#include "tftp.h"
#include <sys/time.h>

extern int cur_mode;     /* Transfer mode set by user / request  */
extern int blk_counter;  /* Current block number being sent      */

#define MAX_RETRIES  5   /* Max retransmit attempts before giving up */

/*
 * set_socket_timeout — Apply a receive timeout on the socket.
 *   secs = 0  → disable timeout (blocking forever)
 *   secs > 0  → recvfrom() returns after secs seconds if nothing arrives
 */
static void set_socket_timeout(int sd, int secs)
{
    struct timeval tv;
    tv.tv_sec  = secs;
    tv.tv_usec = 0;
    setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

/*
 * send_file — Read a local file and send it block-by-block to the peer.
 *             Retransmits automatically on timeout (packet loss).
 *             Gives up after MAX_RETRIES failed attempts.
 */
void send_file(int sd, struct sockaddr_in peer_addr, socklen_t peer_len, char *fname)
{
    int file_fd = open(fname, O_RDONLY);
    if (file_fd < 0) {
        perror(CLR_RED "send_file: cannot open file" CLR_RESET);
        return;
    }

    /* Enable 5-second receive timeout */
    set_socket_timeout(sd, WAIT_TIMEOUT);

    tftp_packet pkt, backup;
    int nbytes;

    /* -------- MODE 1 : Normal (512 bytes per block) -------- */
    if (cur_mode == MODE_NORMAL)
    {
        do {
            memset(&pkt, 0, sizeof(pkt));
            nbytes = read(file_fd, pkt.body.data_pkt.payload, MAX_DATA_SIZE);
            pkt.body.data_pkt.blk_num = blk_counter;
            pkt.opcode = OP_DATA;
            memcpy(&backup, &pkt, sizeof(pkt));

            int retries = 0;
            int ack_ok  = 0;

            while (!ack_ok && retries < MAX_RETRIES)
            {
                sendto(sd, &backup, sizeof(backup), 0,
                       (struct sockaddr *)&peer_addr, peer_len);
                printf(CLR_CYAN " [TX] Block #%d  |  Bytes: %d\n"
                       CLR_RESET, blk_counter, nbytes);

                memset(&pkt, 0, sizeof(pkt));
                int r = recvfrom(sd, &pkt, sizeof(pkt), 0,
                                 (struct sockaddr *)&peer_addr, &peer_len);
                if (r < 0) {
                    /* Timeout — retransmit */
                    retries++;
                    printf(CLR_RED " [!!] Timeout — retrying block #%d"
                           " (%d/%d)\n" CLR_RESET,
                           blk_counter, retries, MAX_RETRIES);
                } else if (nbytes == pkt.body.ack_pkt.blk_num) {
                    ack_ok = 1;
                }
            }

            if (!ack_ok) {
                printf(CLR_RED " [XX] Block #%d failed after %d retries."
                       " Aborting.\n" CLR_RESET, blk_counter, MAX_RETRIES);
                close(file_fd);
                set_socket_timeout(sd, 0);
                return;
            }

            blk_counter++;

        } while (nbytes == MAX_DATA_SIZE);
    }

    /* -------- MODE 2 : Octet (1 byte per block) -------- */
    else if (cur_mode == MODE_OCTET)
    {
        do {
            memset(&pkt, 0, sizeof(pkt));
            nbytes = read(file_fd, pkt.body.data_pkt.payload, 1);
            pkt.body.data_pkt.blk_num = blk_counter;
            pkt.opcode = OP_DATA;
            memcpy(&backup, &pkt, sizeof(pkt));

            int retries = 0;
            int ack_ok  = 0;

            while (!ack_ok && retries < MAX_RETRIES)
            {
                sendto(sd, &backup, sizeof(backup), 0,
                       (struct sockaddr *)&peer_addr, peer_len);
                printf(CLR_CYAN " [TX] Block #%d  |  Bytes: %d\n"
                       CLR_RESET, blk_counter, nbytes);

                memset(&pkt, 0, sizeof(pkt));
                int r = recvfrom(sd, &pkt, sizeof(pkt), 0,
                                 (struct sockaddr *)&peer_addr, &peer_len);
                if (r < 0) {
                    retries++;
                    printf(CLR_RED " [!!] Timeout — retrying block #%d"
                           " (%d/%d)\n" CLR_RESET,
                           blk_counter, retries, MAX_RETRIES);
                } else if (nbytes == pkt.body.ack_pkt.blk_num) {
                    ack_ok = 1;
                }
            }

            if (!ack_ok) {
                printf(CLR_RED " [XX] Block #%d failed after %d retries."
                       " Aborting.\n" CLR_RESET, blk_counter, MAX_RETRIES);
                close(file_fd);
                set_socket_timeout(sd, 0);
                return;
            }

            blk_counter++;

        } while (nbytes == 1);
    }

    /* -------- MODE 3 : Netascii (LF -> CR+LF conversion) -------- */
    else if (cur_mode == MODE_NETASCII)
    {
        do {
            memset(&pkt, 0, sizeof(pkt));

            char conv_buf[MAX_DATA_SIZE];
            memset(conv_buf, 0, MAX_DATA_SIZE);

            pkt.body.data_pkt.blk_num = blk_counter;
            pkt.opcode = OP_DATA;

            int idx = 0;
            char ch;

            while (idx < MAX_DATA_SIZE && read(file_fd, &ch, 1) > 0)
            {
                if (ch == '\n') {
                    conv_buf[idx++] = '\r';
                }
                conv_buf[idx++] = ch;
            }

            memcpy(pkt.body.data_pkt.payload, conv_buf, idx);
            nbytes = idx;
            memcpy(&backup, &pkt, sizeof(pkt));

            int retries = 0;
            int ack_ok  = 0;

            while (!ack_ok && retries < MAX_RETRIES)
            {
                sendto(sd, &backup, sizeof(backup), 0,
                       (struct sockaddr *)&peer_addr, peer_len);
                printf(CLR_CYAN " [TX] Block #%d  |  Bytes: %d\n"
                       CLR_RESET, blk_counter, nbytes);

                memset(&pkt, 0, sizeof(pkt));
                int r = recvfrom(sd, &pkt, sizeof(pkt), 0,
                                 (struct sockaddr *)&peer_addr, &peer_len);
                if (r < 0) {
                    retries++;
                    printf(CLR_RED " [!!] Timeout — retrying block #%d"
                           " (%d/%d)\n" CLR_RESET,
                           blk_counter, retries, MAX_RETRIES);
                } else if (nbytes == pkt.body.ack_pkt.blk_num) {
                    ack_ok = 1;
                }
            }

            if (!ack_ok) {
                printf(CLR_RED " [XX] Block #%d failed after %d retries."
                       " Aborting.\n" CLR_RESET, blk_counter, MAX_RETRIES);
                close(file_fd);
                set_socket_timeout(sd, 0);
                return;
            }

            blk_counter++;

        } while (nbytes == MAX_DATA_SIZE);
    }

    /* Disable timeout after transfer */
    set_socket_timeout(sd, 0);

    printf(CLR_GREEN ">>> FILE SENT SUCCESSFULLY <<<\n" CLR_RESET);
    close(file_fd);
}

/*
 * recv_file — Receive a file block-by-block from the peer and write locally.
 *             Sends ACK after each block. Retransmits ACK on timeout.
 */
void recv_file(int sd, struct sockaddr_in peer_addr, socklen_t peer_len, char *fname)
{
    int file_fd = open(fname, O_CREAT | O_WRONLY | O_EXCL, 0644);
    if (file_fd < 0) {
        file_fd = open(fname, O_WRONLY | O_TRUNC);
    }

    if (file_fd < 0) {
        perror(CLR_RED "recv_file: cannot open/create file" CLR_RESET);
        return;
    }

    /* Enable 5-second receive timeout */
    set_socket_timeout(sd, WAIT_TIMEOUT);

    tftp_packet pkt;
    int nbytes;

    /* -------- MODE 1 & 3 : Normal / Netascii -------- */
    if (cur_mode == MODE_NORMAL || cur_mode == MODE_NETASCII)
    {
        do {
            int retries = 0;
            int recv_ok = 0;

            while (!recv_ok && retries < MAX_RETRIES)
            {
                memset(&pkt, 0, sizeof(pkt));
                int r = recvfrom(sd, &pkt, sizeof(pkt), 0,
                                 (struct sockaddr *)&peer_addr, &peer_len);
                if (r < 0) {
                    retries++;
                    printf(CLR_RED " [!!] Timeout waiting for data block"
                           " (%d/%d)\n" CLR_RESET, retries, MAX_RETRIES);
                } else {
                    recv_ok = 1;
                }
            }

            if (!recv_ok) {
                printf(CLR_RED " [XX] No data received after %d retries."
                       " Aborting.\n" CLR_RESET, MAX_RETRIES);
                close(file_fd);
                set_socket_timeout(sd, 0);
                return;
            }

            nbytes = write(file_fd, pkt.body.data_pkt.payload,
                           strlen(pkt.body.data_pkt.payload));

            printf(CLR_YELLOW " [RX] Block #%d  |  Written: %d bytes\n"
                   CLR_RESET, pkt.body.data_pkt.blk_num, nbytes);

            memset(&pkt, 0, sizeof(pkt));
            pkt.opcode               = OP_ACK;
            pkt.body.ack_pkt.blk_num = nbytes;
            sendto(sd, &pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&peer_addr, peer_len);

        } while (nbytes == MAX_DATA_SIZE);
    }

    /* -------- MODE 2 : Octet -------- */
    else if (cur_mode == MODE_OCTET)
    {
        do {
            int retries = 0;
            int recv_ok = 0;

            while (!recv_ok && retries < MAX_RETRIES)
            {
                memset(&pkt, 0, sizeof(pkt));
                int r = recvfrom(sd, &pkt, sizeof(pkt), 0,
                                 (struct sockaddr *)&peer_addr, &peer_len);
                if (r < 0) {
                    retries++;
                    printf(CLR_RED " [!!] Timeout waiting for data block"
                           " (%d/%d)\n" CLR_RESET, retries, MAX_RETRIES);
                } else {
                    recv_ok = 1;
                }
            }

            if (!recv_ok) {
                printf(CLR_RED " [XX] No data received after %d retries."
                       " Aborting.\n" CLR_RESET, MAX_RETRIES);
                close(file_fd);
                set_socket_timeout(sd, 0);
                return;
            }

            nbytes = write(file_fd, pkt.body.data_pkt.payload,
                           strlen(pkt.body.data_pkt.payload));

            printf(CLR_YELLOW " [RX] Block #%d  |  Written: %d bytes\n"
                   CLR_RESET, pkt.body.data_pkt.blk_num, nbytes);

            memset(&pkt, 0, sizeof(pkt));
            pkt.opcode               = OP_ACK;
            pkt.body.ack_pkt.blk_num = nbytes;
            sendto(sd, &pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&peer_addr, peer_len);

        } while (nbytes == 1);
    }

    /* Disable timeout after transfer */
    set_socket_timeout(sd, 0);

    close(file_fd);
    printf(CLR_GREEN ">>> FILE RECEIVED SUCCESSFULLY <<<\n" CLR_RESET);
}
