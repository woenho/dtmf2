#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <stddef.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <dirent.h>
#include <bits/siginfo.h>
#include <json/json.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/netfilter_ipv4.h>
#include <linux/sockios.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include<syslog.h>

#include "civetweb.h"
#include "logger.h"

/* Get an OS independent definition for sleep() */
#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x)*1000)
#else
#include <unistd.h>
#endif

#define TERM_CHECK		120

typedef struct user_data_t {
	int size;		// object size
	int nStop;		// 내가 연결 종료를 요청했다면 0이 아님
	int count;		// 매번 로깅하지 않고 일정 횟수 통신 후 로깅을 위한 카운트 
	time_t startup;		// 연결을 시작한 시각
	time_t send_last;	// 마지막 발신 시각
	time_t recv_last;	// 마지막 수신 시각(이거 검증하여 통신상태 확인??? 의미없나?)
	char	data[];		// 유저데이타 영역으로 사용하기 위한 예약으로 크기는 가변임
	// data object를 userdata라하면, data크기 = userdata->size - ( userdata->data - userdata);
} UserData;

typedef struct http_header {
	const char* name;  /* HTTP header name */
	const char* value; /* HTTP header value */
}HTTP_HEADER;

#define HTTP_MAX_HEADERS	30

typedef struct http_request_info {
	const char* request_method;		/* "GET", "POST", etc */
	char* request_uri;				/* URL-decoded URI (absolute or relative,* as in the request) */
	const char* query_string;		/* URL part after '?', not including '?', or NULL */
	const char* http_version;		/* E.g. "1.0", "1.1" */
	char* body_string;				/* post body or NULL */
	uint32_t content_length;		/* Length (in bytes) of the request body, can be -1 if no length was given. */
	int num_headers;				/* Number of HTTP headers */
	HTTP_HEADER http_headers[HTTP_MAX_HEADERS]; /* Allocate maximum headers */

}REQUEST_INFO, * PREQUEST_INFO;

typedef struct http_response_info {
	const char* http_version;		/* E.g. "1.0", "1.1" */
	const char* response;			/* http status code */
	char* msg;						/* URL-decoded URI (absolute or relative,* as in the request) */
	char* body_string;				/* post body or NULL */
	uint32_t content_length;		/* Length (in bytes) of the request body, can be -1 if no length was given. */
	int num_headers;				/* Number of HTTP headers */
	HTTP_HEADER http_headers[HTTP_MAX_HEADERS]; /* Allocate maximum headers */

}RESPONSE_INFO, * PRESPONSE_INFO;

sigset_t	g_sigsBlock;	///< 시그널 구조
sigset_t	g_sigsOld;		///< 시그널 구조

int			g_Run = 1;
int			g_sd = -1;

static const char * msgtypename(int flags) {
	unsigned f = (unsigned)flags & 0xFu;
	switch (f) {
	case MG_WEBSOCKET_OPCODE_CONTINUATION:
		return "continuation";
	case MG_WEBSOCKET_OPCODE_TEXT:
		return "text";
	case MG_WEBSOCKET_OPCODE_BINARY:
		return "binary";
	case MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE:
		return "connection close";
	case MG_WEBSOCKET_OPCODE_PING:
		return "PING";
	case MG_WEBSOCKET_OPCODE_PONG:
		return "PONG";
	}
	return "unknown";
}

int signal_init(void (*sa)(int, siginfo_t*, void*), int debug_sig_message)
{

#if 0	
	// 특정 시그널 빼고 등록
	if ((sigfillset(&g_sigsBlock) == -1)
		|| (sigdelset(&g_sigsBlock, SIGINT) == -1)
		|| (sigdelset(&g_sigsBlock, SIGKILL) == -1)
		|| (sigdelset(&g_sigsBlock, SIGUSR1) == -1)
		|| (sigdelset(&g_sigsBlock, SIGSEGV) == -1)
		|| (sigdelset(&g_sigsBlock, SIGALRM) == -1)
		|| (sigdelset(&g_sigsBlock, SIGTERM) == -1)
		|| (sigprocmask(SIG_SETMASK, &g_sigsBlock, &g_sigsOld) == -1)
		)
#else
	/// 모든 시그널 등록 
	if ((sigemptyset(&g_sigsBlock) == -1) || (sigprocmask(SIG_SETMASK, &g_sigsBlock, &g_sigsOld) == -1))
#endif		
	{
		conpt("Signal Set (Pid:%d) (Er:%d,%s)", getpid(), errno, strerror(errno));
		return -1;
	}

	struct sigaction act;
	act.sa_flags = SA_SIGINFO;// SA_NOCLDSTOP, SA_NOCLDWAIT, SA_ONSTACK , SA_RESTART, SA_NODEFER, SA_RESETHAND, ....
	act.sa_sigaction = sa;
#if 0
	// 특정 시그널 만 처리하도록 함수 등록
	if ((sigemptyset(&act.sa_mask) == -1)
		|| (sigaction(SIGINT, &act, NULL) == -1)
		|| (sigaction(SIGUSR1, &act, NULL) == -1)
		|| (sigaction(SIGSEGV, &act, NULL) == -1)
		|| (sigaction(SIGALRM, &act, NULL) == -1)
		|| (sigaction(SIGTERM, &act, NULL) == -1)
		)
	{
		printf("Sigaction (Pid:%d) (Er:%d,%s)", getpid(), errno, (char*)strerror(errno));
		return -1;
	}
#else
	/// 모든 시그널을 함수로 보내자
	if ((sigemptyset(&act.sa_mask) == -1))
	{
		conpt("sigemptyset() (Pid:%d) (Er:%d,%s)", getpid(), errno, (char*)strerror(errno));
		return -1;
	}

	// signal number 는 os마다 다르다. shell command 'kill -l' 로 확인하자
	int nSigno;
	for (nSigno = SIGHUP; nSigno <= 31; nSigno++)
	{
		if (nSigno == SIGKILL || nSigno == SIGSTOP) /// kill (cannot be caught )
			continue;
		if (nSigno == SIGBUS) // core dump 뜨고 싶을 때 사용
			continue;

		if ((sigaction(nSigno, &act, NULL) == -1))
		{
			conpt("sigaction(%d) (Pid:%d) (Er:%d,%s)", nSigno, getpid(), errno, (char*)strerror(errno));
			return -1;
		}
#ifdef DEBUG
		else if (debug_sig_message)
		{
			conpt("sigaction(%d) (Pid:%d) success", nSigno, getpid());
		}
#endif
	}
#endif

	return 0;
}

int get_strftime(char* time_buffer, size_t len)
{
	time_t	now;
	struct	tm tm_s;

	time(&now);
	localtime_r(&now, &tm_s);

	return snprintf(time_buffer, len, "%04d-%02d-%02d %02d:%02d:%02d"
		, tm_s.tm_year + 1900
		, tm_s.tm_mon + 1
		, tm_s.tm_mday
		, tm_s.tm_hour
		, tm_s.tm_min
		, tm_s.tm_sec
	);
}

/* Callback for handling data received from the server */
static int
websocket_client_data_handler(struct mg_connection *conn,
                              int flags,
                              char *data,
                              size_t data_len,
                              void *user_data)
{
	// default value of data buffer size is 16KB
	UserData* udata = (UserData*)user_data;
	udata->recv_last = time(NULL);

	/* We may get some different message types (websocket opcodes).
	 * We will handle these messages differently. */
	int is_text = ((flags & 0xf) == MG_WEBSOCKET_OPCODE_TEXT);
	int is_bin = ((flags & 0xf) == MG_WEBSOCKET_OPCODE_BINARY);
	int is_ping = ((flags & 0xf) == MG_WEBSOCKET_OPCODE_PING);
	int is_pong = ((flags & 0xf) == MG_WEBSOCKET_OPCODE_PONG);
	int is_close = ((flags & 0xf) == MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE);

	/* Log output: We got some data */

	if (!is_text) {
		conft("Client received %lu bytes of %s data from server"
			, (long unsigned)data_len
			, msgtypename(flags)
			);
	}

	/* Check if we got a websocket PING request */
	if (is_ping) {
		/* PING requests are to check if the connection is broken.
		 * They should be replied with a PONG with the same data.
		 */
		mg_websocket_client_write(conn,
		                          MG_WEBSOCKET_OPCODE_PONG,
		                          data,
		                          data_len);
		return 1;
	}

	/* Check if we got a websocket PONG message */
	if (is_pong) {
		/* A PONG message may be a response to our PING, but
		 * it is also allowed to send unsolicited PONG messages
		 * send by the server to check some lower level TCP
		 * connections. Just ignore all kinds of PONGs. */
		return 1;
	}

	/* It we got a websocket TEXT message, handle it ... */
	if (is_text) {
		// struct tmsg_list_elem *p;
		if ( ++udata->count >= TERM_CHECK || (memcmp(data, "{\"code\":201", 11))) {
			data[data_len] = '\0';
			conft("Client received %lu bytes of %s data from server: %s"
				, (long unsigned)data_len
				, msgtypename(flags)
				, data
				);

			if (udata->count >= TERM_CHECK) {
				udata->count = 0;
			}
			if (!memcmp(data, "{\"code\":409", 11)) {
				// 다음 관리자 프로그램이 연결되어 있는 상태다 종료하자
				if (g_sd > 0)
					close(g_sd);
				conft("processmon will stop now.");
				exit(0);
			}
		}
	}

	/* Another option would be BINARY data. */
	if (is_bin) {
		/* In this example, we just ignore binary data.
		 * According to some blogs, discriminating TEXT and
		 * BINARY may be some remains from earlier drafts
		 * of the WebSocket protocol.
		 * Anyway, a real application will usually use
		 * either TEXT or BINARY. */
	}

	/* It could be a CLOSE message as well. */
	if (is_close) {
		conft("Goodbye");
		return 0;
	}

	/* Return 1 to keep the connection open */
	return 1;
}

void startup()
{
	conft("DTMF Server start now....(%d)", g_sd);
	conft("processmon will restart now...");

	if (g_sd > 0) {
		close(g_sd);
		g_sd = -1;
	}

	char cmd[512] = { 0 };
	char* cwd = getenv("OPBIN");
	if (!cwd)
		cwd = ".";
	snprintf(cmd, sizeof(cmd) - 1, "%s/dtmf start &", cwd);
	system(cmd);
	// 잘못되면 이거 무한 루프 돌 수 있다. 막아라
	// crontab에 등록되어 있으면 1분 위에 다시 돈다
	// snprintf(cmd, sizeof(cmd) - 1, "%s/procmon start &", cwd);
	// system(cmd);
}

/* Callback for handling a close message received from the server */
static void
websocket_client_close_handler(const struct mg_connection *conn,
                               void *user_data)
{
	conft("Close handler");

	int nStop = 1;

	if (user_data) {
		nStop = ((UserData*)user_data)->nStop;
		free(user_data);
		user_data = NULL;
	}

	if (!nStop) {
		// DTMF Server start
		startup();
		exit(0);
		// pthread_exit(NULL);
	}
}

/* Websocket client test function */
void
run_websocket_client(const char *host,
                     int port,
                     int secure,
                     const char *path,
                     const char *greetings)
{
	char err_buf[100] = {0};
	struct mg_connection *client_conn;

	/* Log first action (time = 0.0) */
	conft("Connecting to %s:%i", host, port);
	
	UserData* udata = malloc(512);
	bzero(udata, 512);
	udata->size = 512;
	udata->startup = time(NULL);

	/* Connect to the given WS or WSS (WS secure) server */
	client_conn = mg_connect_websocket_client(host,
	                                          port,
	                                          secure,
	                                          err_buf,
	                                          sizeof(err_buf),
	                                          path,
	                                          NULL,
	                                          websocket_client_data_handler,
	                                          websocket_client_close_handler,
                                              udata //NULL
                                              );

	/* Check if connection is possible */
	if (client_conn == NULL) {
		conft("mg_connect_websocket_client error: %s", err_buf);
		// DTMF Server start
		startup();
		return;
	}

	/* Does the server play "ping pong" ? */
	while (g_Run) {
		udata->send_last = time(NULL);
		mg_websocket_client_write(client_conn,
			MG_WEBSOCKET_OPCODE_TEXT,
			greetings,
			strlen(greetings));
		int nTry;
		for(nTry=10;g_Run&&nTry;nTry--)
			usleep(500000);
	}


	/* Somewhat boring conversation, isn't it?
	 * Tell the server we have to leave. */
	conft("Sending close message");

	udata->nStop = 1;

	mg_websocket_client_write(client_conn,
	                          MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE,
	                          NULL,
	                          0);

	/* We don't need to wait, this is just to have the log timestamp
	 * a second later, and to not log from the handlers and from
	 * here the same time (printf to stdout is not thread-safe, but
	 * adding flock or mutex or an explicit logging function makes
	 * this example unnecessarily complex). */
	sleep(5);

	/* Close client connection */
	mg_close_connection(client_conn);


}

void sig_handler(int signo, siginfo_t* info, /*ucontext_t*/void* ucp)
{
	sigset_t sigset, oldset;

	conft("----- recv signal:%d, my_pid=%d, si_pid=%d", signo, getpid(), info->si_pid);


	sigfillset(&sigset);

	// 시그널 블럭
	// 핸들러가 수행되는 동안 수신되는 모든 시그널에 대해서 블럭한다.
	// 시그널 처리 중에 다른 시그널 처리가 실행되면 예상치 못한 문제 발생
	if (sigprocmask(SIG_BLOCK, &sigset, &oldset) < 0)
	{
		conft("sigprocmask %d error", signo);
	} else {

		if (signo == SIGINT) // kill -2
		{
			g_Run = 0;
		}
		else if (signo == SIGSEGV)
		{
			conft("Caught SIGSEGV : si_pid->%d, si_code->%d, addr->%p", info->si_pid, info->si_code, info->si_addr);
			signal(signo, SIG_IGN);
		}

		// 모든 시그널 언블럭
		if (sigprocmask(SIG_UNBLOCK, &sigset, &oldset) < 0)
		{
			conft("sigprocmask %d error", signo);
		}
	}
}

int	tcp_connect(const char* host, int port_no, struct sockaddr_in* paddr)
{
	int		fd;
	struct	sockaddr_in addr_in;
	struct	hostent* hp;

	bzero((char*)&addr_in, sizeof(addr_in));

	addr_in.sin_family = AF_INET;

	if ((hp = gethostbyname(host)) == NULL)
	{
		if (atoi(&(host[0])) > 0)
			addr_in.sin_addr.s_addr = inet_addr(host);
		else
			return(-1);
	}
	else
	{
		addr_in.sin_addr.s_addr = ((struct in_addr*)(hp->h_addr))->s_addr;
	}

	addr_in.sin_port = htons((unsigned short)port_no);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return(-3);

	if (connect(fd, (struct sockaddr*)&addr_in, sizeof(addr_in)) < 0)
	{
		close(fd);
		return(-4);
	}

	if (paddr)
		memcpy(paddr, &addr_in, sizeof(addr_in));

	int optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));

	//struct linger optLinger={1,0};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 버려 버린다(바로리턴)
	//struct linger optLinger={1,5};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(송신완료까지블럭, 5초만 블럭인 시스템도 있고)
	struct linger optLinger = { 0,0 };//linger on! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(바로리턴,백그라운드처리로time_wait)
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&optLinger, sizeof(optLinger));

	return(fd);
}

void run_http_client(const char* host, int port)
{
	// amidtmf2로 http연결하여 보안처리된 id & check를 주고 받는다.....
	struct sockaddr_in server;
	time_t now;
	time(&now);
	srand(now);

	// 연결을 한다
	int dtmf_sd = tcp_connect(host, port, &server);
	if (dtmf_sd < 1) {
		startup();
		return;
	}

	struct timeval stTimeVal;
	fd_set rset;
	uint32_t checked_len, read_len;
	char id[12], check[12];
	char buf[4096];
	int rc;
	// REQUEST_INFO req;
	RESPONSE_INFO res;

	int nStep = 1;
	int log_count = 0;
	int log_display = atoi(getenv("LOG_DISPLAY"));
	if (log_display < 10)
		log_display = 10;

	while (g_Run) {
		FD_ZERO(&rset);
		FD_SET(dtmf_sd, &rset);

		// 5초간 입력을 기다린다
		stTimeVal.tv_sec = 5;
		stTimeVal.tv_usec = 0;
		rc = select(dtmf_sd + 1, &rset, 0, 0, &stTimeVal);
		if (!g_Run)	// 5초를 기다리므로 다시 검증해야 함.
			break;
		if (!rc) {
			// conpt("ami 연결시간 5초 만료!!!!\n");
			// rand() 로 임의 값을 생성하고 이를 check에 맞춘다
			if (nStep == 1) {
				// /keepalive 메시지를 보낸다
				bzero(id, sizeof(id));
				bzero(check, sizeof(check));
				int sep, length;
				length = rand() % 3 + 5;
				for (rc = 0; rc < length; rc++) {
					sep = rand() % 3;
					id[rc] = sep==0 ? rand() % 26 + 'a' // 'a~z'
						: sep == 1 ? rand() % 10 + '0' // 'a~z'
						: rand() % 26 + 'A'; // 'A~Z'
				}
				length = rand() % 3 + 5;
				for (rc = 0; rc < length; rc++) {
					sep = rand() % 3;
					check[rc] = sep == 0 ? rand() % 26 + 'a' // 'a~z'
						: sep == 1 ? rand() % 26 + 'a' // 'a~z'
						: rand() % 26 + 'A'; // 'A~Z'
				}
				check[4] = id[1];
				check[0] = id[3];

				checked_len = sprintf(buf,
					"GET /keepalive?id=%s&check=%s HTTP/1.0\r\n"
					"Host: %s:%d\r\n"
					"Connection: keep-alive\r\n\r\n"
					, id, check, host, port
				);
				write(dtmf_sd, buf, checked_len);
				nStep = 2;
				if (++log_count > log_display) {
					printf("check %d times request:\n%s", log_display, buf);
					fflush(stdout);
				}
				continue;
			} else {
				printf("--- processmon socket recv wait time over....\n");
				startup();
				return;
			}
		} else if (rc < 0) {
			// conpt("ami 연결된 소켓 오류\n");
			startup();
			return;
		} else {
			nStep = 1;
			ioctl(dtmf_sd, SIOCINQ, &checked_len);
			if (!checked_len) {
				printf("socket disconnected....\n");
				startup();
				return;
			}
			read_len = 0;
			rc = 10;
			while 
				(rc > 0
				&& (read_len < 4 
					|| buf[read_len - 4] != '\r'
					|| buf[read_len - 3] != '\n'
					|| buf[read_len - 2] != '\r'
					|| buf[read_len - 1] != '\n'
					)
				){
				ioctl(dtmf_sd, SIOCINQ, &checked_len);
				if (checked_len) {
					read_len += read(dtmf_sd, buf + read_len, 1);
				} else {
					usleep(100000);
					rc--;
				}
			}
			if (read_len < 4
				|| buf[read_len - 4] != '\r'
				|| buf[read_len - 3] != '\n'
				|| buf[read_len - 2] != '\r'
				|| buf[read_len - 1] != '\n') {
				startup();
				return;
			}
			// 응답코드를 확인한다. 
			bzero(&res, sizeof(res));
			// parse
			buf[read_len] = '\0';
			char* src=buf;
			char* ptr;
			if (memcmp(src, "HTTP/", 5) || (memcmp(src + 5, "1.0", 3) && memcmp(src + 5, "1.1", 3))) {
				startup();
				return;
			}
			res.http_version = src + 5;
			ptr = src + 8;
			*ptr = '\0';
			src = ++ptr;

			// http status code
			ptr = strchr(src, ' ');
			if (ptr) *ptr = '\0';
			res.response = src;
			res.msg = ++ptr;
			src = ptr;
			ptr = strchr(src, '\r');
			if (ptr) {
				*ptr++ = '\0';
				src = ptr;
			}
			ptr = strchr(src, '\n');
			if (ptr) {
				*ptr++ = '\0';
			} else {
				// protocol error
				startup();
				return;
			}

			// header parse
			while (*ptr) {
				while (*ptr && *++ptr != '\r');
				if (*ptr == '\r') {
					*ptr++ = '\0';
					*ptr++ = '\0';
					res.http_headers[res.num_headers].name = src;
					while (*src && *++src != ':');
					if (*src == ':') {
						*src++ = '\0';
						*src++ = '\0';
						res.http_headers[res.num_headers++].value = src;
					}
					src = ptr;
				}
			}

			if (log_count > log_display) {
				log_count = 0;
				printf("check %d times response:\nHTTP/%s %s %s\n", log_display, res.http_version, res.response, res.msg);
				for (rc = 0; rc < res.num_headers; rc++) {
					printf("%s: %s\n", res.http_headers[rc].name, res.http_headers[rc].value);
				}
				fflush(stdout);
			}
			// 나머지 버퍼는 비운다... 혹시 쓰레기 청소.
			do {
				ioctl(dtmf_sd, SIOCINQ, &checked_len);
				if(checked_len)
					read(dtmf_sd, buf, checked_len);
			} while (checked_len);
		}
	}
}

/* main will initialize the CivetWeb library
 * and start the WebSocket client test function */
int main(int argc, char *argv[])
{
	if (argc == 2 && !memcmp(argv[1], "-v",2)) {
		printf("\nprocessmon version 1.0\n\n");
		return 0;
	}

	// const char *greetings = "Hello World!";
	const char *greetings = "user=callgate&pw=vai&cmd=live";

	const char *host = "127.0.0.1";
	const char *path = "/ws";

	int port_no = 4061;
	char* logfile = "/var/log/dtmf/processmon.log";
	if (argc > 1) {
		port_no = atoi(argv[1]);
		if (argc > 2) {
			logfile = argv[2];
		}
	}

	con_logfile(logfile);

	// nPort 가 0 이면 중복실행금지 처리 없음
	if (port_no) {
		struct sockaddr_in	addr_in;
		int optval;

		memset(&addr_in, 0x00, sizeof(struct sockaddr_in));

		addr_in.sin_family = AF_INET;
		addr_in.sin_addr.s_addr = INADDR_ANY;
		addr_in.sin_port = htons((unsigned short int)port_no);

		if ((g_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			syslog(LOG_INFO, "processmon --- server socket create error port=%d, errno=%d", port_no, errno);
			conft("pid=%d, socket create error port = %d, errno = %d", getpid(), port_no, errno);
			return -1;
		}

		optval = 1;
		setsockopt(g_sd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

		if (bind(g_sd, (struct sockaddr*)(&addr_in), sizeof(struct sockaddr)) < 0)
		{
			if (errno == EINVAL)// EINVAL -> 이미 바인드 성공했음...
			{
				conft("pid=%d, socket bind ok... use port(%d)", getpid(), port_no);
				// OK
			}
			else if (errno == EADDRINUSE)
			{
				syslog(LOG_INFO, "processmon --- pid=%d, socket bind error. already used port(%d)", getpid(), port_no);
				conft("pid=%d, socket bind error. already used port(%d)", getpid(), port_no);
				close(g_sd);
				return -2;
			}
			else
			{
				syslog(LOG_INFO, "processmon --- pid=%d, socket bind error. port=%d, errno=%d", getpid(), port_no, errno);
				conft("pid=%d, socket bind error. port=%d, errno=%d", getpid(), port_no, errno);
				close(g_sd);
				return 3;
			}
		}
		if (listen(g_sd, 100) == -1)
		{
			conft("pid=%d, socket listen error. port=%d, errno=%d", getpid(), port_no, errno);
			close(g_sd);
			return -9;
		}
	}

	conft("%s, pid=%d, socket create sd = %d", argv[0], getpid(), g_sd);

#if defined(NO_SSL)
	const int port = 4060;
	const int secure = 0;
	mg_init_library(0);
#else
	const int port = 443;
	const int secure = 1;
	mg_init_library(MG_FEATURES_SSL);
#endif

	if (signal_init(&sig_handler, 1) < 0)
		return -1;

	char* usews = getenv("USEWS");
	if (usews && !strcmp(usews, "YES")) {
		run_websocket_client(host, port, secure, path, greetings);
	} else {
		run_http_client(host, port);
	}
	if(g_sd > 0)
		close(g_sd);

	printf("%s --- terminated...\n", argv[0]);

	return 0;
}
