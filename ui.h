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
	union {
		struct {
			uint16_t name_len;
			uint16_t valu_len;
		} name_value;
	};
};

int set_value_to(struct sockaddr *dest, unsigned int dest_len,
		 const char *name, const char *value);
int set_value(const char *name, const char *value);
int get_msg(int sock, struct ui_msg **msg);
char *get_msg_name(struct ui_msg *msg);
char *get_msg_valu(struct ui_msg *msg);


#endif
