#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ui.h"

int set_value_to(struct sockaddr *dest, unsigned int dest_len,
		 const char *name, const char *value)
{
	int sock;
	int ret;
	struct sockaddr_un sockaddr;
	struct ui_msg *msg;
	int len;
	int offset;

	if (strlen(name) == 0)
		return -EINVAL;

	sock = socket(dest->sa_family, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		return -errno;
	}

	len = sizeof(*msg) + strlen(name) + strlen(value) + 2;
	msg = malloc(len);
	if (!msg) {
		ret = -ENOMEM;
		goto out;
	}

	msg->type = MSG_SETVALUE;
	msg->name_value.name_len = strlen(name) + 1;
	msg->name_value.valu_len = strlen(value) + 1;

	offset = sizeof(*msg);
	memcpy((char*)msg + offset, name, msg->name_value.name_len);

	offset += msg->name_value.name_len;
	memcpy((char*)msg + offset, value, msg->name_value.valu_len);

	ret = sendto(sock, msg, len, 0, dest, dest_len);
	if (ret < 0) {
		perror("sendto");
		ret = -errno;
		goto out;
	}

	printf("Sent: %s=%s (%i: %i)\n", name, value, len, ret);
 out:
	close(sock);
	free(msg);

	return ret;
}

int set_value(const char *name, const char *value)
{
	struct sockaddr_un dest;

	dest.sun_family = AF_UNIX;
	strcpy(dest.sun_path, SOCKPATH);

	return set_value_to((struct sockaddr*)&dest, sizeof(dest), name, value);
}

int get_msg(int sock, struct ui_msg **msg)
{
	char buf[MAXMSG];
	int ret;

	ret = recv(sock, buf, MAXMSG, 0);
	if (ret < 0)
		return ret;

	*msg = malloc(ret);
	if (!*msg)
		return -ENOMEM;

	memcpy(*msg, buf, ret);

	return 0;
}

char *get_msg_name(struct ui_msg *msg)
{
	if (msg->type != MSG_SETVALUE)
		return NULL;

	return (char*)msg + sizeof(*msg);
}

char *get_msg_valu(struct ui_msg *msg)
{
	if (msg->type != MSG_SETVALUE)
		return NULL;

	return (char*)msg + sizeof(*msg) + msg->name_value.name_len;
}

#ifdef MAIN
int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: %s [NAME] [VALUE]\n", argv[0]);
		return 1;
	}

	return set_value(argv[1], argv[2]);
}
#endif
