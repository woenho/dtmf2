#ifndef _TST_H_
#define _TST_H_
/*
* programming by woenho@daum.net
* 
* 레벨트리거 epoll 방식을 사용한다.
* EPOLLONESHOT 을 지정해서 일부 엣지트리거 처럼 동작하게 하지만 엣지트리거를 사용하지 않는다
* 이유는 두 개의 메시지가 연이어 도착했을 때, 첫번째 메시지만 처리하고 나머지는 읽지도 않은 상태에서
* 레벨트리거방식으로 epoll_ctl()로 EPOLLIN 플래그 설정하면 다시 두번째 메시지에 대한 통지가 즉시 온다.
* 엣지트리거를 사용하면 이 경우 두번째 메시지는 추가로 세번째 메시지가 올 때 가지 통지가 없다.
* 즉 한 번에 들어온 모든 메시지를 처리해야한다. 이는 프로그래밍상 좀 불편한다고 판단한다(비추천)
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
// network header
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
// c++ stl header
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <fstream>

#ifndef INVALID_SOCKET
#define INVALID_SOCKET	(-1)
#endif

// casting for 64bit os -> ((ulong)a), casting for 32bit os -> ((uint)a)
#if ( __WORDSIZE == 32 )
	#define ADDRESS(a)		((uint)a)
#else
	#define ADDRESS(a)		((ulong)a)
#endif

namespace tst {

	struct TST_USER_T;		// 포인터 선언..
	struct TST_SOCKET_T;	// 포인터 선언..
	class tstpool;			// 포인터 선언..
	
	// 소켓타입
	// sock_client: 사용자가 직접 외부 호스트에 연결한 소켓
	// sock_sub: 소켓타입이 sock_listen 인 소켓으로 부터 accept 받은 서브 소켓
	// sock_listen: tstpool 가 서버모드로 동작할 때 listen 한 소켓
	// sock_user(n): 사용자가 별도 관리하고 싶은 소켓의 그룹을 분류하는 방식
	typedef enum { sock_sub = 0, sock_listen, sock_client, sock_user1, sock_user2, sock_user3 } TST_SOCKETTYPE;
	// 워크 쓰레드의 현재 상태
	typedef enum { tst_suspend = 0, tst_run, tst_send, tst_reconnected, tst_disconnect, tst_exit = 9, tst_exited } TST_STAT;

	// 워크쓰레드가 호출할 사용자함수 형식
	typedef TST_STAT(*tstFunction)(struct TST_SOCKET_T* psocket);
	// TST_USER 구조체 삭제를 위한 함수
	typedef void(*CleanFunction)(struct TST_USER_T* puser);

	// TST_DATA에서 사용하는 사용자지정 데이타구조체
	typedef struct TST_USER_T {
		CleanFunction removeBuffer;
		int type;					// TST_DATA가 여떤용도로 사용되는지 사용자가 지정해 두는 변수
		uint32_t s_len;				// 사용자지정 데이타크기
		char s[];					// 사용자지정 데이타
	}TST_USER, *PTST_USER;
	// 송수신버퍼에 대한 구조체
	// ----------------------------------------------
	// TST_DATA::s 가 가변이라 new 사용하지 말고 malloc 해서 사용하시라
	// ----------------------------------------------
	typedef struct TST_DATA_T {
		void reset_data() { req_len = req_pos = com_len = 0; bzero(s, s_len); }
		uint32_t req_pos;			// req_len 설정시 그 시작점이 되는 위치 (현재 버퍼위치==> req_pos + com_len )
		uint32_t req_len;			// 수신버퍼라면 수신해야할 메시지의 크기, 발신할 결우 발신할 메시지의 크기를 설정하면 이후 result_len 이 req_len 과 같아질때까지 쓰레드가 알아서 처리한다
		uint32_t com_len;			// 수신버퍼라면 수신이 완료된 바이트 수, 발신버퍼라면 발신완료된 바이트 수로 사용자가 잘 설정하여 사용하도록 한다
									// req_len==0 이거나 req_len==com_len 가 되면 사용자함수(THREADINFO::tst_func)를 호출한다.
		uint32_t checked_len;		// 수신버퍼라면 수신해야될 도작된 바이트 수, 발신버퍼라면 발신해야할 남은 바이트수
		struct timespec trans_time;	// NTP 기준 마지막 송수신 시각
		uint32_t s_len;				// 송수신버퍼의 크기정보
		char s[];					// 송수신버퍼로 create() 함수에서 지정한 만큼 확보한다
	} TST_DATA, * PTST_DATA;

	// -----------------------------------------------
	// TST_SOCKET 은 malloc 하지 말고 가능하면 new로 사용하자
	// ----------------------------------------------------
	typedef struct TST_SOCKET_T {
		int thread_no;				// current work thread no, 사용자함수 호출할 때 마다 변경됨
		int sd;						// socket descriptor of cennected client 
		TST_SOCKETTYPE type;		// socket type (0: sock_sub, 1: sock_listen, 2: sock_client)
		struct sockaddr_in client;	// remote ip & remote port
		uint32_t events;			// EPOLLIN,EPOLLOUT,EPOLLPRI,EPOLLHUP,EPOLLRDHUP,EPOLLERR,EPOLLET,EPOLLONESHOT
		PTST_DATA recv;				// 수신할 데이타 정보, 송수신을 동시에 수행 할 수 있으므로 따로이 확보한다
		PTST_DATA send;				// 송신할 데이타 정보, 송수신을 동시에 수행 할 수 있으므로 따로이 확보한다
		PTST_USER user_data;		// 송수신 버퍼가 아닌 사용자가 임의값을 설정하여 사용하고 싶을 때 사용, TST_SOCKET 메모리해제시 free(user_data)호출

		// 사용자가 필요에 따라 직접 설정하여 사용하는 함수 정의(인수는 this)
		// 쓰레드가 호출한 tst_func 에서 데이타처리를 하지 않고 추가로 다시 함수를 호출하고 싶다면 해당 소켓정보에 그 함수를 기술한다
		// 쓰레드가 호출한 tst_func 에서 이 함수를 호출 하도록 구현되어야 한다
		tstFunction func;		// 어떤 특정 client 에 대한 작업이 필요한 경우 설정하여 사용하면 되겠다.

		inline void initBuffer(uint32_t recv_len, uint32_t send_len){
			if (recv_len > 1) {
				if (recv) free(recv);
				recv = (PTST_DATA)calloc(sizeof(TST_DATA) + recv_len, 1);
				recv->s_len = recv_len;
			}
			if (send_len > 1) {
				if (send) free(send);
				send = (PTST_DATA)calloc(sizeof(TST_DATA) + send_len, 1);
				send->s_len = send_len;
			}
		}
		
		inline void removeBuffer() {
			if (recv) {
				free(recv);
				recv = NULL;
			}
			if (send) {
				free(send);
				send = NULL;
			}
			if (user_data) {
				if (user_data->removeBuffer)
					user_data->removeBuffer(user_data);
				free(user_data);
				user_data = NULL;
			}
		}

		TST_SOCKET_T() {
			bzero(this, sizeof(*this));
		}

		TST_SOCKET_T(uint32_t recv_len, uint32_t send_len) {
			bzero(this, sizeof(*this));
			initBuffer(recv_len, send_len);
		}
		
		~TST_SOCKET_T() {
			removeBuffer();
		}
	} TST_SOCKET, * PTST_SOCKET;

	typedef struct TST_THREAD_INFO_T
	{
		// thread base info
		int				thread_no;		// 쓰레드 고유일련번호
		pthread_t		thread_id;		// 쓰레드 아이디
		pthread_attr_t	attr;			// 쓰레드 속성
		tstpool*		pool;			// tstpool object's pointer
		int				exit_code;		// 쓰레드 종료시 종료코드
		TST_STAT		thread_stat;	// 쓰레드의 현재 상태
		PTST_SOCKET		tst_socket;		// epoll로 부터 이벤트가 오면 소켓정보를 m_connect에서 찾아 여기에 할당하고 사용자함수를 호출

		// mutex 초기화(선언과 함께=PTHREAD_MUTEX_INITIALIZER) or pthread_mutex_init(&mutex,NULL);
		pthread_mutex_t	mutex;			// 쓰레드의 최적화숫자를 산정하기 위하여 각 워크쓰레드별로 뮤텍스를 관리한다
		pthread_cond_t	hEvent;			// 쓰레드 깨움 이벤트시그널

		// 쓰레드 통계
		struct timespec	begin_time;		// 마지막 수행한 잡의 수행 시작 시각
		struct timespec	end_time;		// 마지막 수행한 잡의 수행 종료 시각
		// 32bit , 64bit 공통으로 사용하기 위해 uint64_t 지정
		uint64_t		sum_time;		// milliseconds (수행 시간의 합)
		uint64_t		most_elapsed;	// nano seconds. 타스크 처리사간 중 가장 오래 걸린 사긴은?
		uint64_t		exec_count;		// 쓰레드가 요청을 실행한 건수
		uint64_t		idle_count;		// 쓰레드가 쉬고있다

		// thread function
		tstFunction		tst_func;		// socket 으로부터 데이타가 들어오면 이를 처리할 사용자 함수
		tstFunction		tst_idle;		// 쓰레드가 쉬고있을 때 호출할 사용자 함수
		double get_averageElapsedtime() { return exec_count ? sum_time / exec_count : 0.0f;}
	} TST_THREAD_INFO, * PTST_THREAD_INFO;

	//----------------------------------------
	// tst (tcp server threadpool) class
	//----------------------------------------
	class tstpool {
	public:
		tstpool();
		~tstpool();
		int tcp_create(const char* ip, u_short port_no);
		int tcp_connect(const char* dns, u_short port_no, struct sockaddr_in* paddr);

		int create(int thread_count				// 쓰레드풀에 생성할 쓰레드 숫자
			, const char* bind_ip				// bind_port 가 0이면 무시된다
			, unsigned short bind_port			// bind_port 가 0이면 서버소켓을 생성하지 않는다. 클라이언트소켓{socket_connect()}들 만으로 쓰레드 동작
			, tstFunction func				// 사용자 함수
			, uint32_t max_recv_size=4096
			, uint32_t max_send_size=4096
			, pthread_attr_t* attr=NULL
			); 
		int destroy(uint64_t wait_time = (uint64_t)5e+9);
		bool need_send(int sd);		// 먼저 send 버퍼 작업후에 호출하자
		void closesocket(int sd);	// 클라이언트와 연결을 임의로 끊고 싶을 때 사용, 오류로 인한 close는 자동으로 처리된다.
		
		inline void addsocket(PTST_SOCKET socket) {
			if (m_recv_buf_size && !socket->recv) {
				socket->recv = (PTST_DATA)calloc(sizeof(TST_DATA) + m_recv_buf_size, 1);
			}
			if (m_send_buf_size && !socket->send) {
				socket->send = (PTST_DATA)calloc(sizeof(TST_DATA) + m_send_buf_size, 1);
			}
			pthread_mutex_lock(&m_mutexConnect);
			m_connect[socket->sd] = socket;
			pthread_mutex_unlock(&m_mutexConnect);
		}

		// 최초 연결부터 클라이언트에 뭐가 쓰고 싶다면 m_fconnected() 에서 발신등록해라 (첨 등록에 EPOLLOUT 없다)
		// 또는 사용자가 방화벽 처리를 하고 싶다면 m_fconnected()에서 처리해라
		// tst_send 보다 큰 값을 리턴하면 연결을 해제한다
		// 나중에 다른 클라이언트에 발신하고 싶으면 need_send() 호출해라
		void setEventNewConnected(tstFunction func) { m_fconnected = func; }		// m_fconnected 함수 설정
		void setEventDisonnected(tstFunction func) { m_fdisconnected = func; }	// m_fdisconnected 함수 설정

		// 최초 연결부터 클라이언트에 뭐가 쓰고 싶다면 m_fconnected() 에서 버퍼에 설정만 하고 tst_send를 리턴해라.
		// 워크쓰레드가 보내도록 하자
		// 만일 특정 클라이언트에 대한 연결을 허가하지 않는다면 tst_disconnect 를 리턴해라
		// 새로운 연결이 들어 왔을 때 TST_SOCKET을 생성하고 epoll에 등록하기전에 호출해 준다
		tstFunction m_fconnected;
		tstFunction m_fdisconnected;		// 연결이 종료되면 TST_SOCKET을 지우기 전에 호출해 준다
		
		// m_fmain 이 함수는 매우 신중히 설정해야한다.
		// 반드시 필요한 경우만 설정하여 사용하고 기본적 업무처리는 워크쓰레드로 넘겨야 한다
		tstFunction m_fmain;				// 메인소켓이 사용자 지정소켓(sock_client)인 경우 워크쓰레드로 넘기기전에 호출한다

	public:
		struct in_addr m_bind_ip;			// server bind address
		ushort	m_port;						// server port
		int		m_epfd;						// epoll descriptor
		pthread_t m_main_thread;			// main thread id
		uint32_t m_thread_count;			// work thread count
		bool m_main_run;					// main thread loop boolean value, if set to 0 then main thread terminate
		bool m_work_run;					// work thread loop boolean value, if set to 0 then work thread terminate
		TST_SOCKETTYPE m_socktype;			// m_sockmain type (0: sock_sub, 1: sock_listen, 2: sock_client)
		// 사용자지정(sock_client)이면 연결을 직접하고 TST_SOCKET도 직접 new 해서 addsocket()을 통해 m_connect에 추가 해야한다
		int m_sockmain;					// main socket id, 메인쓰레드에서 자동으로 epoll에 등록해 준다
		// mutex 초기화(선언과 함께=PTHREAD_MUTEX_INITIALIZER) or pthread_mutex_init(&mutex,NULL);
		pthread_mutex_t	m_mutexConnect;		// m_connect 에 대한 동기화작업이 필요하면 사용한다
		pthread_mutex_t	m_mutexWork;		// 워크쓰레드들 사이에 동기화가 필요한 경우 사용하기 위한 뮤텍스
		PTST_THREAD_INFO m_workers;				// 워크쓰레드 테이블
		struct timespec m_waittime;			// 각 쓰레드 마다 스스로 깨어날 시간을 지정한다 (default 3초)
		uint64_t m_endwaittime;				// 쓰레드풀 종료 시, 아직 실행되고 있는 워크 쓰레드의 종료를 기다리는 시간(나노초)
		uint32_t m_size_event;				// m_events 의 크기
		struct epoll_event* m_events;		// epoll_wait() 결과를 저장하는 메모리로 쓰레드 생성할 때 쓰레드숫자만큼 확보한다
		std::map<int, PTST_SOCKET> m_connect;	// 연결된 클라이언트 소켓을 키로 소켓에서 사용하는 사용자데이타구조체를 관리한다
		uint32_t m_recv_buf_size;
		uint32_t m_send_buf_size;
	};
}

// -------------------------------------------

#ifndef _DEBUGTRACE_
#define _DEBUGTRACE_
#if defined(DEBUGTRACE)
	#define TRACE(...) \
	/* do while(0) 문은 블록이 없는 if문에서도 구문 없이 사용하기 위한 방법이다 */ \
	do { \
		struct timeval debug_now; \
		struct	tm debug_tm; \
		gettimeofday(&debug_now, NULL); \
		localtime_r(&debug_now.tv_sec,&debug_tm);\
		char debug_buf[4096+24]; \
		int debug_len = sprintf(debug_buf,"%04d-%02d-%02d %02d:%02d:%02d.%.3ld " \
			, debug_tm.tm_year + 1900 \
			, debug_tm.tm_mon + 1 \
			, debug_tm.tm_mday \
			, debug_tm.tm_hour \
			, debug_tm.tm_min \
			, debug_tm.tm_sec \
			, debug_now.tv_usec / 1000 \
			); \
		snprintf(debug_buf+debug_len,sizeof(debug_buf)-debug_len,__VA_ARGS__); \
		fwrite(debug_buf,sizeof(char),strlen(debug_buf),stdout); \
		fflush(stdout); \
	}while(0) 
#else
	#define TRACE(...) 
#endif
#endif

#endif

