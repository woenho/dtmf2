#ifndef _AMIPROC_H_
#define _AMIPROC_H_

#include "tst.h"
#include "AsyncThreadPool.h"
#include "util.h"


using namespace std;
using namespace tst;

typedef struct AMI_LOGIN_T {
	char Host[32];
	uint16_t Port;
	char Username[32];
	char Secret[32];
}AMI_LOGIN, * PAMI_LOGIN;

typedef struct AMI_EVENTS_T {
	int nThreadNo;						// atp워크쓰레드 고유일련번호
	char event[4096];					// 수신된 AMI event 를 복사해 놓는다
	uint16_t rec_count;					// event에 수신된 레코드 건수
	char* key[100];						// event에 수신된 레코드 중 키의 위치를 기록
	char* value[100];					// event에 수신된 레코드 중 값의 위치를 기록
}AMI_EVENTS, * PAMI_EVENTS;

typedef ATP_STAT eventproc(AMI_EVENTS& events);

typedef enum {ami_base=100} AMI_TYPE;
typedef enum { action_none, action_requst, action_response } AMI_MODE;
typedef void ami_callbak(PTST_SOCKET psocket);

void* rm_ami_socket(PTST_USER puser);

typedef struct AMI_RESPONSE_T {
	AMI_MODE mode;						// 현재 이 소켓에 ami action 의 처리단계 및 상태
	int result;							// mode == action_response 일 때 그 결과 값 (0: 성공, 이외 값은 오류코드)
	char msg[128];						// result 가 설정되면 그 사유를 기록해 둔다
	AMI_EVENTS responses;				// result 결과전문을 저장한다.
	AMI_RESPONSE_T() { bzero(this, sizeof(*this)); }
}AMI_RESPONSE, *PAMI_RESPONSE;

// ami_socket 구조체 멤버 user_data 에 등록된다. 별도의 ami 연결이 필요하면 별도로 관리해야한다.
typedef struct AMI_MANAGE_T {
	// 사용자 프로그램에서는 절대 설정 금지. 이 변수가 설정되면 ami_event() 에서는 Response 수신 시 대기프로세스를 깨워준다
	PAMI_RESPONSE pResp;				// ami action의 결과값을 받기 위해 AMI_MANAGE::ami_sync()에서 설정하고 ami_event()함수에서 널처리한다
	// --------------------------
	ami_callbak func;					// AMI ACTION 이 끝났을 때 호출 함수
	pthread_mutex_t	mutexAMI;			// AMI 명령을 보내기 위한 대기열
	pthread_mutex_t	mutexResp;			// AMI명령에 대한 답변을 기다리는 대기열
	pthread_cond_t	condResp;			// AMI명령에 대한 답변을 연동하는 시그널
	uint32_t ami_waitcount;				// AMI 명령을 보내기 위하여 대기하는 대기열 수
	uint32_t resp_waitcount;			// AMI 명령에 대한 응답대기 대기열 수로 필요하지 않으나 혹시 프로그래밍상 오류로 2개가 된다면 이를 검증해야하기에 추가함
	uint32_t actionid;					// AMI 명령을 보내는 순번 

	static PTST_USER alloc() {
		uint32_t s_len = sizeof(TST_USER) + sizeof(struct AMI_MANAGE_T);
		PTST_USER puser = (PTST_USER)calloc(s_len,1);
		puser->s_len = s_len;
		puser->type = ami_base;
		puser->removeBuffer = (CleanFunction)rm_ami_socket;
		((struct AMI_MANAGE_T*)puser->s)->init();
		return puser;
	}

	void init() {
		bzero(this, sizeof(*this));
		pthread_mutex_init(&mutexAMI, NULL);
		pthread_mutex_init(&mutexResp, NULL);
		pthread_cond_init(&condResp, NULL);
	}
	void destroy() {
		pthread_mutex_destroy(&mutexAMI);
		pthread_mutex_destroy(&mutexResp);
		pthread_cond_destroy(&condResp);
	}
	int ami_lock() {
		ami_waitcount++;
		return pthread_mutex_lock(&mutexAMI);
	}
	int ami_unlock() {
		ami_waitcount--;
		return pthread_mutex_unlock(&mutexAMI);
	}
	int resp_lock() {
		resp_waitcount++;
		return pthread_mutex_lock(&mutexResp);
	}
	int resp_unlock() {
		resp_waitcount--;
		return pthread_mutex_unlock(&mutexResp);
	}

	PAMI_RESPONSE ami_sync(char* action, bool logprint = true);
	void ami_async(char* action);

}AMI_MANAGE, *PAMI_MANAGE;

extern int g_exit;
extern tstpool server;				// 기본적인 tcp epoll 관리쓰레드 풀 클래스 객체
extern PTST_SOCKET ami_socket;		// ami 연결용 TST_SOCKET 구조체로 new 로 직접 생성하여 server.addsocket(ami_socket)으로 추가등록하고 epoll 도 직접 등록한다.
extern map<const char*, void*> g_process;
extern int log_event_level;

int parse_amievent(AMI_EVENTS& events);
const char* get_amivalue(AMI_EVENTS& events, const char* key);
void logging_events(AMI_EVENTS& events);

TST_STAT ami_event(PTST_SOCKET psocket);
TST_STAT http(PTST_SOCKET psocket);
TST_STAT my_disconnected(PTST_SOCKET psocket);
ATP_STAT atpfunc(PATP_DATA atpdata);
ATP_STAT process_events(PATP_DATA atp_data);


#endif
