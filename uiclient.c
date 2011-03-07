#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ui.h"

int set_value(const char *name, const char *value)
{
	int sock;
	int ret;
	struct sockaddr_un sockaddr;
	struct ui_msg msg;

	if (strlen(name) == 0)
		return -EINVAL;

	sockaddr.sun_family = AF_UNIX;
	strcpy(sockaddr.sun_path, SOCKPATH);

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		return -errno;
	}

	msg.name_value.name_len = strlen(name);
	msg.name_value.valu_len = strlen(value);

	ret = sendto(sock, &msg, sizeof(msg), 0,
		     (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	if (ret < 0) {
		perror("sendto");
		ret = -errno;
		goto out;
	}

	ret = sendto(sock, name, strlen(name), 0,
		     (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	if (ret < 0) {
		perror("sendto");
		ret = -errno;
		goto out;
	}

	if (strlen(value)) {
		ret = sendto(sock, value, strlen(value), 0,
			     (struct sockaddr *)&sockaddr, sizeof(sockaddr));
		if (ret < 0) {
			perror("sendto");
			ret = -errno;
			goto out;
		}
	}

	//printf("Sent: %s=%s\n", name, value);
 out:
	close(sock);

	return ret;
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
