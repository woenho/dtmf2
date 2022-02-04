#include "amiaction.h"
#include "http.h"
#include "websocket.h"
#include "processevents.h"

extern char dtmfCompileDate[20];

char cfg_path[512] = { 0 };

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
		server.m_main_run = false;
	}
	else if (signo == SIGUSR2) // kill -12 어떤 시스템은 31, 시스템마다 다름
	{
		PTHREADINFO pThread = atp_getThreadInfo();
		int nCount = atp_getThreadCount();
		int nQueueRealtime = atp_getRealtimeQueueCount();
		int nQueueNormal = atp_getNormalQueueCount();
		int nIndx;

		conft("atp compile date: %s", atpCompileDate);
		conft("tst compile date: %s", tstCompileDate);
		conft("dtmf compile date: %s", dtmfCompileDate);

		conft("atp thread Realtime queue count=%d", nQueueRealtime);
		conft("atp thread Normal queue count=%d", nQueueNormal);

		for (nIndx = 0; nIndx < nCount; nIndx++) {
			conft("atp threadno=%d, realtime execute=%lu average elapsed=%.3f, normal execute=%lu average elapsed=%.3f"
				, pThread[nIndx].nThreadNo
				, pThread[nIndx].nRealtimeCount
				, atp_getAverageRealtimeWorkingtime(nIndx) / 1e+3	// 평균 작업소요 시간(mili)
				, pThread[nIndx].nNormalCount
				, atp_getAverageNormalWorkingtime(nIndx) / 1e+3		// 평균 작업소요 시간(mili)
			);
		}
		// tstpoll
		for (nIndx = 0; nIndx < (int)server.m_thread_count; nIndx++) {
			conft("tst threadno=%d, execute=%lu average elapsed=%.3f, most_elapsed=%.3f"
				, server.m_workers[nIndx].thread_no
				, server.m_workers[nIndx].exec_count
				, server.m_workers[nIndx].get_averageElapsedtime() / 1e+3
				, server.m_workers[nIndx].most_elapsed / 1e+9
			);
		}
		CWebConfig cfg;
		if (cfg.Open(cfg_path) > 0) {
			log_event_level = atoi(cfg.Get("LOG","ev_level").c_str());
		}
		conft("log_event_level=%d", log_event_level);
	}
	else if (signo == SIGUSR1) // kill -10 어떤 시스템은 30, 시스템마다 다름
	{
		PTHREADINFO pThread = atp_getThreadInfo();
		int nCount = atp_getThreadCount();
		int nQueueRealtime = atp_getRealtimeQueueCount();
		int nQueueNormal = atp_getNormalQueueCount();
		int nIndx;

		conft("atp compile date: %s", atpCompileDate);
		conft("tst compile date: %s", tstCompileDate);
		conft("dtmf compile date: %s", dtmfCompileDate);

		conft("atp thread Realtime queue count=%d", nQueueRealtime);
		conft("atp thread Normal queue count=%d", nQueueNormal);

		for (nIndx = 0; nIndx < nCount; nIndx++) {
			conft("atp threadno=%d, realtime execute=%lu average elapsed=%.3f, normal execute=%lu average elapsed=%.3f"
				, pThread[nIndx].nThreadNo
				, pThread[nIndx].nRealtimeCount
				, atp_getAverageRealtimeWorkingtime(nIndx) / 1e+3	// 평균 작업소요 시간(mili)
				, pThread[nIndx].nNormalCount
				, atp_getAverageNormalWorkingtime(nIndx) / 1e+3		// 평균 작업소요 시간(mili)
			);
			pThread[nIndx].nRealtimeCount = pThread[nIndx].nNormalCount = 0;	// excute count reset
		}

		// tstpoll
		for (nIndx = 0; nIndx < (int)server.m_thread_count; nIndx++) {
			conft("tst threadno=%d, execute=%lu average elapsed=%.3f, most_elapsed=%.3f"
				, server.m_workers[nIndx].thread_no
				, server.m_workers[nIndx].exec_count
				, server.m_workers[nIndx].get_averageElapsedtime() / 1e+3
				, server.m_workers[nIndx].most_elapsed / 1e+9
			);
			server.m_workers[nIndx].exec_count = server.m_workers[nIndx].idle_count = 0;
			server.m_workers[nIndx].sum_time = server.m_workers[nIndx].most_elapsed = 0;
		}

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

TST_STAT dtmf2_disconnected(PTST_SOCKET psocket) {
	if (!psocket)
		return tst_suspend;

	if (psocket == ami_socket) {
		// if (psocket->type == sock_client && psocket->user_data->type == ami_base) {
			conpt("--- 흐미 ami 끊어졌다.....");
			ami_socket = NULL;
		// }
	} else {
#ifdef DEBUG
		conpt("--- disconnected client socket....%s(%s:%d)", __func__, inet_ntoa(psocket->client.sin_addr), ntohs(psocket->client.sin_port));
#endif
		if (psocket->type == sock_websocket) {
			TRACE("--ws- websocket session이 해제되었다.\n");
			
			// websocket 해제 시 keepalive 중단 처리필요함



		}
	}

	return tst_suspend;
}

int main(int argc, char* argv[])
{
	if (argc == 2 && !strcmp(argv[1], "-v")) {
		printf("\namidtmf version 2.0\n\n");
		printf("atp compile date: %s\n", atpCompileDate);
		printf("tst compile date: %s\n", tstCompileDate);
		printf("dtmf compile date: %s\n", dtmfCompileDate);
		return 0;
	}

	getcwd(cfg_path, sizeof(cfg_path));
#if defined(DEBUGTRACE)
	strcat(cfg_path, "/amidtmf_t.conf");
#elif defined(DEBUG)
	strcat(cfg_path, "/amidtmf_d.conf");
#else
	strcat(cfg_path, "/amidtmf.conf");
#endif
	printf("config file path:%s:\n", cfg_path);
	if (g_cfg.Open(cfg_path) < 0) {
		char* errmsg = strerror(errno);
		fprintf(stderr, "\n-------------------\n%s open failed\n%s\n", cfg_path, errmsg);
		fflush(stderr);
		return -1;
	}

	char* logmethod = strdup(g_cfg.Get("LOG", "method", "logfile").c_str());
	printf("log method=%s\n", logmethod ? logmethod : "error");

	if (logmethod && !strcmp(logmethod, "logfile")) {
		con_logfile(g_cfg.Get("LOG", "logfile", "./amidtmf.log").c_str());
	}
	else if (logmethod && !strcmp(logmethod, "server")) {
		con_logudp(g_cfg.Get("LOG", "host", "127.0.0.1").c_str(), atoi(g_cfg.Get("LOG", "port", "26000").c_str()));
	}
	else {
		fprintf(stderr, "\n-------------------\nLOG method setting error in cofig file(%s)\n", cfg_path);
		if (logmethod) free(logmethod);
		return -1;
	}
	if (logmethod) free(logmethod);

#if defined(DEBUGTRACE)
	conft("\n------------------------\n trace debuging info DTMF server start. pid(%d)\n========================================", getpid());
#elif defined(DEBUG)
	conft("\n------------------------\n possible debuging DTMF server start. pid(%d)\n========================================", getpid());
#else
	conft("\n------------------------\n optimization DTMF server start. pid(%d)\n========================================", getpid());
#endif
	conft("atp compile date: %s", atpCompileDate);
	conft("tst compile date: %s", tstCompileDate);
	conft("dtmf compile date: %s", dtmfCompileDate);

	if (signal_init(&sig_handler, true) < 0)
		return -1;

	log_event_level = atoi(g_cfg.Get("LOG", "ev_level").c_str());
	conft("log_event_level=%d", log_event_level);




	// first는 test용
	//server.setEventNewConnected(first);

	server.setEventDisonnected(my_disconnected);

	if (server.create(atoi(g_cfg.Get("HTTP", "thread", "5").c_str())
					, "0.0.0.0"
					, atoi(g_cfg.Get("HTTP", "port", "4060").c_str())
					, http
					, 4096
					, 4096) < 1) {
		conft("---쓰레드풀이 기동 되지 못했다....");
		return 0;
	}

	// 연결해제시 호출로 반드시 설정해야한다
	// ami socket 연결해제시에 전역변수인 ami_socket 초기화가 필요하다
	server.m_fdisconnected = dtmf2_disconnected;
	// -------------------------------------------------------------------------------------

	atp_create(atoi(g_cfg.Get("THREAD", "count", "5").c_str()), atpfunc);

	// 처리할 이벤트를 등록 한다....

	g_process.clear();
	g_process["UserEvent"] = (void*)event_userevent;
	g_process["DialEnd"] = (void*)event_dialend;

	map<const char*, void*>::iterator it;
	for (it = g_process.begin(); it != g_process.end(); it++) {
		conft(":%s: event func address -> %lX\n", it->first, ADDRESS(it->second));
	}

	g_route.clear();
	g_route["/dtmf"] = (void*)http_dtmf;
	g_route["/transfer"] = (void*)http_transfer;
	g_route["/keepalive"] = (void*)http_alive;
	for (it = g_route.begin(); it != g_route.end(); it++) {
		conft(":%s: http route func address -> %lX\n", it->first, ADDRESS(it->second));
	}

	g_websocket.clear();
	g_websocket["/alive"] = (void*)websocket_alive;
	for (it = g_route.begin(); it != g_route.end(); it++) {
		conft(":%s: websocket route func address -> %lX\n", it->first, ADDRESS(it->second));
	}

	// -------------------------------------------------------------------------------------

#if 0
	int i = 300;
	while (--i && server.m_main_run) {
#else
	while (server.m_main_run) {
#endif
		if (!ami_socket) {
			// 연결을 시도하는 작업 요청
			PATP_DATA atpdata = atp_alloc(sizeof(AMI_LOGIN));
			AMI_LOGIN& login = *(PAMI_LOGIN)&atpdata->s;
			strncpy(login.Host, g_cfg.Get("AMI", "host","127.0.0.1").c_str(), sizeof(login.Host));
			login.Port = atoi(g_cfg.Get("AMI", "port", "5038").c_str());
			strncpy(login.Username, g_cfg.Get("AMI", "user", "call").c_str(), sizeof(login.Username));
			strncpy(login.Secret, g_cfg.Get("AMI", "secret", "call").c_str(), sizeof(login.Secret));
			atpdata->func = amiLogin;
			atp_addQueue(atpdata);
			conft("REQUEST ami login...\n");
		}
		sleep(3);
	}



	conft("---쓰레드풀을 종료합니다.....\n");

	server.destroy();

	atp_destroy(gracefully);

	return 0;
}

