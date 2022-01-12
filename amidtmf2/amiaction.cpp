/*
* 이 파일은 ami action 요청단계에 있어서 실제 요정 문장을 만드는 함수들을 모아 놓는다
* AMI action 관련 유틸성격을 갖는 함수들의 모음이다
* ami처리 시 기본적으로 필요한 채널정보 관리에 사용되는 memcached 정보처리도 포함했다.
* amiLogin() 함수는 AsyncThreadPool의 인스턴스 생성시 지정한 atpfunc() 함수에서 바로 호출되는 서브합수이다.
* 나머지 ami action 함수들은 PAMI_RESPONSE 를 리턴하도록 한다.
* ami 명령 실행결과를 반드시 필요로 하지 않는 경우는 굳이 여기 추가하지말고 직접 AMI_MANAGE::ami_async() 호출을 사용하라
*/
#include "amiaction.h"

int set_callinfo(const char* caller, CallInfo_t* info)
{
	char buf[512];

	char* now = get_datetime();

	sprintf(buf, "W:%.1d:%s:%s:%s:%s:%s:%s:"
		, info->nWebLoaded
		, now
		, info->szExten
		, info->szChannel
		, info->szUniqueid
		, *info->szDestChannel ? info->szDestChannel : " "
		, *info->szDestUniqueid ? info->szDestUniqueid : " "
	);
	free(now);

	return set_memcached(caller, buf, 3600);

}

int get_callinfo(const char* caller, CallInfo_t* info)
{
	if (!info)
		return 1;

	char* data = get_memcached(caller);
	if (!data)
		return 2;

	// 첫 항목은 'W:' 이어야 amiweb에서 사용하는 콜러정보이다
	if (*data != 'W') {
		free(data);
		return 3;
	}

	char* webloaded = data + 2;
	char* time = NULL;
	char* exten = NULL;
	char* channel = NULL;
	char* uniqueid = NULL;
	char* destchannel = NULL;
	char* destuniqueid = NULL;
	char* p = data + 2;

	while (*p) {
		if (*p == ':') {
			*p = '\0';
			if (!time)
				time = p + 1;
			else if (!exten)
				exten = p + 1;
			else if (!channel)
				channel = p + 1;
			else if (!uniqueid)
				uniqueid = p + 1;
			else if (!destchannel)
				destchannel = p + 1;
			else if (!destuniqueid)
				destuniqueid = p + 1;
		}
		p++;
	}

	info->nWebLoaded = *webloaded - '0';
	if (time) strncpy(info->szTime, time, sizeof(info->szTime) - 1);
	if (exten) strncpy(info->szExten, exten, sizeof(info->szExten) - 1);
	if (channel) strncpy(info->szChannel, channel, sizeof(info->szChannel) - 1);
	if (uniqueid) strncpy(info->szUniqueid, uniqueid, sizeof(info->szUniqueid) - 1);
	if (destchannel) strncpy(info->szDestChannel, destchannel, sizeof(info->szDestChannel) - 1);
	if (destuniqueid) strncpy(info->szDestUniqueid, destuniqueid, sizeof(info->szDestUniqueid) - 1);

	free(data);

	return 0;
}

ATP_STAT amiLogin(PATP_DATA atp_data)
{
	if (ami_socket)
		return stat_suspend;

	// 일단 로그인 프로세스가 시작되었다는 것을 알리기 위해서라도 객체만 생성한다
	uint32_t	buf_len = 4096;
	ami_socket = new TST_SOCKET(buf_len, buf_len);
	struct	sockaddr_in s_info;
	AMI_LOGIN& login = *(PAMI_LOGIN)&atp_data->s;

	int sd = server.tcp_connect(login.Host, login.Port, &s_info);
	if (sd < 1) {
		// 연결 실패
		conpt("--- ami tcp socket connect failed(%d:%s)\n", errno, strerror(errno));
		delete ami_socket;
		ami_socket = NULL;
		return stat_suspend;
	}

	struct timeval stTimeVal;
	fd_set rset;

	stTimeVal.tv_sec = 5;
	stTimeVal.tv_usec = 0;
	FD_ZERO(&rset);
	FD_SET(sd, &rset);
	
	// 5초간 입력을 기다린다
	int rc = select(sd+1, &rset, 0, 0, &stTimeVal);
	if (!rc) {
		conpt("ami 연결시간 5초 만료!!!!\n");
		delete ami_socket;
		ami_socket = NULL;
		return stat_suspend;
	}
	if (rc<0) {
		conpt("ami 연결된 소켓 오류\n");
		delete ami_socket;
		ami_socket = NULL;
		return stat_suspend;
	}

	ami_socket->sd = sd;
	ami_socket->type = sock_client;
	memcpy(&ami_socket->client, &s_info, sizeof(s_info));
	// 이 소켓은 AMI 소켓이고 이 소켓에 대이타가 들어오면 ami_event() 를 호출하라
	ami_socket->func = ami_event;
	ami_socket->user_data = AMI_MANAGE::alloc();

	AMI_MANAGE& manage = *(PAMI_MANAGE)ami_socket->user_data->s;	
	TST_DATA& rdata = *ami_socket->recv;
	TST_DATA& sdata = *ami_socket->send;
	int connected = 0;
	int retry = 100;

	while (connected < 2) {
		ioctl(sd, SIOCINQ, &rdata.checked_len);
		if (rdata.checked_len) {
			rdata.com_len += read(sd, rdata.s + rdata.com_len, rdata.checked_len);
			conpt("recv:\n%s", rdata.s);
		}

		if (rdata.com_len < 22) {
			if (!--retry) {
				conpt("연결된 소켓은 Asterisk AMI 가 아니다!(retry over)\n");
				delete ami_socket;
				ami_socket = NULL;
				return stat_suspend;
			}
			usleep(10000);
			continue;
		}

		if (!connected && strncmp(rdata.s, "Asterisk Call Manager/", 22)) {
			conpt("연결된 소켓은 Asterisk AMI 가 아니다!(no matching Asterisk Call Manager/)\n");
			delete ami_socket;
			ami_socket = NULL;
			return stat_suspend;
		}
		if (!connected && !strncmp(rdata.s, "Asterisk Call Manager/", 22)) {

			sdata.req_len = sprintf(sdata.s,
				"Action: Login\n"
				"Username: %s\n"
				"Secret: %s\n"
				"ActionID: %d\n\n"
				, login.Username
				, login.Secret
				, ++manage.actionid
			);
			sdata.com_len = write(sd, sdata.s, sdata.req_len);
			if (sdata.com_len != sdata.req_len) {
				conpt("연결된 AMI소켓에 쓰기 실패\n");
				delete ami_socket;
				ami_socket = NULL;
				return stat_suspend;
			}
			connected = 1;
			retry = 100;
			rdata.reset_data();
			continue;
		}

		// 연결되었는지 확인
		if (rdata.s[rdata.com_len - 2] == '\r' && rdata.s[rdata.com_len - 1] == '\n') {
			AMI_EVENTS events;
			strncpy(events.event, rdata.s, rdata.com_len);
			rdata.reset_data();

			parse_amievent(events);


			if (!events.rec_count) {
				conpt("연결된 소켓은 Asterisk AMI 가 아니다!(no parse_amievent)\n");
				delete ami_socket;
				ami_socket = NULL;
				return stat_suspend;
			}
			/*
			* Response: Success
			* ActionID: 1
			* Message: Authentication accepted
			*/
			if(!strncmp(events.key[0], "Response", 8)) {
				if (!strncmp(events.value[0], "Success", 7)) {
					connected = 2;
				}
				else {
					conpt("AMI login failed:%s\n", get_amivalue(events,"Message"));
					delete ami_socket;
					ami_socket = NULL;
					return stat_suspend;
				}
			}
		}
	}

	server.addsocket(ami_socket);

	// epoll에 등록
	struct epoll_event ev;
	memset(&ev, 0x00, sizeof(ev));
	ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
	ev.data.fd = sd;
	epoll_ctl(server.m_epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);

	// 혹시 벌써 들어온 문자가 있다면, 이벤트 타야한다... 이벤트 확인시키자.
	ioctl(sd, SIOCINQ, &ami_socket->recv->checked_len);
	conpt("--- ami in data check(%d)\n", ami_socket->recv->checked_len);

	return stat_suspend;
}


PAMI_RESPONSE amiDeviceStatus(const char* device)
{
	PAMI_RESPONSE resp = new AMI_RESPONSE;



	return resp;
}

PAMI_RESPONSE amiSendDtmf(const char* caller, const char* dir, const char* dtmf)
{
	PAMI_RESPONSE resp = NULL;

	CallInfo_t ci = { 0 };

	int nDir = strcmp(dir, "caller");
	if (nDir) {
		if (strcmp(dir, "callee")) {
			resp = new AMI_RESPONSE;
			resp->result = 400;
			sprintf(resp->msg, "dir is not corected...diir=%s", dir);
			return resp;
		}
	}

	if (get_callinfo(caller, &ci)) {
		conft("channel get not found(caller=%s)\n", caller);
		resp = new AMI_RESPONSE;
		resp->result = 404;
		sprintf(resp->msg, "channel get not found(caller=%s)\n", caller);
		return resp;
	}

	conft("/dtmf caller->%s, dtmf->%s, dir->%s, Channel->%s, DestChannel->%s\n", caller, dtmf, dir, ci.szChannel, ci.szDestChannel);

	AMI_MANAGE& manage = *(PAMI_MANAGE)ami_socket->user_data->s;

	int nIndx = 0;
	char buf[128];
	while (dtmf[nIndx]) {
		sprintf(buf,
			"Action: PlayDTMF\n"
			"Channel: %s\n"
			"Digit: %c\n"
			"Duration: 250\n"
			, nDir ? ci.szDestChannel : ci.szChannel
			, dtmf[nIndx]
		);
		if (resp) free(resp);
		resp = manage.ami_sync(buf);
		if (resp->result) {
			resp->result = 500;
			sprintf(resp->msg, "dtmf send fail:%s\n", get_amivalue(resp->responses, "Message"));
			return resp;
		}
		nIndx++;
		usleep(300000);// Duration 보다 크면 안정적으로 소리를 들을 수 있다. 컴퓨터적으로는 어쨌든 신호가 간다
	}

	if (resp) {
		resp->result = 200;
		strcpy(resp->msg, "OK");
	}
	else {
		resp = new AMI_RESPONSE;
		resp->result = 400;
		sprintf(resp->msg, "maybe dtmf is empty??");
	}
	return resp;
}

PAMI_RESPONSE amiBlindTransfer(const char* caller, const char* callee, const char* header)
{
	CallInfo_t ci = { 0 };
	PAMI_RESPONSE resp = NULL;

	if (get_callinfo(caller, &ci)) {
		conft("channel get not found(caller=%s)\n", caller);
		resp = new AMI_RESPONSE;
		resp->result = 404;
		sprintf(resp->msg, "channel get not found(caller=%s)\n", caller);
		return resp;
	}

	conft("/transfer caller->%s, channels->%s, callee=%s", caller, ci.szDestChannel, callee);

	AMI_MANAGE& manage = *(PAMI_MANAGE)ami_socket->user_data->s;

	char buf[2048];	// refer용 header 길이 제한필요
	if (header) {
		snprintf(buf, sizeof(buf) - 1,
			"Action: Setvar\n"
			"Channel: %s\n"
			"Variable: REFER_HEADER\n"
			"Value: %s\n"
			, ci.szChannel
			, header
		);
		resp = manage.ami_sync(buf);
		if (resp->result) {
			resp->result = 500;
			sprintf(resp->msg, "REFER_HEADER setvar fail:%s\n", get_amivalue(resp->responses, "Message"));
			return resp;
		}
		free(resp);
	}

	sprintf(buf,
		"Action: BlindTransfer\n"
		"Channel: %s\n"
		"Context: %s\n"
		"Exten: %s\n"
		, ci.szDestChannel
		, "TRANSFER"	// refer context
		, callee
	);
	resp = manage.ami_sync(buf);
	if (resp->result) {
		resp->result = 500;
		sprintf(resp->msg, "BlindTransfer fail:%s\n", get_amivalue(resp->responses, "Message"));
		return resp;
	}

	return resp;
}


