/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */
/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#ifndef __UI_H
#define __UI_H

#include <stdint.h>
#include <sys/socket.h>

#define SOCKPATH "/tmp/KK7DS_UI"
#define MAXMSG 2048
#define SOCKPORT 9123

enum {
        MSG_SETVALUE,
        MSG_MAX
};

struct ui_msg {
        uint16_t type;
        uint16_t length;
        union {
                struct {
                        uint16_t name_len;
                        uint16_t valu_len;
                } name_value;
        };
};

int ui_connect(struct sockaddr *dest, unsigned int dest_len);
int ui_send(int sock, const char *name, const char *value);
int ui_send_to(struct sockaddr *dest, unsigned int dest_len,
               const char *name, const char *value);
int ui_get_msg(int sock, struct ui_msg **msg);
char *ui_get_msg_name(struct ui_msg *msg);
char *ui_get_msg_valu(struct ui_msg *msg);

#endif
