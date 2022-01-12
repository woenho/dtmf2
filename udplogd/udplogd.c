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
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>

#if !defined(DEBIAN)
#include <bits/siginfo.h>
#endif

#define bool int

sigset_t g_sigsBlock;	///< 시그널 구조
sigset_t g_sigsOld;		///< 시그널 구조

#include "logger.h"

int g_run;

int	udp_create(int port_no)
{
	struct	sockaddr_in	addr_in;
	int		fd;

	memset(&addr_in, 0x00, sizeof(addr_in));

	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = INADDR_ANY;
	addr_in.sin_port = htons((unsigned short)port_no);

	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		conpt("socket create error port = %d, errno = %d", port_no, errno);
		return -1;
	}

	if (bind(fd, (struct sockaddr*)(&addr_in), sizeof(struct sockaddr)) < 0)
	{
		if (errno == EADDRINUSE)
		{
			conpt("bind error. 사용중인 port(%d)임", port_no);
			close(fd);
			return -1;
		}
		else
		{
			conpt("bind error. port=%d, errno=%d", port_no, errno);
			close(fd);
			return -2;
		}
	}

	return fd;

}


int signal_init(void (*sa)(int, siginfo_t*, void*), bool debug_sig_message)
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
		conpt("Signal Set (Pid:%d) (Er:%d,%s)", getpid(), errno, (char*)strerror(errno));
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
		conpt("Sigaction (Pid:%d) (Er:%d,%s)", getpid(), errno, (char*)strerror(errno));
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

void sig_handler(int signo, siginfo_t* info, /*ucontext_t*/void* ucp)
{
	sigset_t sigset, oldset;

	// 핸들러가 수행되는 동안 수신되는 모든 시그널에 대해서 블럭한다.
	// 시그널 처리 중에 다른 시그널 처리가 실행되면 예상치 못한 문제 발생

	sigfillset(&sigset);
	// 시그널 블럭
	if (sigprocmask(SIG_BLOCK, &sigset, &oldset) < 0)
	{
		conpt("sigprocmask %d error", signo);
	}

	conpt("----- recv signal:%d, my_pid=%d, si_pid=%d", signo, getpid(), info->si_pid);

	if (signo == SIGINT) // kill -2
	{
		g_run = 0;
	}
	else if (signo == SIGSEGV)
	{
		//signal(signo, SIG_ERR);
		conpt("Caught SIGSEGV : si_pid->%d, si_code->%d, addr->%p", info->si_pid, info->si_code, info->si_addr);

		fprintf(stderr, "Fault address: %p\n", info->si_addr);
		switch (info->si_code)
		{
		case SEGV_MAPERR:
			fprintf(stderr, "Address not mapped.\n");
			break;

		case SEGV_ACCERR:
			fprintf(stderr, "Access to this address is not allowed.\n");
			break;

		default:
			fprintf(stderr, "Unknown reason.\n");
			break;
		}


		signal(signo, SIG_IGN);
	}

	// 시그널 언블럭
	if (sigprocmask(SIG_UNBLOCK, &sigset, &oldset) < 0)
	{
		conpt("sigprocmask %d error", signo);
	}

}

int main(int argc, char* argv[]) 
{
	int logport = 26000;
	char buf[4096];
	char* ptr;
	struct in_addr valid_ip = { 0 };
	char ipaddress[INET_ADDRSTRLEN];
	char logfile[128];


	if (argc > 1) {
		strncpy(ipaddress, argv[1], sizeof(ipaddress) - 1);
		inet_pton(AF_INET, ipaddress, &valid_ip);
	}

	if (argc > 2) {
		logport = atoi(argv[2]);
	}

	if (argc > 3) {
		strncpy(logfile,argv[3],sizeof(logfile)-1);
	}
	else {
		snprintf(logfile,sizeof(logfile),"/var/log/udplogd/udplogd_%d.log", logport);
	}


	log_logfile(logfile);

	ptr = strrchr(logfile, '/');

	if (ptr) {
		*ptr = '\0';
		sprintf(buf, "mkdir -p %s", logfile);
		system(buf);
		*ptr = '/';
	} else {
		// 폴더 지정이 없다면 현재 디렉토리에 로그파일을 생성한다....
	}

	logpt("log daemon startup... port(%d), path:%s", logport, logfile);

	if (signal_init(&sig_handler, 1) < 0)
		return -1;

	int socketServer = udp_create(logport);

	if (socketServer < 1)
	{
		logpt("Can't open udp server port:%d", logport);
		return 0;
	}


	g_run = 1;

	fd_set ready;
	struct timeval	waittime;
	struct sockaddr_in udpClientAddr;
	int nSoketAddrSize = sizeof(udpClientAddr);

	while (g_run)
	{
		FD_ZERO(&ready);
		FD_SET(socketServer, &ready);
		waittime.tv_sec = 0;
		waittime.tv_usec = 500 * 1000; /// 500*1000 -> 0.5 sec

		int rc = select(socketServer + 1, &ready, (fd_set*)0, (fd_set*)0, &waittime);

		if (rc == 0)
		{
			// 0.5초동안 메시지가 안들어오면 로그파일 닫기...
			if (logfp) {
				fclose(logfp);
				logfp = NULL;
			}
			continue;	// time out
		}

		if (rc < 0)
		{
			if (logfp) {
				fclose(logfp);
				logfp = NULL;
			}

			if (errno != EINTR) // system intrupt event,,,, retry
			{
				logpt("select() failed");
				close(socketServer);
				usleep(100000);
				socketServer = udp_create(logport);
			}
			continue;
		}

		ssize_t nLen = recvfrom(socketServer, buf, sizeof(buf)-1, 0, (struct sockaddr*)&udpClientAddr, (socklen_t*)&nSoketAddrSize);

		buf[nLen] = '\0';

#if defined(DEBUG)
		inet_ntop(AF_INET, &(udpClientAddr.sin_addr), ipaddress, INET_ADDRSTRLEN);
		printf("recv:%s:%d, mode=%d:%s:\n", ipaddress, ntohs(udpClientAddr.sin_port), *buf, buf+1);
#endif
		if (valid_ip.s_addr && valid_ip.s_addr != udpClientAddr.sin_addr.s_addr) {
			// 로그 발신지 검증 상태 인데 로그발신지 IP가 다르면 pass
			continue;
		}

		log_mode section = *buf;

		if (section == LOGMODE_CONF)
			logf(buf + 1);
		else if (section == LOGMODE_CONFN)
			logfn(buf + 1);
		else if (section == LOGMODE_CONFT)
			logft(buf + 1);
		else if (section == LOGMODE_CONFTN)
			logftn(buf + 1);
		else if (section == LOGMODE_CONP)
			logp(buf + 1);
		else if (section == LOGMODE_CONPT)
			logpt(buf + 1);
		else if (section == LOGMODE_CONPN)
			logpn(buf + 1);
		else if (section == LOGMODE_CONPTN)
			logptn(buf + 1);
		else
			logp(buf);
	}

	close(socketServer);

	logpt("pg:%s, pid:%d is endding\n\n", argv[0], getpid());

	return 0;
}


