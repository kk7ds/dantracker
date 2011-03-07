#ifndef __UI_H
#define __UI_H

#include <stdint.h>

#define SOCKPATH "/tmp/KK7DS_UI"

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

int set_value(const char *name, const char *value);

#endif
