/*
* websocket 수신에있어서 코어모듈이다. 수정에 신중함이 필요하다.
* 소켓을 통하여 들어온 HTTP메시지를 http.cpp 파일에서 websocket 프로토콜로 업그레이드 한 후에 호출된다
* 이후에는 http.cpp 에서 처리하지 않고 바로 websocket() 함수가 호출된다.
* 기본적으로 모든 메시지의 패킷을 수신한 후에 g_websocket 에 등록한 사용자 함수를 호출해 준다.
* 이 사용자 함수들은 ws_routes.cpp에 등록하도록 한다.
* 웹소켓 최초 호출시 하용된 route 정보를 이용하여 호출되는 사용자 함수가 결정된다.
* 이후 해당 웹소켓데이타는 모두 해당 사용자 함수에서 처리된다.
*/

#include "websocket.h"
#include "util.h"

extern int log_event_level;

map<const char*, void*> g_websocket;

void ws_writeping(PTST_SOCKET psocket) {
	char buf[4];
	buf[0] = 0x80 /*0x80은 FIN설정용*/ | (0x0f & MG_WEBSOCKET_OPCODE_PING);
	buf[1] = 0; // 연결해제 상태코드로 이건 4바이트다
	write(psocket->sd, buf, 2);
}

void ws_writepong(PTST_SOCKET psocket) {
	char buf[4];
	buf[0] = 0x80 /*0x80은 FIN설정용*/ | (0x0f & MG_WEBSOCKET_OPCODE_PONG);
	buf[1] = 0; // 연결해제 상태코드로 이건 4바이트다
	write(psocket->sd, buf, 2);
}

void ws_writedisconnect(PTST_SOCKET psocket, uint16_t status) {
	char buf[4];
	int req_len = 2; // 1002 상태코드 -> 프로토콜 오류
	buf[0] = 0x80 /*0x80은 FIN설정용*/ | (0x0f & MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE);
	buf[1] = (unsigned char)req_len; // 연결해제 상태코드로 이건 4바이트다
	uint16_t* scode = (uint16_t*)&buf[2];
	*scode = htons(1000); // status 1002 하니까 포스트맨이 프로토콜오류처리하네.. 정상종료처리로 변경
	write(psocket->sd, buf, 4);
}

int ws_writetext(PTST_SOCKET psocket, const char* text) {
	char buf[10];

	if (!text)
		return -1;

	TRACE("--ws- send\n%s\n", text);

	size_t req_len = strlen(text);
	buf[0] = 0x80 /*0x80은 FIN설정용*/ | (0x0f & MG_WEBSOCKET_OPCODE_TEXT);

	if (req_len < 126) {
		buf[1] = (unsigned char)req_len;
		write(psocket->sd, buf, 2);
	} else if (req_len == 126) {
		buf[1] = (unsigned char)126;
		uint16_t* plen = (uint16_t*)&buf[2];
		*plen = htons(req_len);
		write(psocket->sd, buf, 4);
	} else {
		buf[1] = (unsigned char)127;
		uint64_t* plen = (uint64_t*)&buf[2];
		*plen = req_len;
		write(psocket->sd, buf, 10);
	}
	
	return write(psocket->sd, text, req_len);
}


TST_STAT websocket(PTST_SOCKET psocket) {

	if (!psocket || !psocket->recv) {
		return tst_disconnect;
	}

	// 처음 연결된 것인가?
	if (psocket->type == sock_sub) {
		TRACE("--ws- connected\n");
		psocket->type = sock_websocket;
		//data_len = sprintf(data, "{\"code\":%03d, \"msg\":\"%s\"}", 200, "ok. I will disconnect websocket session");
		// 처음 연결되었을 때  하고 싶은것이 있다면.....
		// HTTP에서 설정한 user_data를 없애고 websocket 구조체를 할당한다
		REQUEST_INFO& req = *(PREQUEST_INFO)psocket->user_data->s;
		
		PTST_USER puser = WS_INFO::alloc();
		WS_INFO& ws = *(PWS_INFO)puser->s;

		// g_websocket 에 설정된 라우팅 정보가 있다면 해당 라우팅 정보를 설정한다
		map<const char*, void*>::iterator it;
		for (it = g_websocket.begin(); it != g_websocket.end(); it++) {
			// printf(":%s: http func address -> %lX\n", it->first, ADDRESS(it->second));
			if (!strcmp(req.request_uri, it->first)) {
				ws.websocket_func = (tstFunction)it->second;
				TRACE("--ws- ws routes-> %s\n", req.request_uri);
				break;
			}
		}

		TST_STAT next = tst_suspend;

		// 라우팅 정보가 지정되지 않은 uri 에 대한 처리는 연결해제이다
		if (!ws.websocket_func) {
			TST_DATA& sdata = *psocket->send;	// 발신버퍼
			sprintf(sdata.s, "요청한 uri '%s' 는 지원하지 않는 라우팅 정보입니다.", req.request_uri);
			ws_writetext(psocket, sdata.s);
			ws_writedisconnect(psocket, 1000);

			next = tst_disconnect;
		}

		// 바로 위에서 req 를 사용하므로 req 사용 종료 후 메모리를 해제한다
		free(psocket->user_data);
		// 이것은 tstpool class 가 알아서 메모리 해제해 준다
		psocket->user_data = puser;

		return next;
	}

	// websocket 클라이언트가 서버로 데이타를 보낼 때는 무조건 암호화한다
	// 요거 websocket 프로토콜을 알아야 코딩된다
	// https://tools.ietf.org/id/draft-ietf-hybi-thewebsocketprotocol-09.html#rfc.section.1.3
	// 위의 RFC 문서를 참조하여 작성했다.

#if 0
	0               1               2               3
	 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
	+-+-+-+-+-------+-+-------------+-------------------------------+
	|F|R|R|R| opcode|M| Payload len | Extended payload length       |
	|I|S|S|S| (4)   |A|     (7)     | (16 / 63)                     |
	|N|V|V|V|       |S|             | (if payload len == 126 / 127) |
	| |1|2|3|       |K|             |                               |
	+-+-+-+-+-------+-+-------------+-------------------------------+
	| Extended payload length continued, if payload len == 127      |
	+-------------------------------+-------------------------------+
	| Extended payload length cont~~| Masking-key, if MASK set to 1 |
	+-------------------------------+ ------------------------------+
	| Masking - key(continued)      | Payload Data  ~~~~            |
	+---------------------------------------------------------------+

	1. 첫번째 바이트
	1.1.opcode : x1, x2, x8, x9, xA == = > 4비트값 중 5가지만 사용
	{ 이 비트는 오른쪽(여덟번째비트)이 설정되면 x1, 왼쪽(다섯째비트)이 설정되면 x8
	   % x0 denotes a continuation frame
	   % x1 denotes a text frame
	   % x2 denotes a binary frame
	   % x3 - 7 are reserved for further non - control frames
	   % x8 denotes a connection close
	   % x9 denotes a ping
	   % xA denotes a pong
	   % xB - F are reserved for further control frames
	}
	2. 두번째 바이트
	2.1 MASK : 설정되면 암호화 되어 있다(암호화 키는 길이필드 다음 바이트부터 무조건 4바이트)
		클라이언트에서 서버로 송신되는 모든 데이타는 반드시 암호화 한다
	2.2 Payload len : 이 값에 따라 길이를 나타내는 방법이 다르다
	{
		Payload len == 0 ~125 : 마스킹키 다음 바이트부터 실린 데이타의 실제 길이
		Payload len == 126 : 세번째 바이트와 네번째 바이트의 값이 unsigned short 형식으로
							 마스킹키 다음 데이타구간에 실린 데이타의 실제 길이
		Payload len == 127 : 세번째 바이트 부터 열번째 바이트의 값이 unsigned 64int 형식으로
							 마스킹키 다음 데이타구간에 실린 데이타의 실제 길이
							 (단 첫번째 비트는 반드시 0이어야 한다.즉 63비트만 유효함)
							 이경우는 최대 길이를 제한할 필요가 있다.아니면 이 값에 다라 시스템이 오동작한다(메모리 확보실패.등등...)
	}
	3. 일단 길이값이 구해지면 해당 길이만큼의 데이타를 수신해야한다.
		수신이 완료되면 데이타 복호화작업을 한다.
	4. 데이타 암복호화 : 마스킹 값을 가지고 비트마스킹(XOR)하여 복호화작업을 한다
		데이타 첫번째 바이트와 마스킹 첫번째 바이트,
		데이타 두번째 바이트와 마스킹 두번째 바이트,
		데이타 세번째 바이트와 마스킹 세번째 바이트,
		데이타 네번째 바이트와 마스킹 네번째 바이트,
		데이타 다섯번째 바이트와 마스킹 첫번째 바이트,
		데이타 여섯번째 바이트와 마스킹 두번째 바이트....
		for (i = 0; i < (size_t)data_len; i++) {
			data[i] ^= mask[i & 3];
		}
	5. 프로토콜에 따르면 길이가 0이 설정 되어도 최소한 2바이트는 무조건 보낸다
		데이타가 있으면 데이터길이에 따라 마스킹키 및 데이터 시작 위치가 변경된다.
#endif




	// 이하는 메시지를 받는데 이어받기환경에 대한 이해가 필요하다

	PTST_DATA prdata = psocket->recv;
//	TST_DATA& sdata = *psocket->send;

	if (!psocket->user_data)
		return tst_disconnect;

	WS_INFO& wsinfo = *(PWS_INFO)psocket->user_data->s;

	TRACE("--ws- recv message %s\n", wsinfo.step ? "end" : "start");

	// 이미 기본 헤더를 받은 경우의 처리 ( 헤더를 받고 데이타가 커서 데이타 수신일수도 있다.)
	if (!wsinfo.step) {
		int m_len = 4;
		int m_pos = 0;
		prdata->com_len = read(psocket->sd, prdata->s, 2);
		wsinfo.opcode = prdata->s[0] & 0x0F;
		wsinfo.is_text = wsinfo.opcode == MG_WEBSOCKET_OPCODE_TEXT;
		wsinfo.is_bin = wsinfo.opcode == MG_WEBSOCKET_OPCODE_BINARY;
		wsinfo.is_close = wsinfo.opcode == MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE;
		wsinfo.is_ping = wsinfo.opcode == MG_WEBSOCKET_OPCODE_PING;
		wsinfo.is_pong = wsinfo.opcode == MG_WEBSOCKET_OPCODE_PONG;


		wsinfo.is_masked = prdata->s[1] & 0x80;
		wsinfo.data_len = prdata->s[1] & 0x7F; // 데이타길이 확인
		if (wsinfo.data_len < 126) {
			m_pos = 2;
			if (wsinfo.is_masked) {
				prdata->com_len += read(psocket->sd, prdata->s + prdata->com_len, m_len); // mask 길이 만큼 더 읽자
			}
		} else if (wsinfo.data_len == 126) {
			m_pos = 4;
			prdata->com_len += read(psocket->sd, prdata->s + prdata->com_len, 2); // data길이 정보 만큼 더 읽자
			wsinfo.data_len = ntohs(*(uint16_t*)(&prdata->s[2]));
			if (wsinfo.is_masked) {
				prdata->com_len += read(psocket->sd, prdata->s + prdata->com_len, m_len); // 마스크길이 만큼 더 읽자
			}
		} else {
			// wsinfo.data_len == 127
			m_pos = 10;
			prdata->com_len += read(psocket->sd, prdata->s + prdata->com_len, 8); // data길이 정보 만큼 더 읽자
			wsinfo.data_len = (((uint64_t)ntohl(*(uint32_t*)(&prdata->s[2]))) << 32) + ntohl(*(uint32_t*)(&prdata->s[6]));
			if (wsinfo.is_masked) {
				prdata->com_len += read(psocket->sd, prdata->s + prdata->com_len, m_len); // 마스크길이 만큼 더 읽자
			}

			//  1024* 1024 * 10 => 10485760
			if (wsinfo.data_len > 10485760) {
				// wsinfo.data_len > 10 MB 면 실패처리할까?
				ws_writedisconnect(psocket, 1004);
				return tst_disconnect;
			}
		}
		if (wsinfo.is_masked) {
			memcpy(wsinfo.mask, &prdata->s[m_pos], m_len);
		}
		wsinfo.step = 1;

		if (wsinfo.data_len > prdata->s_len) {
			// 메모리 확보 필요하다
			// 수신버퍼크기를 확장한다
			PTST_DATA new_ptr = (PTST_DATA)realloc(psocket->recv, sizeof(TST_DATA) + wsinfo.data_len + 1);
			if (!new_ptr) {
				// 메모리 확보 실패
				ws_writedisconnect(psocket, 1004);
				return tst_disconnect;
			}
			new_ptr->s_len = wsinfo.data_len;
			psocket->recv = new_ptr;
			prdata = psocket->recv;
		}
		// 읽기 락을 걸까?.......
		
		wsinfo.data = prdata->s;
		prdata->reset_data();

		prdata->com_len = read(psocket->sd, prdata->s, wsinfo.data_len);
		if (prdata->com_len < wsinfo.data_len) {
			// tstpoll 의 자동 받기 기능으로 나머지 데이타를 모두 받자
			prdata->req_pos = prdata->com_len;
			prdata->req_len = wsinfo.data_len - prdata->req_pos;
			prdata->com_len = 0;
			return tst_suspend; // 자동으로 이어받기 해 준다...
		}
	}

	// 모든 데이타 받기가 종료된 다음 복호화 작업을 한다
	if (wsinfo.is_masked) {
		size_t i;
		TRACE("--ws- completed websocket data length=%ld\n", wsinfo.data_len);
		for (i = 0; i < (size_t)wsinfo.data_len; i++) {
			wsinfo.data[i] ^= wsinfo.mask[i & 3];
		}
	}
	
	if (log_event_level > 1) {
		if (wsinfo.is_close)
			conft("ws-read\nopcode=%0.2X, len=%d, end_status=%d", wsinfo.opcode, wsinfo.data_len, ntohs(*(uint16_t*)wsinfo.data));
		else if (wsinfo.is_text)
			conft("ws-read\nopcode=%0.2X, len=%d msg=:%s:", wsinfo.opcode, wsinfo.data_len, wsinfo.data ? wsinfo.data : "");
		else
			conft("ws-read\nopcode=%0.2X, len=%d", wsinfo.opcode, wsinfo.data_len);
	}


	TST_STAT next = wsinfo.is_close ? tst_disconnect : tst_suspend;

	// 사용자 함수 호출
	if (wsinfo.websocket_func) {
		next = wsinfo.websocket_func(psocket);
		if (next != tst_run) {
			// 일반적인 경우라면 반드시 리셋 되어야 한다
			prdata->reset_data();
			bzero(psocket->user_data->s, psocket->user_data->s_len);
		}
		return next;
	}

	// 사용자함수가 지정되지 않은 라우팅에 대한 처리는?????





	// 다음데이타를 연이어 받지 않는 일반적인 경우라면 반드시 리셋 되어야 한다
	prdata->reset_data();
	bzero(psocket->user_data->s, psocket->user_data->s_len);


	return  next;
}