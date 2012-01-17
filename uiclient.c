/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ui.h"

int ui_connect(struct sockaddr *dest, unsigned int dest_len)
{
	int sock;

	sock = socket(dest->sa_family, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return -errno;
	}

	if (connect(sock, dest, dest_len)) {
		perror("connect");
		close(sock);
		return -errno;
	}

	return sock;
}

int ui_send(int sock, const char *name, const char *value)
{
	int ret;
	int len;
	int offset;
	struct ui_msg *msg;

	len = sizeof(*msg) + strlen(name) + strlen(value) + 2;
	msg = malloc(len);
	if (!msg)
		return -ENOMEM;

	msg->type = MSG_SETVALUE;
	msg->length = len;
	msg->name_value.name_len = strlen(name) + 1;
	msg->name_value.valu_len = strlen(value) + 1;

	offset = sizeof(*msg);
	memcpy((char*)msg + offset, name, msg->name_value.name_len);

	offset += msg->name_value.name_len;
	memcpy((char*)msg + offset, value, msg->name_value.valu_len);

	ret = send(sock, msg, len, MSG_NOSIGNAL);

	free(msg);
	return ret;
}

int ui_send_to(struct sockaddr *dest, unsigned int dest_len,
	       const char *name, const char *value)
{
	int sock;
	int ret;

	if (strlen(name) == 0)
		return -EINVAL;

	sock = ui_connect(dest, dest_len);
	if (sock < 0)
		return sock;

	ret = ui_send(sock, name, value);

	close(sock);

	return ret;
}

int ui_get_msg(int sock, struct ui_msg **msg)
{
	struct ui_msg hdr;
	int ret;

	ret = read(sock, &hdr, sizeof(hdr));
	if (ret <= 0)
		return ret;

	*msg = malloc(hdr.length);
	if (!*msg)
		return -ENOMEM;

	memcpy(*msg, &hdr, sizeof(hdr));

	ret = read(sock, ((char *)*msg)+sizeof(hdr), hdr.length - sizeof(hdr));

	return 1;
}

char *ui_get_msg_name(struct ui_msg *msg)
{
	if (msg->type != MSG_SETVALUE)
		return NULL;

	return (char*)msg + sizeof(*msg);
}

char *ui_get_msg_valu(struct ui_msg *msg)
{
	if (msg->type != MSG_SETVALUE)
		return NULL;

	return (char*)msg + sizeof(*msg) + msg->name_value.name_len;
}

#ifdef MAIN
#include <sys/un.h>
#include <netinet/in.h>

int ui_send_default(const char *name, const char *value)
{
	if (getenv("INET")) {
		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(SOCKPORT);
		sin.sin_addr.s_addr = 0x7F000001; /* 127.0.0.1 */
		return ui_send_to((struct sockaddr *)&sin, sizeof(sin),
				  name, value);
	} else {
		struct sockaddr_un sun;
		sun.sun_family = AF_UNIX;
		strcpy(sun.sun_path, SOCKPATH);
		return ui_send_to((struct sockaddr *)&sun, sizeof(sun),
				  name, value);
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: %s [NAME] [VALUE]\n", argv[0]);
		return 1;
	}

	return ui_send_default(argv[1], argv[2]);
}
#endif
