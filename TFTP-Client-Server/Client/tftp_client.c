/* tftp_client.c — TFTP Client: interactive shell for get/put operations */

#include "tftp.h"
#include "tftp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio_ext.h>
#include <sys/wait.h>
#include <fcntl.h>

/* ---------- Global State ---------- */
int cur_mode    = MODE_NORMAL;  /* Transfer mode (1=Normal, 2=Octet, 3=Netascii) */
int blk_counter = 1;           /* Block number counter, reset before each transfer */

/* ========================================================
 *  main — Display a menu and process user commands in a
 *         loop until the user types "exit".
 * ======================================================== */
int main(void)
{
    char cmd_buf[256];
    tftp_client_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));

    printf(CLR_MAGENTA
           "=========================================\n"
           "        TFTP Client  —  Ready\n"
           "=========================================\n"
           CLR_RESET);

    while (1)
    {
        __fpurge(stdin);
        printf(CLR_MAGENTA
               "\nCOMMANDS:\n"
               "  connect  — Connect to a TFTP server\n"
               "  put      — Upload a file to server\n"
               "  get      — Download a file from server\n"
               "  mode     — Change transfer mode\n"
               "  exit     — Quit the client\n"
               ">> " CLR_RESET);

        fgets(cmd_buf, sizeof(cmd_buf), stdin);
        cmd_buf[strcspn(cmd_buf, "\n")] = '\0';  /* Strip trailing newline */

        cli_handle_cmd(&ctx, cmd_buf);
    }

    return 0;
}

/* ========================================================
 *  cli_handle_cmd — Parse the user's command string and
 *  call the appropriate function.
 * ======================================================== */
void cli_handle_cmd(tftp_client_ctx *ctx, char *cmd)
{
    /* ---- connect ---- */
    if (strcmp(cmd, "connect") == 0)
    {
        printf(CLR_MAGENTA "Enter server IP address: " CLR_RESET);
        scanf("%s", ctx->srv_ip);

        if (is_valid_ip(ctx->srv_ip) == RET_FAIL) {
            printf(CLR_RED "[-] Invalid IP address — please try again.\n" CLR_RESET);
            return;
        }

        printf(CLR_GREEN "[+] IP address accepted.\n" CLR_RESET);
        cli_connect(ctx, ctx->srv_ip, SERVER_PORT);
        printf(CLR_GREEN "[+] Connected to %s:%d\n" CLR_RESET,
               ctx->srv_ip, SERVER_PORT);
    }

    /* ---- put (upload) ---- */
    else if (strcmp(cmd, "put") == 0)
    {
        /* Spawn a child to list local files for the user's reference */
        pid_t child = fork();
        if (child > 0) {
            wait(NULL);   /* Parent waits for ls to finish */
        } else if (child == 0) {
            execlp("ls", "ls", "-lh", "--color=auto", NULL);
            exit(0);
        }

        char fname[64];
        printf(CLR_MAGENTA "Enter filename to upload: " CLR_RESET);
        __fpurge(stdin);
        scanf("%s", fname);

        cli_put(ctx, fname);
    }

    /* ---- get (download) ---- */
    else if (strcmp(cmd, "get") == 0)
    {
        char fname[64];
        printf(CLR_MAGENTA "Enter filename to download: " CLR_RESET);
        __fpurge(stdin);
        scanf("%s", fname);

        cli_get(ctx, fname);
    }

    /* ---- mode ---- */
    else if (strcmp(cmd, "mode") == 0)
    {
        printf(CLR_MAGENTA
               "Select Transfer Mode:\n"
               "  1 — Normal   (512-byte blocks)\n"
               "  2 — Octet    (byte-by-byte)\n"
               "  3 — Netascii (LF → CR+LF)\n"
               ">> " CLR_RESET);
        scanf("%d", &cur_mode);

        const char *mode_names[] = { "", "Normal", "Octet", "Netascii" };
        if (cur_mode >= 1 && cur_mode <= 3) {
            printf(CLR_GREEN "[+] Mode set to: %s\n" CLR_RESET,
                   mode_names[cur_mode]);
        } else {
            printf(CLR_RED "[-] Invalid mode. Defaulting to Normal.\n" CLR_RESET);
            cur_mode = MODE_NORMAL;
        }
    }

    /* ---- exit ---- */
    else if (strcmp(cmd, "exit") == 0)
    {
        printf(CLR_YELLOW "[*] Closing connection and exiting...\n" CLR_RESET);
        cli_disconnect(ctx);
        printf(CLR_GREEN "Goodbye!\n" CLR_RESET);
        exit(0);
    }

    /* ---- unknown ---- */
    else
    {
        printf(CLR_RED "[-] Unknown command: '%s'\n"
                       "    Type one of: connect, put, get, mode, exit\n"
               CLR_RESET, cmd);
    }
}

/* ========================================================
 *  cli_connect — Initialize the UDP socket and populate
 *  the server address structure. No packet is sent here.
 * ======================================================== */
void cli_connect(tftp_client_ctx *ctx, char *ip, int port)
{
    ctx->sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->sd < 0) {
        perror(CLR_RED "cli_connect: socket failed" CLR_RESET);
        return;
    }

    ctx->srv_addr.sin_family      = AF_INET;
    ctx->srv_addr.sin_port        = htons(port);
    ctx->srv_addr.sin_addr.s_addr = inet_addr(ip);
    ctx->srv_len                  = sizeof(ctx->srv_addr);
}

/* ========================================================
 *  cli_put — Verify the file exists locally, then send a
 *  WRQ to the server to begin an upload.
 * ======================================================== */
void cli_put(tftp_client_ctx *ctx, char *fname)
{
    /* If open with O_EXCL succeeds, the file does NOT exist */
    int probe = open(fname, O_CREAT | O_RDONLY | O_EXCL, 0644);
    if (probe != -1) {
        close(probe);
        unlink(fname);   /* Remove the accidentally-created file */
        printf(CLR_RED "[-] File '%s' not found locally.\n" CLR_RESET, fname);
        return;
    }

    ctx->srv_len = sizeof(ctx->srv_addr);
    dispatch_request(ctx->sd, ctx->srv_addr, fname, OP_WRQ);
}

/* ========================================================
 *  cli_get — Send an RRQ to the server to begin a download.
 * ======================================================== */
void cli_get(tftp_client_ctx *ctx, char *fname)
{
    dispatch_request(ctx->sd, ctx->srv_addr, fname, OP_RRQ);
}

/* ========================================================
 *  cli_disconnect — Close the UDP socket.
 * ======================================================== */
void cli_disconnect(tftp_client_ctx *ctx)
{
    if (ctx->sd > 0) {
        close(ctx->sd);
        ctx->sd = 0;
    }
}

/* ========================================================
 *  dispatch_request — Build and send an RRQ or WRQ packet,
 *  then wait for the server's response.
 * ======================================================== */
void dispatch_request(int sd, struct sockaddr_in srv_addr,
                      char *fname, int opcode)
{
    blk_counter = 1;   /* Always start block numbering fresh */

    tftp_packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.opcode             = opcode;
    pkt.body.req_pkt.tmode = cur_mode;
    strncpy(pkt.body.req_pkt.fname, fname,
            sizeof(pkt.body.req_pkt.fname) - 1);

    sendto(sd, &pkt, sizeof(pkt), 0,
           (struct sockaddr *)&srv_addr, sizeof(srv_addr));

    printf(CLR_CYAN "[*] %s request sent for '%s' (mode %d)\n" CLR_RESET,
           (opcode == OP_WRQ) ? "WRQ" : "RRQ", fname, cur_mode);

    /* Block until the server responds */
    await_response(sd, srv_addr, fname, opcode);
}

/* ========================================================
 *  await_response — Wait for the server's first reply
 *  (ACK or ERROR) and start the appropriate data transfer.
 * ======================================================== */
void await_response(int sd, struct sockaddr_in srv_addr,
                    char *fname, int opcode)
{
    tftp_packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    socklen_t srv_len = sizeof(srv_addr);
    recvfrom(sd, &pkt, sizeof(pkt), 0,
             (struct sockaddr *)&srv_addr, &srv_len);

    if (pkt.opcode == OP_ACK)
    {
        if (pkt.body.ack_pkt.blk_num == BLK_WR)
        {
            /* Server is ready to receive — start sending */
            printf(CLR_GREEN "[+] Server ready — uploading...\n" CLR_RESET);
            send_file(sd, srv_addr, srv_len, fname);
        }
        else if (pkt.body.ack_pkt.blk_num == BLK_RD)
        {
            /* Server is ready to send — start receiving */
            printf(CLR_GREEN "[+] Server ready — downloading...\n" CLR_RESET);
            recv_file(sd, srv_addr, srv_len, fname);
        }
        /* Any other ACK block number is ignored */
    }
    else if (pkt.opcode == OP_ERROR)
    {
        printf(CLR_RED "[-] Server error: %s\n" CLR_RESET,
               pkt.body.err_pkt.err_msg);
    }
}

/* ========================================================
 *  is_valid_ip — Basic IPv4 validation:
 *    • Only digits and dots allowed
 *    • Exactly 3 dots required
 * ======================================================== */
int is_valid_ip(char *ip_str)
{
    int dot_count = 0;

    for (int i = 0; ip_str[i] != '\0'; i++)
    {
        char c = ip_str[i];

        if (c == '.') {
            dot_count++;
            continue;
        }

        if (c < '0' || c > '9') {
            return RET_FAIL;   /* Non-numeric, non-dot character found */
        }
    }

    if (dot_count != 3) {
        return RET_FAIL;       /* IPv4 must have exactly 3 dots */
    }

    return RET_OK;
}
