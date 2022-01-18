/*
* 이 파일은 대단히 중요한 코어기능을 수행한다
* 수정함에 매우 신중해야한다.
* 그 기능은 아래와 같은 목록이다
* 1. ami event 메시지를 분리하여 AMI_EVENTS 구조체에 넣는다
* 2. 처리대상 이벤트를 선별하여 AsyncThreadPool에 작업을 의뢰한다 (process_events() 함수를 호출하게 설정한다)
* 3. 이벤트 관련 중요한 몇가지 함수를 포함한다
* 3.1 rm_ami_socket(): 생성된 ami 소켓정보를 삭제한다
* 3.2 get_amivalue(): AMI_EVENTS 구조체에서 지정된 키에 해당하는 값을 찾아준다
* 3.3 logging_events(): 파싱된 AMI_EVENTS 구조체의 내용을 로깅해 준다
* 3.4 parse_amievent: ami event 메시지를 파싱하여 AMI_EVENTS 구조체에 넣는다
* 3.5 ami_sync(): ami action을 실행하고 그 결과값을 가져오기까지 동기화해준다
* 3.6 ami_async(): ami action을 실행하고 그 결과는 무시된다
* 3.7 ami_event(): ami 이벤트 처리중심엔진이다. 계속 들어오는 이벤트정보를 한 개 한 개의 메시지단위로 분리 수신하고 분리하여 처리할 함수를 호출해준다 
* 3.8 my_disconnected(): ami 통신연결이 분리되면 호출되며 글로별변수 ami_socket을 리셋한다, 소켓정보삭제는 tstpoll 에서 자동으로 이루어진다
* 3.9 atpfunc(): AsyncThreadPool 에서 호출되는 기본 인트로 함수로 실체 event를 처리하는 메인함수인 process_events() 를 호출해주도록 한다.
* 3.10 process_events(): AsyncThreadPool 을 통하여 호출되는 이벤트처리 메인함수로 g_process 등록된 실제 이벤트별 처리함수를 호출해준다
*/

#include "amiaction.h"
#include "http.h"
#include "processevents.h"


tstpool server;						// 기본적인 tcp epoll 관리쓰레드 풀 클래스 객체
PTST_SOCKET ami_socket = NULL;		// ami 연결용 TST_SOCKET 구조체로 new 로 직접 생성하여 server.addsocket(ami_socket)으로 추가등록하고 epoll 도 직접 등록한다.
map<const char*, void*> g_process;	// ami event 중 처리하고자 하는 이벹에 대한 함수포인터를 등록해 둔다
int log_event_level = 0;				// 0이 아닌 값이 설정되면 event 명을 로깅한다

void* rm_ami_socket(tst::TST_USER_T* puser) {
	
	if (!puser || puser->s_len != (sizeof(TST_USER) + sizeof(AMI_MANAGE)))
		return NULL;

	PAMI_MANAGE ami_manage = (PAMI_MANAGE)puser->s;
	ami_manage->destroy();
	return NULL;
}

const char* get_amivalue(AMI_EVENTS& events, const char* key)
{
	int i;
	for (i = 0; i < events.rec_count; i++) {
		if (!strcmp(events.key[i], key))
			return events.value[i];
	}
	return "";
}

void logging_events(AMI_EVENTS& events)
{
	int i, len = 0;
	char msg[2048];
	for (i = 0; i < events.rec_count; i++) {
		len += sprintf(msg + len, "%s%s: %s\n", i ? "    " : "", events.key[i], events.value[i]);
	}
	conft("--- %s(atp threadno:%d)...\n%s\n", __func__, events.nThreadNo, msg);
}

int parse_amievent(AMI_EVENTS& events) {
	// parsing
	char* p = events.event;
	char* start = events.event;
	bool isHeader = true;

	for (events.rec_count = 0; *p; p++) {
		if (isHeader) {
			if (*p == ':') {
				*p = '\0';
				events.key[events.rec_count] = start;
				isHeader = false;
				start = NULL;
			}
		} else {
			if (!start && *p != ' ')
				start = p;
			if (*p == '\r' && *(p+1) == '\n') {
				*p++ = '\0';
				*p++ = '\0';
				if (start) {
					events.value[events.rec_count++] = start;
				}
				isHeader = true;
				start = p;
			}
		}
	}
	return events.rec_count;
}

PAMI_RESPONSE AMI_MANAGE::ami_sync(char* action, bool logprint)
{
	PAMI_RESPONSE pResponse = new AMI_RESPONSE;

	if (!ami_socket || ami_socket->type != sock_client) {
		pResponse->result = -99;
		strcpy(pResponse->msg, "AMI서버에 연결되지 않음");
		return pResponse;
	}

	TST_DATA& sdata = *ami_socket->send;
	ami_lock();
	resp_lock();

	sdata.req_len = sprintf(sdata.s,
		"%s"
		"ActionID: %d\n"
		"\n"
		, action
		, ++actionid
	);
	conft("ami_sync action ---------\n%s", sdata.s);
	struct timespec waittime;
	clock_gettime(CLOCK_REALTIME, &waittime);	// NTP영향받아야 함
	waittime.tv_sec += 5;
	waittime.tv_nsec = 0;
	
	pResponse->mode = action_requst;
	// ami_event() 함수는 AMI_MANAGE::pResp 값이 설정 되면 response 를 대기하는 프로세스가 있다고 인식한다.
	pResp = pResponse; // ami action에 대한 response를 받으면 ami_event() 함수가 널로 설절정해 준다.
	write(ami_socket->sd, sdata.s, sdata.req_len);
	int rc = pthread_cond_timedwait(&condResp, &mutexResp, &waittime);
	if (!rc) {
		// 정상처리
		if (logprint) {
			int i, len = 0;
			char msg[2048];
			for (i = 0; i < pResponse->responses.rec_count; i++) {
				len += sprintf(msg + len, "%s%s: %s\n", i ? "    " : "", pResponse->responses.key[i], pResponse->responses.value[i]);
			}
			conft("ami_sync response ---------\n%s", msg);
		}
	}
	else {
		if (rc == ETIMEDOUT) {
			//printf("ami action setvar error... 서버가 응답을 하지 않음\n");
			pResponse->result = -8;
			strcpy(pResponse->msg, "ami action error... 서버가 응답을 하지 않음");
		} else {
			pResponse->result = errno;
			snprintf(pResponse->msg, sizeof(pResponse->msg)-1, "ami action error... %s",strerror(errno));
		}
	}

	resp_unlock();
	ami_unlock();

	return pResponse;
}

void AMI_MANAGE::ami_async(char* action) {
	if (!ami_socket || ami_socket->type != sock_client)
		return;

	ami_lock();
	TST_DATA& sdata = *ami_socket->send;
	sdata.req_len = sprintf(sdata.s,
		"%s"
		"ActionID: %d\n"
		"\n"
		, action
		, ++actionid
	);
	conft("ami_async action ---------\n%s", sdata.s);
	write(ami_socket->sd, sdata.s, sdata.req_len);
	ami_unlock();
}

// AMI 에 연결된 소켓에서 데이타가 들어오면 무조건 이 사용자함수(ami_event())가 호출된다
TST_STAT ami_event(PTST_SOCKET psocket) {

	if (!psocket || !psocket->recv) {
		return tst_disconnect;
	}

	// AMI_MANAGE::alloc() 에서 ami_base 를 설정한다
	if (!psocket->user_data || psocket->user_data->type != ami_base) { 
		
		// 이건 잘못 들어온거다 상위의 http 함수에서 처리해라
		return tst_run;
	}

	if (log_event_level > 1) {
		conft("--- enter %s()", __func__);
	}

	// 이하는 ami_message 만 이리 온다고 가정하고 함수 작성한다.
	TST_DATA& rdata = *psocket->recv;
	TST_DATA& sdata = *psocket->send;
	TST_USER& udata = *(PTST_USER)psocket->user_data;
	AMI_MANAGE& manage = *(PAMI_MANAGE)udata.s;
	int remain;
	if (psocket->events & EPOLLIN) {
		remain = rdata.s_len - rdata.com_len - 1; // 현재 남은 공간, -1은 마지막문자로 널스트링을 넣기 위하여
		if (remain > (int)rdata.checked_len)
			remain = rdata.checked_len;

		clock_gettime(CLOCK_REALTIME, &rdata.trans_time);

		// login 외의 모든 메시지는 "\r\n" 이 두 개로 구분된다
		while (remain-- > 0
			&& ( rdata.com_len < 4
				|| rdata.s[rdata.com_len - 4] != '\r'
				|| rdata.s[rdata.com_len - 3] != '\n'
				|| rdata.s[rdata.com_len - 2] != '\r'
				|| rdata.s[rdata.com_len - 1] != '\n'
				)
			) {
			rdata.com_len += read(psocket->sd, rdata.s + rdata.com_len, 1);
		}


		if (rdata.com_len < 4
			|| rdata.s[rdata.com_len - 4] != '\r'
			|| rdata.s[rdata.com_len - 3] != '\n'
			|| rdata.s[rdata.com_len - 2] != '\r'
			|| rdata.s[rdata.com_len - 1] != '\n') {
			return tst_suspend;
		}

		rdata.s[rdata.com_len] = '\0';
		// TRACE("%s", rdata.s);

		// event or response data processing
		if (manage.pResp && !strncmp(rdata.s, "Response:", 9)) {
			if (manage.resp_waitcount && manage.pResp->mode == action_requst) {
				manage.resp_lock();
				AMI_RESPONSE& resp = *manage.pResp;

				strncpy(resp.responses.event, rdata.s, sizeof(resp.responses.event) - 1);
				parse_amievent(resp.responses);


				uint32_t curr_id = atoi(get_amivalue(resp.responses, "ActionID")); // response의 action id

				if (curr_id < manage.actionid) {
					// 이전 액션의 결과면 패스... 다음 것을 기다려야 한다
					manage.resp_unlock();
					// Response: 이벤트를 처리하는 핸들러가 있다면 호출해 주여야 한다...
					goto check_ami_event_handler;
				}
				else if (curr_id > manage.actionid) {
					// 이후 액션의 결과면 오류 처리
					resp.mode = action_response;
					resp.result = -2;
					sprintf(resp.msg, "action id is past...");
					// bzero(&resp.responses, sizeof(resp.responses));
				}
				else {
					resp.mode = action_response;
					if (!strncmp(resp.responses.value[0], "Success", 7)) {
						// 응답 성공
						resp.result = 0;
					}
					else {
						// 응답실패 
						resp.result = -1;
					}
					// set "Message: Authentication accepted" to manage.msg
					strncpy(resp.msg, get_amivalue(resp.responses, "Message"), sizeof(resp.msg) - 1);
				}
				manage.pResp = NULL;
				// 싱크로 동작하는 ami sction 요청이 있다. 통보해 주자
				pthread_cond_signal(&manage.condResp);
				manage.resp_unlock();
				rdata.reset_data();
				return tst_suspend;
			} else {
				// AMI_MANAGE::ami_sync() 함수 사용이 잘목 되었다 확인하라!!!!!
				// AMI_MANAGE::ami_sync() 함수를 잘못 수정하면 이리올 수도 있다....
				manage.pResp = NULL;
				conft("AMI_MANAGE::ami_sync() 함수를 잘못 수정한것 같다. 확인하시라!");
			}
		}

check_ami_event_handler:
		char* ptr = strchr(rdata.s, ':');
		if (*ptr && *++ptr == ' ') {
			++ptr;
			int len = strcspn(ptr, "\r\n") + 1;
			if (len < 1) len = 1;
			char ev_id[len];
			bzero(ev_id, len);
			strncpy(ev_id, ptr, len - 1);

			if (log_event_level) {
				AMI_EVENTS log;
				strncpy(log.event, rdata.s, rdata.com_len);
				parse_amievent(log);
				if (log_event_level > 2) {
					logging_events(log);
				} else {
					conft("--- event --- %s --- %s", get_amivalue(log, "Channel"), ev_id);
				}
			}
			map<const char*, void*>::iterator it;
			for (it = g_process.begin(); it != g_process.end(); it++) {
				if (!strcmp(it->first, ev_id)) {
					// 이벤트에 대한 처리를 여기서 직접하지 않고 AsyncThreadPool 로 넘기는 이유는
					// 이 함수가 종료되어야 다음 이벤트를 가져오러 가기 때문이다
					// 등록된 이벤트가 있는 것만 ASyncThreadPool 을 호출하자....
					PATP_DATA atpdata = atp_alloc(sizeof(AMI_EVENTS));
					AMI_EVENTS& events = *(PAMI_EVENTS)&atpdata->s;
					strncpy(events.event, rdata.s, rdata.com_len);
					parse_amievent(events);
					// 이벤트 처리
					atpdata->func = process_events;
					atp_addQueue(atpdata);
					// TRACE("REQUEST ami events(%s)...\n", events.value[0]);
					break;
				}
			}
		}
	} else if (psocket->events & EPOLLOUT) {
		clock_gettime(CLOCK_REALTIME, &sdata.trans_time);
		conft("---send remain(%d).....", psocket->send->checked_len);
	} else {
		if (psocket->events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
			// 연결이 끊어졌다.
			conft("---흐미 AMI 끊어졌다.....");
			ami_socket = NULL;
		}

	}

	rdata.reset_data();
	return tst_suspend;
}

TST_STAT my_disconnected(PTST_SOCKET psocket) {
	if (!psocket)
		return tst_suspend;

	if (psocket == ami_socket) {
		if (psocket->user_data->type == ami_base) {
			conft("--- 흐미 ami 끊어졌다.....");
			ami_socket = NULL;
		}
	}
	else {
#ifdef DEBUG
		conft("--- disconnected client socket....%s(%s:%d)", __func__, inet_ntoa(psocket->client.sin_addr), ntohs(psocket->client.sin_port));
#endif
	}

	return tst_suspend;
}

ATP_STAT atpfunc(PATP_DATA atpdata)
{
	ATP_STAT next = stat_suspend;

	if (atpdata->func) {
		next = atpdata->func(atpdata);
	}
	return next;
}

ATP_STAT process_events(PATP_DATA atp_data)
{
	ATP_STAT next = stat_suspend;

	AMI_EVENTS& events = *(PAMI_EVENTS)&atp_data->s;
	events.nThreadNo = atp_data->threadNo;

	// 등록된 이벤트를 처리한다.

	map<const char*, void*>::iterator it;
	for (it = g_process.begin(); it != g_process.end(); it++) {
		if (!strcmp(it->first, events.value[0])) {
			eventproc* func = (eventproc*)it->second;
			next = func(events);
			break;
		}
	}

	return next;
}


