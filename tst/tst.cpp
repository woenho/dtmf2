/*
* programming. woenho@daum.net
* on. 2021-12-23
* for. anybody
* using. free
* pay. comment to me
*/

#include "tst.h"
#include<syslog.h>

using namespace std;
using namespace tst;

tstpool::tstpool()
{
	bzero(this, sizeof(*this));
	m_waittime.tv_sec = 3;
	// 요거 클리어 먼저 안하면 인서트시 세그먼트 오류난다... stl::map 뻑...
	m_connect.clear();
}

tstpool::~tstpool()
{
	destroy();
}


int	tstpool::tcp_create(const char* ip, u_short port_no)
{
	struct	sockaddr_in	addr_in;
	int		optval;
	int		fd;
	int		retry;

	fprintf(stdout, "tstpool compile date : %s", tstCompileDate);
	fflush(stdout);
	syslog(LOG_INFO, "tstpool compile date : %s", tstCompileDate);

#define MAX_RETRY		(10)

	memset(&addr_in, 0x00, sizeof(addr_in));

	addr_in.sin_family = AF_INET;
	if( !ip || !*ip)
		addr_in.sin_addr.s_addr = INADDR_ANY;
	else 
		addr_in.sin_addr.s_addr = inet_addr(ip);
	addr_in.sin_port = htons(port_no);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		fprintf(stderr, "server socket create error port = %d, errno=%d\n", port_no, errno);
		fflush(stderr);
		syslog(LOG_INFO, "tstpool --- server socket create error port=%d, errno=%d", port_no, errno);
		return -1;
	}

	optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

	for (retry = 0; retry < MAX_RETRY; retry++)
	{
		if (bind(fd, (struct sockaddr*)(&addr_in), sizeof(struct sockaddr)) < 0)
		{
			if (errno == EINVAL)// EINVAL -> 이미 바인드 성공했음...
			{
				// OK
				break;
			}
			else if (errno == EADDRINUSE)
			{
				syslog(LOG_INFO, "tstpool bind error. 사용중인 port(%d)임, retry count(%d)", port_no, retry);
				fprintf(stderr, "tstpool bind error. 사용중인 port(%d)임, retry count(%d)\n", port_no, retry);
				optval = 1;
				setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
			}
			else
			{
				syslog(LOG_INFO, "tstpool bind error. ip=%s, port=%d, errno=%d", inet_ntoa(addr_in.sin_addr), port_no, errno);
				fprintf(stderr, "tstpool bind error. ip=%s, port=%d, errno=%d\n", inet_ntoa(addr_in.sin_addr), port_no, errno);
			}
			fflush(stderr);
			usleep(200000); // 0.2 sec
		}
		else
		{
			break;
		}
	}

	if (retry >= MAX_RETRY)
	{
		close(fd);
		return -2;
	}

	if (listen(fd, 100) == -1)
	{
		close(fd);
		return -9;
	}

	optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));

	//struct linger optLinger={1,0};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 버려 버린다(바로리턴)
	//struct linger optLinger={1,5};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(송신완료까지블럭, 5초만 블럭인 시스템도 있고)
	struct linger optLinger = { 0,0 };//linger on! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(바로리턴,백그라운드처리로time_wait)
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&optLinger, sizeof(optLinger));

	return fd;

}

int	tstpool::tcp_connect(const char* host, u_short port_no, struct	sockaddr_in* paddr)
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

	addr_in.sin_port = htons(port_no);

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

void* tst_main(void* param)
{
	tstpool* pool = (tstpool*)param;
	uint32_t i;
	int nCheckCount;
	struct timespec timenow;
	uint32_t seconds;
	uint64_t nanoseconds;
	map<int, PTST_SOCKET>::iterator it;
	int sd;
	struct sockaddr_in client;
	socklen_t socklen = sizeof(client);
	uint32_t events;
	struct epoll_event ev;	///< 이벤트 처리종류(read/wtire/error) 지정용 epoll구조
	PTST_SOCKET socket;
	int check_len, err = 0;
	TST_STAT next;

	// 메인소켓이 지정되었으면 epoll에 등록한다
	if (pool->m_sockmain > 0) {
		memset(&ev, 0x00, sizeof(ev));
#ifdef OLDEPOLL
		ev.events = EPOLLIN /*| EPOLLRDHUP*/ | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
#else
		ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
#endif
		ev.data.fd = pool->m_sockmain;


		if (epoll_ctl(pool->m_epfd, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0)
		{
			TRACE("[epoll_ctl(EPOLL_CTL_ADD)] server sd:%d, failed add!(%s)\n", ev.data.fd, strerror(errno));
			// 흐미 뭐지? 어케하노? 이런 일은 없을거라...이거 타면 linux 시스템 죽는 중이라는...
		}
		else
		{
			TRACE("[epoll_ctl(EPOLL_CTL_ADD)] server sd:%d, added\n", ev.data.fd);

			if (pool->m_socktype > sock_listen) {
				// 메인소켓이 클라이언트라면 연결하자마자 들어온 메시지가 있을 수 있다.
				// 이것은 epoll이 절대 트리거 해주지 않는다.
				// ioctl()을 호출하거나 한 바이트만 읽어도 그 순간 epoll에 등록된 EPOLLIN 이벤트가 발생한다.
				err = ioctl(pool->m_sockmain, SIOCINQ, &check_len);	// tcp수신버퍼에 도착한 바이트 수를 check_len에 넣어준다, 성공시 0 리턴
			}
		}
	} else {
		// 서버소켓이 생성되지 않았다면 만들어진 워크쓰레드를 모두 종료하고 메인쓰레드도 종료한다
		pool->m_main_run = 0;
		TRACE("tst ----------need main socket setting\n");
	}



	// 

	nCheckCount = 0;
	while (pool->m_main_run) {
		// 앞전에 가져온 모든 이벤트에 대한 작업의뢰가 완료하고 epoll 이벤트를 가지러 가자
		if (nCheckCount<1) {
			memset(pool->m_events, 0x00, pool->m_size_event);
			nCheckCount = epoll_wait(pool->m_epfd, pool->m_events, pool->m_thread_count, 100); // wait 100 mili seconds
		}

		if (nCheckCount<1)
			continue;

		events = pool->m_events[nCheckCount - 1].events;
		sd = pool->m_events[nCheckCount - 1].data.fd;
		// listen소켓 accept처리
		if ( sd == pool->m_sockmain) {
			if (pool->m_socktype == sock_listen) {
				if (events & EPOLLIN) {
					sd = accept(pool->m_sockmain, (sockaddr*)&client, &socklen);
					if (sd > 0) {
						// socket 구조체 생성
						socket = new TST_SOCKET(pool->m_recv_buf_size, pool->m_send_buf_size);
						socket->sd = sd;
						socket->type = sock_sub;
						memcpy(&socket->client, &client, sizeof(client));
						it = pool->m_connect.find(sd);
						if (it != pool->m_connect.end()) {
							// 헐?? 뭔가 가비지가 남았을까?
							pool->closesocket(sd);
						}

						pool->addsocket(socket);
						next = tst_suspend;
						if (pool->m_fconnected) {
							// 최초 연결부터 클라이언트에 뭐가 쓰고 싶다면 m_fconnected() 에서 버퍼에 설정만 해라.
							// 워크쓰레드가 보내도록 하자
							// 만일 특정 클라이언트에 대한 연결을 허가하지 않는다면 tst_disconnect 를 리턴해라
							next = pool->m_fconnected(socket);
						}

						if (next < tst_reconnected) {
							// epoll에 등록
							memset(&ev, 0x00, sizeof(ev));
							// 최초 연결부터 클라이언트에 뭐가 쓰고 싶다면 m_fconnected() 에서 버퍼에 설정만 해라. 뭐크쓰레드가 보내도록 하자
							ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
							if (next == tst_send)
								ev.events |= EPOLLOUT;
							ev.data.fd = sd;
							if (epoll_ctl(pool->m_epfd, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0)
							{
								TRACE("[epoll_ctl(EPOLL_CTL_ADD)] client sd:%d, failed add!(%s)\n", ev.data.fd, strerror(errno));
								// 흐미 뭐지? 어케하노? 이런 일은 없을거라...
#ifdef DEBUG
							} else {
								TRACE("[epoll_ctl(EPOLL_CTL_ADD)] client sd:%d, added\n", ev.data.fd);
#endif
							}
						} else {
							pool->closesocket(sd);
						}
					} else {
						syslog(LOG_INFO, "tstpool accept()error, errno=%d, msg:%s", errno, strerror(errno));
						nCheckCount--;
						continue;
					}
					// epoll에 서버소켓 수정 등록
					memset(&ev, 0x00, sizeof(ev));
					ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
					ev.data.fd = pool->m_sockmain;
					if (epoll_ctl(pool->m_epfd, EPOLL_CTL_MOD, ev.data.fd, &ev) < 0)
					{
						TRACE("[epoll_ctl(EPOLL_CTL_MOD)] server sd:%d, failed mod!(%s)\n", ev.data.fd, strerror(errno));
						// 흐미 뭐지? 어케하노? 이런 일은 없을거라...
#ifdef DEBUG
					} else {
						TRACE("[epoll_ctl(EPOLL_CTL_MOD)] server sd:%d, mod success, ready to next connect\n", ev.data.fd);
#endif
					}
				} else if (events & EPOLLHUP || events & EPOLLRDHUP || events & EPOLLERR) {
					// 서버소켓이 닫혔다? 뭐지???
					pool->m_main_run = 0;
				} else {
					// 뭐지?? 서버소켓은 connect (EPOLLIN)만 들어온다.
					// 할것 없다.
				}
				nCheckCount--;
				continue;

			} else if (pool->m_socktype > sock_listen) {
				// 메인소켓이 사용자 지정한 클라이언트일 때의 동작
 				// 사용자 지정 호출 함수가 있다면 호출하고 워크쓰레드로 넘기자
				if (pool->m_fmain) {
					it = pool->m_connect.find(sd);
					next = pool->m_fmain(it == pool->m_connect.end() ? 0 : it->second);
					if (next == tst_reconnected)
						continue;
				}
			}
		}

		if (events & EPOLLIN) {
			// 수신된 데이타길이는 바로 검증되지만 실제 read() 함수 호출하면 안된다 read() 함수는 워크쓰레드에서 해야한다....
			err = ioctl(sd, SIOCINQ, &check_len);	// tcp수신버퍼에 도착한 바이트 수를 check_len에 넣어준다, 성공시 0 리턴
		}
		else if (events & EPOLLOUT) {
			err = ioctl(sd, SIOCOUTQ, &check_len);	// tcp발신버퍼에 남은 바이트 수를 check_len에 넣어준다, 성공시 0 리턴
		}

		TRACE("tst main---events(%d), check_len(%d), err(%d)\n", events, check_len, err);

		// 연결이 끊어진 소켓의 뒷처리
		if ((events & EPOLLIN && !check_len) || err < 0 || events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
			// socket 구조체 찾아서 삭제
			pool->closesocket(sd);
			
			if (pool->m_sockmain == sd) {
				// 메인 소켓이 닫혔다.
				TRACE("tst main socket is closed......\n");
				pool->m_main_run = 0;
				break;
			}

			nCheckCount--;
			continue;
		}


		// 기타 사용자소켓 + 서브소켓처리
		// m_connect의 등록된 소켓을 사용 중 에는 삭제 되지 않는 다는 것을 전제한다
		// 즉 사용되고 있는 소켓에는 이벤트가 발생하지 않는 다는 것이다(EPOLLONESHOT 플러그 반드시 사용)
		// 그러므로 현재 소켓이 아닌 다른 소켓에 작업을 하려면 그 소켓을 사용하는 동안 삭제될 수 있으므로 매우 신중해야한다.
		it = pool->m_connect.find(sd);
		if (it != pool->m_connect.end()) {
			socket = it->second;
#if defined(ROUND_ROBIN)
			i = i >= pool->m_thread_count ? 0 : i;
			for (; i < pool->m_thread_count; i++) {
#else
			// 항상 0번부터 찾는 이유
			// 1. 워크쓰레드 중 몇 개가 항상 사용 중인지, 최대 동시 실행쓰레드가 몇인지 실행건수로 확인할 수 있다.
			// 2. 이를 통하여 적절한 쓰레드수를 설정하도록 돕는다
			// 3. 쓰레드 수는 보통 많이 설정 하지 않으므로 시스템 성능상 별 차이가 없다.
			for (i = 0; i < pool->m_thread_count; i++) {
#endif
				// 모든 쉬는 쓰레드를 확인하며 하나씩 일을 할당하고 깨운다
				if (pool->m_workers[i].thread_stat < tst_run) {
					pthread_mutex_lock(&pool->m_workers[i].mutex);
					pool->m_workers[i].thread_stat = tst_run;
					socket->events = events;
					if (socket->recv) socket->recv->checked_len = 0;
					if (socket->send) socket->send->checked_len = 0;
					if (events & EPOLLIN && socket->recv) {
						socket->recv->checked_len = check_len;
					}
					else if (events & EPOLLOUT && socket->send) {	// EPOLLOUT 인 경우 사용자 함수에서 신중히 처리 필요함
						socket->send->checked_len = check_len;
					}
					pool->m_workers[i].tst_socket = socket;
					TRACE("tst request a job sd=%d, thread no=%d, socket=%lX\n", sd, i, ADDRESS(pool->m_workers[i].tst_socket));
					pthread_cond_signal(&pool->m_workers[i].hEvent);
					pthread_mutex_unlock(&pool->m_workers[i].mutex);
					nCheckCount--;
					break;
				}				
			}
		}
	}

	// 만일 아직 남은 잡이 있다면 모든 쓰레드가 깨어나 나머지 일을 해야한다
	// 실행유지플래그(m_work_run)를 끄고 일제히 깨운다
	// 만약 thread_stat >= tst_run 상태라면 현재 작업중인 쓰레드이며 쓰레드 작업 완료 후 잠들기 전에 m_work_run 값을 검증하여 스스로 종료한다.
	pool->m_work_run = 0;

	for (i = 0; i < pool->m_thread_count; i++) {
		// 모든 쉬는 쓰레드를 깨운다
		pthread_mutex_lock(&pool->m_workers[i].mutex);
		if (pool->m_workers[i].thread_stat < tst_run) {
			pool->m_workers[i].thread_stat = tst_exit;
			pthread_cond_signal(&pool->m_workers[i].hEvent);
		}
		pthread_mutex_unlock(&pool->m_workers[i].mutex);
	}

	char* szThreadStatus = (char*)malloc(pool->m_thread_count);
	bzero(szThreadStatus, pool->m_thread_count);

	// 모든 워크쓰레드가 정상종료 되었는지 검증한다, 각 쓰레드별로 경과시간을 최대 g_endwaittime 만큼 기다린다
	TRACE("tst === check exited all work threads endwait seconds(%.6f)\n", pool->m_endwaittime / 1e+9);

	i = 0;
	nanoseconds = 0;

	do
	{
		usleep(100000);			// sleep 0.1 second
		nCheckCount = 0;
		for (i = 0; i < pool->m_thread_count; i++) {
			// 일하는 쓰레드 가 있으면 일이 종료될 때를 기다린다
			if (szThreadStatus[i]) {
				nCheckCount++;
			} else {
				if (pool->m_workers[i].thread_stat >= tst_exit) {
					szThreadStatus[i] = '9';
					nCheckCount++;
				} else {
					clock_gettime(CLOCK_MONOTONIC, &timenow);
					seconds = timenow.tv_sec - pool->m_workers[i].begin_time.tv_sec;
					nanoseconds = timenow.tv_nsec - pool->m_workers[i].begin_time.tv_nsec + (seconds * 1e+9);
					if (nanoseconds >= pool->m_endwaittime) {
						TRACE("tst === thread no(%d), need force exit check %.6f / %.6f\n", pool->m_workers[i].thread_no, nanoseconds / 1e+9, pool->m_endwaittime / 1e+9);
						szThreadStatus[i] = '9';
						nCheckCount++;
					} else {
						TRACE("tst === thread no(%d), elapsed time %.6f seconds, waittime %.6f\n", pool->m_workers[i].thread_no, nanoseconds / 1e+9, pool->m_endwaittime / 1e+9);
					}
				}
			}
		}
	} while (nCheckCount < (int)pool->m_thread_count);

	free(szThreadStatus);

	TRACE("tst === force kill. if still running thread\n");
	// 기다려도 종료하지 않은 쓰레드는 강제 종료 시킨다
	for (i = 0; i < pool->m_thread_count; i++) {
		if (pool->m_workers[i].thread_stat == tst_exited) {
			TRACE("tst === thread no(%d), exited check ok!\n", pool->m_workers[i].thread_no);
		}
		else {
			clock_gettime(CLOCK_MONOTONIC, &timenow);
			seconds = timenow.tv_sec - pool->m_workers[i].begin_time.tv_sec;
			nanoseconds = timenow.tv_nsec - pool->m_workers[i].begin_time.tv_nsec + (seconds * 1e+9);
			TRACE("tst === thread no(%d), still running. force kill. run time(%.6f second)\n", pool->m_workers[i].thread_no, nanoseconds / 1e+9);
			pthread_cancel(pool->m_workers[i].thread_id);
		}
	}

	// workthread memory free
	free(pool->m_workers);
	pool->m_workers = NULL;

	// thread count reset
	pool->m_thread_count = 0;

	// 관리쓰레드 명시적 종료
	pthread_exit(0);
}

void* tst_work(void* param)
{
	PTST_THREAD_INFO me = (PTST_THREAD_INFO)param;
	int nStat;
	struct timespec waittime;
	struct timespec timenow;
	uint32_t seconds = 0;
	uint64_t nanoseconds = 0;
	struct epoll_event ev;	///< 이벤트 처리종류(read/wtire/error) 지정용 epoll구조
	PTST_SOCKET socket;
	TST_STAT next;
	PTST_DATA data;

	while (me->pool->m_work_run) {

		nStat = 0;

		// pthread_cond_wait() 은 락걸린 뮤텍스를 언락하고 웨이팅이 들어간다
		// 시그날이 들어오면 자기가 깨어날 때 뮤텍스를 락걸고 나온다
		// 깨어나면 락을 자기가 가지고 있으므로 락을 가지고 할일만 하고 락을 풀어 주어야 해야한다
		if (me->thread_stat < tst_exit) {
			clock_gettime(CLOCK_REALTIME, &timenow);	//pthread_cond_timedwait()이 NTP 영향 받음
			waittime.tv_sec = timenow.tv_sec + me->pool->m_waittime.tv_sec;
			waittime.tv_nsec = timenow.tv_nsec + me->pool->m_waittime.tv_nsec;
			// 이 뮤텍스는 구조상 쓰레드마다 있으며 메인 쓰레드와의 동기화에만 사용된다.
			pthread_mutex_lock(&me->mutex);
			nStat = pthread_cond_timedwait(&me->hEvent, &me->mutex, &waittime);
			pthread_mutex_unlock(&me->mutex);
		}

		if (nStat && nStat != ETIMEDOUT) {
			// wait 실패시 -1 설정, errno 로 오류 확인 필요
			// EBUSY : 16	/* Device or resource busy */
			if (errno == EBUSY) {
				TRACE("tst workthread no(%d), timedwait() EBUSY, errno=%d\n", me->thread_no, errno);
			} else if (errno == EINVAL) {
				TRACE("tst workthread no(%d), timedwait() EINVAL, errno=%d\n", me->thread_no, errno);
			} else if (nStat == ETIMEDOUT) {
				TRACE("tst workthread no(%d), timedwait() ETIMEDOUT, errno=%d\n", me->thread_no, errno);
			} else {
				TRACE("tst workthread no(%d), timedwait() error, errno=%d\n", me->thread_no, errno);
				usleep(10000);// 10,000 마이크로초 => 10밀리초 => // 마이크로초 (백만분의1초 10의 -6승)
			}

			me->thread_stat = tst_suspend;
			continue;
		}

#if 0
		TRACE("tst workthread no(%d), getup or wakeup... status(%d, %d)\n", me->thread_no, me->thread_stat, errno);
#endif

		// 메인쓰레드가 상태를 변경해 주고 이벤트 시그날을 보내준다
		if (me->thread_stat == tst_run) {
			clock_gettime(CLOCK_MONOTONIC, &me->begin_time); // NTP영향 받으면 안됨

			next = tst_suspend;
			
			socket = me->tst_socket;
			// 실행명령 전달받음
			TRACE("tst workthread no(%d), I got a job. excuted:%ju, sd:%d, socket:%lX\n", me->thread_no, me->exec_count, socket ? socket->sd : -1, ADDRESS(me->tst_socket));


			if (socket->events & EPOLLIN) {
				// req_len 이 설정되어 있다면 수신처리 한다
				// 확인 후 사용자 함수를 호출 하여면 next = tst_run을 설정한다
				data = socket->recv;
				if (data && data->req_len) {
					if (data->com_len >= data->req_len) {
						next = tst_run;
					} else {
#ifdef DEBUGTRACE
						char* p = &data->s[data->req_pos + data->com_len];
#endif
						// 메인쓰레드 체크 이후 추가로 도착한 내역이 있을 수 있다...
						nStat = ioctl(socket->sd, SIOCINQ, &data->checked_len);
						if (nStat < 0) {
							// socket error
						}
						else {
#ifdef READ_SEREAL
							while (data->checked_len-- > 0 && data->result_len < data->req_len) {
								data->result_len += read(socket->sd, &data->s[data->result_len], 1);
							}
#else
							if (data->checked_len > (data->req_len - data->com_len))
								data->checked_len = (data->req_len - data->com_len); // 나머지는 담에 읽어라, 다음 메시지다
							data->com_len += read(socket->sd, data->s + data->req_pos + data->com_len, data->checked_len);
#endif
							if (data->com_len >= data->req_len) {
								data->checked_len = 0;
								next = tst_run;
							}
						}
						clock_gettime(CLOCK_REALTIME, &data->trans_time);
						TRACE("tst workthread auto read from(sd:%d) req_len=%d com_len=%d:%s:\n", socket->sd, data->req_len, data->com_len, p);
					}
				} else {
					next = tst_run;
				}
			} else if (socket->events & EPOLLOUT) {
				// req_len 이 설정되어 있다면 발신처리 한다
				// 확인 후 사용자 함수를 호출 하여면 next = tst_run을 설정한다
				data = socket->send;
				if (data && data->req_len) {
					// 메인쓰레드 체크 이후 데이타가 발신 되었을 수 있나????
					// nStat = ioctl(socket->sd, SIOCOUTQ, &data->checked_len);

					if (data->com_len >= data->req_len) {
						next = tst_run;
					} else {
						TRACE("tst workthread auto send to(sd:%d):%s:\n", socket->sd, &data->s[data->req_pos + data->com_len]);
						data->com_len += write(socket->sd, data->s + data->req_pos + data->com_len, data->req_len - data->com_len);
						clock_gettime(CLOCK_REALTIME, &data->trans_time);
						if (data->com_len < data->req_len)
							next = tst_send; // 다 보내지 못했다면 send 버퍼가 꽊찼다. 버퍼가 비워지면 다시 EPOLLOUT 이벤트를 발생시켜라.
						else
							next = tst_run;	// 다 보냈다. 다 보냈다는 것을 사용자지정함수에 전달하자
					}
				} else {
					next = tst_run;
				}
			}

			socket->thread_no = me->thread_no;

			if (next == tst_run && me->tst_func)
				next = me->tst_func(socket);

			clock_gettime(CLOCK_MONOTONIC, &me->end_time); // NTP영향 받으면 안됨
			seconds = me->end_time.tv_sec - me->begin_time.tv_sec;
			nanoseconds = me->end_time.tv_nsec - me->begin_time.tv_nsec + (seconds * 1e+9);
			me->sum_time += nanoseconds / 1e+6; // milli seconds
			if (me->most_elapsed < nanoseconds)
				me->most_elapsed = nanoseconds;

			me->exec_count++;
			TRACE("tst workthread no(%d), I finished a job. excuted:%ju, sd:%d, elapsed:%.6f , next:%d\n", me->thread_no, me->exec_count, socket ? socket->sd : -1, nanoseconds / 1e+9, next);

			// 구조상 이 값은 메인쓰레드와 웨크쓰레드가 같이 수정하므로 락을 건다
			// 그리고 이 이하 lock ~ unlink 까지는 그 순서가 매우중요하므로 절대 수정 금지
			// 뮤텍스 락과 상관없이 epoll 은 등록하자마자 동작한다
			pthread_mutex_lock(&me->mutex);
			if (next == tst_disconnect) {
				if (socket && socket->sd > 0)
					me->pool->closesocket(socket->sd);
				next = tst_suspend;
			}
			// 처리가 끝난 소켓 중 연결을 끊지 않은 소켓은 epoll에 수정등록한다
			if (socket && socket->sd > 0) {
				memset(&ev, 0x00, sizeof(ev));
				// EPOLLOUT 은 필요시만 설정한다 아니면 무한루프...
				ev.events = EPOLLIN /*| EPOLLOUT*/ | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
				// next == tst_send: req_len을 설정하면 다음번 work_thread 가 보내준다
				// next == tst_send: req_len을 설정하지 않으면 사용자가 메시지 보낸 경우다 메시지 다 보낸 후에 EPOLLOUT 으로 사용자함수 호출되며 checked_len = 0 이다
				// next == tst_send: req_len을 설정하지 않으면 사용자가 다 보낸것을 확인 후에 tst_disconnect 호출해주는 것을 추천한다
				if (next == tst_send) {
					ev.events |= EPOLLOUT;
					next = tst_suspend;
				}
				ev.data.fd = socket->sd;
				if (epoll_ctl(me->pool->m_epfd, EPOLL_CTL_MOD, ev.data.fd, &ev) < 0)
				{
					TRACE("[epoll_ctl(EPOLL_CTL_MOD)] sd:%d, failed mod!(%s)\n", ev.data.fd, strerror(errno));
					// 흐미 뭐지? 어케하노? 이런 일은 없을거라...
#ifdef DEBUG
				} else {
					TRACE("[epoll_ctl(EPOLL_CTL_MOD)] sd:%d, mod success, ready to next socket access\n", ev.data.fd);
#endif
				}
			} 

			me->thread_stat = next;

			// 메인쓰레드가 tst_socket 을 설정하고 쓰레드를 깨운다
			me->tst_socket = NULL; //요거 락없이 걸면 메인 쓰레드와 쫑난다
			pthread_mutex_unlock(&me->mutex);

		} else if (me->thread_stat == tst_suspend) {
			me->thread_stat = tst_run;

			// TRACE("workthread no(%d), I got a idle time, idle count:%lu\n", me->thread_no, me->idle_count);

			next = tst_suspend;

			if (me->tst_idle)
				next = me->tst_idle(NULL);

			me->idle_count++;
			// TRACE("workthread no(%d), I finished a idle time. count: %lu, next: %d\n", me->thread_no, me->idle_count, next);

			// 구조상 이 값은 메인쓰레드와 웨크쓰레드가 같이 수정하므로 락을 건다
			pthread_mutex_lock(&me->mutex);
			me->thread_stat = next;
			pthread_mutex_unlock(&me->mutex);

		} else if (me->thread_stat == tst_exit) {
			
			// 명시적으로 죽으라는 메시지를 받았다.
			TRACE("tst workthread no(%d), my signal is tst_exit\n", me->thread_no);

			me->exit_code = 9;	// tst_exit 상태확인 하고 종료함

			me->thread_stat = tst_exited;

			TRACE("tst workthread no(%d), I'm going to exiting\n", me->thread_no);

			break; /// exit from work loop
		} else {

			// ????


			// 구조상 이 값은 메인쓰레드와 웨크쓰레드가 같이 수정하므로 락을 건다
			pthread_mutex_lock(&me->mutex);
			me->thread_stat = tst_suspend;
			pthread_mutex_unlock(&me->mutex);

		}
	}

	me->thread_stat = tst_exited;
	
	TRACE("tst workthread no(%d), terminated, excuted_count=%ju, most_elapsed=%.6f\n", me->thread_no, me->exec_count, me->most_elapsed / 1e+9);

	pthread_exit(0);
}

int tstpool::create(int thread_count, const char* bind_ip, unsigned short bind_port, tstFunction func
	, uint32_t max_recv_size, uint32_t max_send_size, pthread_attr_t* attr)
{
	// 만일 bind_port 가 유효하지 않다면 이미 사용자 가 메인 소켓에 대한 처리를 환료하고 create() 함수를 했다고 가정한다
	// then --> socket connect() and set to m_socktype = sock_client......
    if (bind_port > 0) {
        // 소켓 생성
		m_sockmain = tcp_create(bind_ip, bind_port);

		if (m_sockmain < 1)
			return -1;
		m_socktype = sock_listen;
    }

	m_epfd = epoll_create(1); // epoll descriptor 생성

	m_thread_count = thread_count;

	pthread_mutex_init(&m_mutexConnect, NULL);
	pthread_mutex_init(&m_mutexWork, NULL);

	m_workers = (PTST_THREAD_INFO)calloc(sizeof(TST_THREAD_INFO) * m_thread_count,1);
	m_size_event = sizeof(struct epoll_event) * m_thread_count;
	m_events = (struct epoll_event*)calloc(m_size_event, 1);

	// 쓰레드 생성전에 설정해야한다. 값이 0이면 쓰레드 만들자 마자 종료한다
	m_main_run = 1;
	m_work_run = 1;

	m_recv_buf_size = max_recv_size;
	m_send_buf_size = max_send_size;

	// 워크쓰레드 생성
	uint32_t i;
	for (i = 0; i < m_thread_count; i++) {
		m_workers[i].thread_no = i;
		m_workers[i].thread_stat = tst_suspend;
		m_workers[i].tst_func = func;
		m_workers[i].pool = this;
		pthread_mutex_init(&m_workers[i].mutex, NULL);
		pthread_cond_init(&m_workers[i].hEvent, NULL);
		pthread_create(&m_workers[i].thread_id, attr, tst_work, &m_workers[i]);
		pthread_detach(m_workers[i].thread_id);	// pthread_exit(0); 시 리소스 자동 해제
	}

	// 관리 쓰레드 생성
	pthread_create(&m_main_thread, NULL, tst_main, this); // 관리 쓰레드는 pthread_join() 으로 동기화 해서 종료할 것임

	if(!m_main_run) {
		// 흐미 쓰레드 생성중 뭐가 문제 발생했다.
		// 모든 자원해제하자
		pthread_join(m_main_thread, NULL); // 메인쓰레드 리소스를 해제한다

		if (m_events)
			free(m_events);
		m_events = NULL;

		for (i = 0; i < m_thread_count; i++) {
			pthread_cond_destroy(&m_workers[i].hEvent);
			pthread_mutex_destroy(&m_workers[i].mutex);
		}

		if (m_workers)
			free(m_workers);
		m_workers = NULL;

		pthread_mutex_destroy(&m_mutexWork);
		pthread_mutex_destroy(&m_mutexConnect);

		TRACE("=== failed: thread pool create\n");
		return 0;
	}

	TRACE("=== order: thread pool create, work thread count=%d\n", m_thread_count);
	
	return 1;
}

int tstpool::destroy(uint64_t wait_time)
{
	struct epoll_event ev;	///< 이벤트 처리종류(read/wtire/error) 지정용 epoll구조
	PTST_SOCKET socket = NULL;

	if (!m_main_thread || !m_main_run)
		return 0;
	TRACE("=== order: tstpool destroy start ===================\n");

	m_endwaittime = wait_time;
	m_main_run = 0;

	// 모든 쓰레드 종료 동기화가 필요하면???
	pthread_join(m_main_thread, NULL); // 쓰레드 종료를 기다리고 종료되면 리소스를 해제한다

	m_main_thread = 0;

	// 사용자가 지정한 클라이언트 소켓이 메인이면 사용자가 닫아야한다...
	if (m_socktype == sock_listen) {
		if (m_sockmain > INVALID_SOCKET) {
			close(m_sockmain);
			m_sockmain = INVALID_SOCKET;
		}
	}

    // m_connect 의 모든 메모리 해제 후에 clear
    map<int, PTST_SOCKET>::iterator it_connect;
    for (it_connect = m_connect.begin(); it_connect != m_connect.end(); it_connect++ ) {
		socket = it_connect->second;
		memset(&ev, 0x00, sizeof(ev));
		ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
		ev.data.fd = socket->sd;
		epoll_ctl(m_epfd, EPOLL_CTL_DEL, socket->sd, &ev);
		TRACE("[epoll_ctl(EPOLL_CTL_DEL)] sd:%d, deleted\n", ev.data.fd);

		if (socket->sd > INVALID_SOCKET) {
			//struct linger optLinger={1,0};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 버려 버린다(바로리턴)
			//struct linger optLinger={1,5};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(송신완료까지블럭, 5초만 블럭인 시스템도 있고)
			struct linger optLinger = { 0,0 };//linger on! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(바로리턴,백그라운드처리로time_wait)
			setsockopt(socket->sd, SOL_SOCKET, SO_LINGER, (char*)&optLinger, sizeof(optLinger));
			close(socket->sd);
		}

		delete socket;
    }
    m_connect.clear();

    if (m_events) 
        free(m_events);
    m_events = NULL;

	uint32_t i;
	for (i = 0; i < m_thread_count; i++) {
		pthread_cond_destroy(&m_workers[i].hEvent);
		pthread_mutex_destroy(&m_workers[i].mutex);
	}

	if (m_workers)
		free(m_workers);
	m_workers = NULL;

	pthread_mutex_destroy(&m_mutexWork);
	pthread_mutex_destroy(&m_mutexConnect);

	TRACE("=== %s() terminated now\n", __func__);

	return 1;
}

bool tstpool::need_send(int sd)
{
	if (!m_main_thread || !m_main_run)
		return 0;

	// 특정 클라이언트에 작업을 하고 싶다면 EPOLLOUT 을 설정하면 된다
	// 이거 설정되면 워크 쓰레드가 사용자 함수 호출 하며 TST_SOCKET::event 에 EPOLLOUT 설정한다
	struct epoll_event ev;	///< 이벤트 처리종류(read/wtire/error) 지정용 epoll구조
	memset(&ev, 0x00, sizeof(ev));
	ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
	ev.data.fd = sd;
	if (epoll_ctl(m_epfd, EPOLL_CTL_MOD, ev.data.fd, &ev) < 0)
	{
		TRACE("[epoll_ctl(EPOLL_CTL_MOD)] sd:%d, failed mod!(%s)\n", ev.data.fd, strerror(errno));
		// 흐미 삭제된 소켓에 수정을 시도했네...
		return false;
	}

	TRACE("[epoll_ctl(EPOLL_CTL_MOD)] sd:%d, mod success, ready to next socket access\n", ev.data.fd);
	
	return true;
}

void tstpool::closesocket(int sd)
{
	struct epoll_event ev;	///< 이벤트 처리종류(read/wtire/error) 지정용 epoll구조
	PTST_SOCKET socket = NULL;

	pthread_mutex_lock(&m_mutexConnect);
	map<int,PTST_SOCKET>::iterator it = m_connect.find(sd);
	if (it != m_connect.end()) {
		socket = it->second;
		m_connect.erase(it);
	}
	pthread_mutex_unlock(&m_mutexConnect);

	TRACE("disconnect sd=%d, client ip=%s, client port=%d\n", socket->sd, inet_ntoa(socket->client.sin_addr), socket->client.sin_port);

	memset(&ev, 0x00, sizeof(ev));
	ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
	ev.data.fd = sd;
	epoll_ctl(m_epfd, EPOLL_CTL_DEL, sd, &ev);
	TRACE("[epoll_ctl(EPOLL_CTL_DEL)] sd:%d, deleted\n", ev.data.fd);

	if (sd > INVALID_SOCKET) {
		//struct linger optLinger={1,0};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 버려 버린다(바로리턴)
		//struct linger optLinger={1,5};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(송신완료까지블럭, 5초만 블럭인 시스템도 있고)
		struct linger optLinger = { 0,0 };//linger on! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(바로리턴,백그라운드처리로time_wait)
		setsockopt(sd, SOL_SOCKET, SO_LINGER, (char*)&optLinger, sizeof(optLinger));
		close(sd);
	}

	if (m_fdisconnected)
		m_fdisconnected(socket);

	if(socket)
		delete socket;	
}
