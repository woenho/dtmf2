#include "websocket.h"
#include "util.h"
#include "amiproc.h"

extern int log_event_level;

int g_processmon_sd = 0;

TST_STAT websocket_alive(PTST_SOCKET psocket)
{
	if (!psocket || !psocket->user_data)
		return tst_disconnect;

	WS_INFO& wsinfo = *(PWS_INFO)psocket->user_data->s;

	if (wsinfo.is_ping) {
		TRACE("--ws- recv ping opcode\n");
		ws_writepong(psocket);
		return tst_suspend;
	}

	if (wsinfo.is_pong) {
		TRACE("--ws- recv pong opcode\n");
		return tst_suspend;
	}

	if (wsinfo.is_close) {
		TRACE("--ws- recv disconnect opcode\n");
		return tst_disconnect;
	}

	if (wsinfo.is_bin) {
		TRACE("--ws- recv binary opcode\n");
		// 아직 지원하지 않는다
		ws_writetext(psocket, "opcode binary는 지원하지 않는다");
		return tst_suspend;
	}

	if (!wsinfo.is_text) {
		// 프로토콜 오류
		ws_writetext(psocket, "opcode 지정이 잘못 되었습니다.");
		return tst_disconnect;
	}

	TRACE("--ws- recv text opcode, len=%u:%s:\n", (uint32_t)wsinfo.data_len, wsinfo.data);

	// 들어온 데이타는 text 이다...
	if(wsinfo.data_len < 1) {
		// 프로토콜 오류
		ws_writetext(psocket, "text의 길이가 0입니다.");
		return tst_suspend;
	}


#if 0
	// websocket을 이용한 단순 응답테스트용 루틴
	// TST_DATA& rdata = *psocket->recv;
	TST_DATA& sdata = *psocket->send;
	snprintf(sdata.s, sdata.s_len - 1, "text의 길이가 %u입니다.", (uint32_t)wsinfo.data_len);
	ws_writetext(psocket, sdata.s);
	return tst_suspend;

#endif

	CQueryString qs(wsinfo.data, CQueryString::ALL);
	if (*qs.Get("id") && *qs.Get("check")) {
		const char* id = qs.Get("id");
		const char* check = qs.Get("check");
		int id_len = strlen(id);
		int check_len = strlen(check);
		if (id_len < 5 || check_len < 5 || id[1] != check[4] || id[3] != check[0]) {
			ws_writetext(psocket, "{\"code\": 400, \"msg\":\"keepalive check error... will be disconnected\"}");
			return tst_disconnect;
		}
		if (g_processmon_sd && g_processmon_sd != psocket->sd) {
			// 이미 등록된 프로세스모니터가 있다면 유효한지 확인한다.
			map<int, PTST_SOCKET>::iterator it = server.m_connect.find(g_processmon_sd);
			if (it != server.m_connect.end()) {
				ws_writetext(psocket, "{\"code\": 409, \"msg\":\"I have another processmon's connection.\"}");
				return tst_disconnect;
			}
		}
		ws_writetext(psocket, "{\"code\": 201, \"msg\":\"keepalive check ok.\"}");
		g_processmon_sd = psocket->sd;
	} else {
		ws_writetext(psocket, "{\"code\": 401, \"msg\":\"keepalive check error... will be disconnected\"}");
		return tst_disconnect;
	}


	return tst_suspend;
}



