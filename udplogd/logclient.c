#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/netfilter_ipv4.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>

#include <sys/select.h>

#include "logger.h"

int main(int argc, char* argv[])
{
	if (argc < 5) {
		printf("Usage %s {ip address} {port} {log_mode} \"contents\"\n", argv[0]);
		printf("%s 127.0.0.1 26000 1 -> msg + LF       (no time, LF) --> 자체타임정보를 로그에 포함하는 경우 사용\n", argv[0]);
		printf("%s 127.0.0.1 26000 2 -> msg            (no time, no LF) --> 자체타임정보를 로그에 포함하는 경우 사용\n", argv[0]);
		printf("%s 127.0.0.1 26000 5 -> time + msg + LF(logserver time, LF)--> 같은 서버인 경우 로그서버시각이용을 추전\n", argv[0]);
		printf("%s 127.0.0.1 26000 6 -> time + msg     (logserver time, no LF)\n", argv[0]);
		return 0;
	}

	char* ip = argv[1];
	unsigned short port = atoi(argv[2]);
	char mode = (char)atoi(argv[3]);
	char* msg = argv[4];
	char buf[4096];
	int nLen = snprintf(buf, sizeof(buf), "%c%s", mode, msg);

	struct	sockaddr_in	addr_in;
	int		fd;
	memset(&addr_in, 0x00, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = INADDR_ANY;
	addr_in.sin_port = htons(0);// client 0 is random, 결정은 sendto() 최초호출 시
	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		printf("logclient socket create error errno = %d\n", errno);
		return -1;
	}

	struct sockaddr_in ServerAddr;
	memset(&ServerAddr, 0, sizeof(struct sockaddr_in));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = inet_addr(ip);
	ServerAddr.sin_port = htons(port);
	if (sendto(fd, buf, nLen, 0, (struct sockaddr*)&ServerAddr, sizeof(struct sockaddr_in)) != nLen) {
		printf("log send error errno = %d\n", errno);
	} else {
		printf("log send ok\n");
	}
	close(fd);

	return 0;

}