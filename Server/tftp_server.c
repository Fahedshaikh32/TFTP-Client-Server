/* tftp_server.c — TFTP Server with fork() + unique port per client */

#include "tftp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

/* ---------- Global State ---------- */
int cur_mode    = MODE_NORMAL;
int blk_counter = 1;

/* ---------- Port Counter ----------
 * Each forked child gets its own unique port starting from 20000.
 * Parent increments this after every fork.
 */
int next_port = 20000;

/* ---------- Forward Declaration ---------- */
void handle_request(int sd, struct sockaddr_in cli_addr,
                    socklen_t cli_len, tftp_packet *pkt, int child_port);

/* ========================================================
 *  main — Bind to SERVER_PORT (6969), wait for requests,
 *         fork a child for each one with a unique port.
 * ======================================================== */
int main(void)
{
    int sd;
    struct sockaddr_in srv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    memset(&srv_addr, 0, sizeof(srv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    tftp_packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    /* Create main UDP socket on port 6969 */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) {
        perror(CLR_RED "Server: socket creation failed" CLR_RESET);
        exit(EXIT_FAILURE);
    }

    /* Allow port reuse so restart does not fail */
    int opt = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons(SERVER_PORT);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        perror(CLR_RED "Server: bind failed" CLR_RESET);
        close(sd);
        exit(EXIT_FAILURE);
    }

    printf(CLR_MAGENTA
           "==========================================\n"
           "   TFTP Server  |  Listening on Port %d\n"
           "   Each client gets a unique port (20000+)\n"
           "==========================================\n"
           CLR_RESET, SERVER_PORT);

    /* Reap zombie children automatically */
    signal(SIGCHLD, SIG_IGN);

    /* Main server loop */
    while (1)
    {
        memset(&pkt, 0, sizeof(pkt));

        /* Wait for incoming RRQ or WRQ on port 6969 */
        int rx = recvfrom(sd, &pkt, sizeof(pkt), 0,
                          (struct sockaddr *)&cli_addr, &cli_len);
        if (rx < 0) {
            perror(CLR_RED "Server: recvfrom failed" CLR_RESET);
            continue;
        }

        printf(CLR_CYAN "\n[+] Request from %s  |  Assigning port %d\n"
               CLR_RESET, inet_ntoa(cli_addr.sin_addr), next_port);

        /* Assign a unique port to this client */
        int assigned_port = next_port++;

        /* Fork a child process to handle this client */
        pid_t pid = fork();

        if (pid < 0)
        {
            perror(CLR_RED "Server: fork failed" CLR_RESET);
            continue;
        }
        else if (pid == 0)
        {
            /* ---- CHILD PROCESS ---- */
            /* Child closes the parent's main socket */
            close(sd);

            /* Child creates its OWN socket on the assigned unique port */
            int child_sd;
            struct sockaddr_in child_addr;
            memset(&child_addr, 0, sizeof(child_addr));

            child_sd = socket(AF_INET, SOCK_DGRAM, 0);
            if (child_sd < 0) {
                perror(CLR_RED "Child: socket failed" CLR_RESET);
                exit(EXIT_FAILURE);
            }

            int copt = 1;
            setsockopt(child_sd, SOL_SOCKET, SO_REUSEADDR, &copt, sizeof(copt));

            child_addr.sin_family      = AF_INET;
            child_addr.sin_port        = htons(assigned_port);
            child_addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(child_sd, (struct sockaddr *)&child_addr,
                     sizeof(child_addr)) < 0) {
                perror(CLR_RED "Child: bind on unique port failed" CLR_RESET);
                close(child_sd);
                exit(EXIT_FAILURE);
            }

            printf(CLR_YELLOW
                   "[Child PID:%d] Handling '%s' on port %d\n"
                   CLR_RESET, getpid(),
                   pkt.body.req_pkt.fname, assigned_port);

            /* Handle the actual file transfer on the unique port */
            handle_request(child_sd, cli_addr, cli_len, &pkt, assigned_port);

            close(child_sd);

            printf(CLR_GREEN
                   "[Child PID:%d] Transfer complete. Exiting.\n"
                   CLR_RESET, getpid());

            exit(EXIT_SUCCESS);
            /* ---- END CHILD ---- */
        }
        else
        {
            /* ---- PARENT PROCESS ---- */
            /* Parent goes back to listening immediately */
            printf(CLR_MAGENTA
                   "[Parent] Forked child PID:%d for port %d\n"
                   "==========================================\n"
                   "   Waiting for next request ...\n"
                   "==========================================\n"
                   CLR_RESET, pid, assigned_port);
        }
    }

    close(sd);
    return 0;
}

/* ========================================================
 *  handle_request — Called inside child process.
 *  Decides send or receive based on opcode,
 *  communicates with client on the unique child port.
 * ======================================================== */
void handle_request(int sd, struct sockaddr_in cli_addr,
                    socklen_t cli_len, tftp_packet *pkt, int child_port)
{
    /* Sync mode and reset block counter */
    cur_mode    = pkt->body.req_pkt.tmode;
    blk_counter = 1;

    printf(CLR_YELLOW
           "[*] File: %-30s | Mode: %d | Port: %d\n"
           CLR_RESET,
           pkt->body.req_pkt.fname, cur_mode, child_port);

    /* ---- WRQ: Client wants to upload ---- */
    if (pkt->opcode == OP_WRQ)
    {
        /* Open or create the destination file */
        int tmp_fd = open(pkt->body.req_pkt.fname,
                          O_CREAT | O_WRONLY | O_EXCL, 0644);
        if (tmp_fd < 0) {
            tmp_fd = open(pkt->body.req_pkt.fname, O_WRONLY | O_TRUNC);
        }
        close(tmp_fd);

        /* Send ACK to tell client we are ready to receive */
        tftp_packet ack;
        memset(&ack, 0, sizeof(ack));
        ack.opcode               = OP_ACK;
        ack.body.ack_pkt.blk_num = BLK_WR;
        sendto(sd, &ack, sizeof(ack), 0,
               (struct sockaddr *)&cli_addr, cli_len);

        recv_file(sd, cli_addr, cli_len, pkt->body.req_pkt.fname);
        return;
    }

    /* ---- RRQ: Client wants to download ---- */
    else if (pkt->opcode == OP_RRQ)
    {
        /* Check if file exists using O_EXCL trick */
        int tmp_fd = open(pkt->body.req_pkt.fname,
                          O_CREAT | O_RDONLY | O_EXCL, 0644);
        if (tmp_fd < 0)
        {
            /* File exists — serve it */
            close(tmp_fd);

            /* Send ACK to tell client we are ready to send */
            tftp_packet ack;
            memset(&ack, 0, sizeof(ack));
            ack.opcode               = OP_ACK;
            ack.body.ack_pkt.blk_num = BLK_RD;
            sendto(sd, &ack, sizeof(ack), 0,
                   (struct sockaddr *)&cli_addr, cli_len);

            send_file(sd, cli_addr, cli_len, pkt->body.req_pkt.fname);
            return;
        }
        else
        {
            /* File does not exist — clean up and send error */
            close(tmp_fd);
            unlink(pkt->body.req_pkt.fname);

            tftp_packet err_pkt;
            memset(&err_pkt, 0, sizeof(err_pkt));
            err_pkt.opcode                = OP_ERROR;
            err_pkt.body.err_pkt.err_code = 1;
            snprintf(err_pkt.body.err_pkt.err_msg,
                     sizeof(err_pkt.body.err_pkt.err_msg),
                     "ERROR: File '%s' not found on server.",
                     pkt->body.req_pkt.fname);

            sendto(sd, &err_pkt, sizeof(err_pkt), 0,
                   (struct sockaddr *)&cli_addr, cli_len);

            printf(CLR_RED "[-] RRQ failed — '%s' not found.\n" CLR_RESET,
                   pkt->body.req_pkt.fname);
        }
    }
}
