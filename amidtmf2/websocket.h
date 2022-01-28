
#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include "http.h"

/* New nomenclature */
enum {
	MG_WEBSOCKET_OPCODE_CONTINUATION = 0x0,
	MG_WEBSOCKET_OPCODE_TEXT = 0x1,
	MG_WEBSOCKET_OPCODE_BINARY = 0x2,
	MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE = 0x8,
	MG_WEBSOCKET_OPCODE_PING = 0x9,
	MG_WEBSOCKET_OPCODE_PONG = 0xa
};

typedef enum { ws_base = 100 } WebSocket_TYPE;

typedef struct WS_INFO_T {
	// ws 패킷 값
	char opcode;
	uint64_t data_len;
	char mask[4];
	char* data;
	// 상태값
	char step;	// 0: 헤더수신필요, 1: 헤더 분석완료( ws패킷값은 모두 설정 되었다) 데이타수신 필요, 2: 데이타수신 완료
	// 가공한 값
	char is_masked;
	char is_text;
	char is_bin;
	char is_close;
	char is_ping;
	char is_pong;
	// 웹소켓 데이타를 처리할 사용자 함수
	tstFunction websocket_func;

	// 고정된 길이의 WS_INFO_T 구조체이므로 그냥 두어도 소켓연결이 해제되면 tstpoll 클래스가 알아서 메모리 해제해 준다
	static PTST_USER alloc() {
		PTST_USER puser = (PTST_USER)calloc(sizeof(TST_USER) + sizeof(struct WS_INFO_T), 1);
		puser->s_len = sizeof(struct WS_INFO_T);
		puser->type = ws_base;
		return puser;
	}

} WS_INFO, *PWS_INFO;

extern map<const char*, void*> g_websocket;

void ws_writedisconnect(PTST_SOCKET psocket, uint16_t status);
int ws_writetext(PTST_SOCKET psocket, const char* text);
TST_STAT websocket_alive(PTST_SOCKET psocket);
TST_STAT websocket(PTST_SOCKET psocket);



#endif
