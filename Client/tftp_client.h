/* Client-specific header */

#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

#include "tftp.h"

/* ---------- Client Context Structure ---------- */
typedef struct {
    int  sd;                          /* Socket descriptor        */
    struct sockaddr_in  srv_addr;     /* Server address struct     */
    socklen_t           srv_len;      /* Length of server address  */
    char                srv_ip[INET_ADDRSTRLEN]; /* Server IP string */
} tftp_client_ctx;

/* ---------- Client Function Prototypes ---------- */
void cli_connect(tftp_client_ctx *ctx, char *ip, int port);
void cli_put(tftp_client_ctx *ctx, char *fname);
void cli_get(tftp_client_ctx *ctx, char *fname);
void cli_disconnect(tftp_client_ctx *ctx);
void cli_handle_cmd(tftp_client_ctx *ctx, char *cmd);

void dispatch_request(int sd, struct sockaddr_in srv_addr, char *fname, int opcode);
void await_response(int sd, struct sockaddr_in srv_addr, char *fname, int opcode);

int  is_valid_ip(char *ip_str);

#endif /* TFTP_CLIENT_H */
