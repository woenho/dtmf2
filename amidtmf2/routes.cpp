#include "amiaction.h"
#include "http.h"

TST_STAT http_dtmf(PTST_SOCKET psocket)
{
	REQUEST_INFO& req = *(PREQUEST_INFO)psocket->user_data->s;
	// TST_DATA& rdata = *psocket->recv;	// 수신버퍼
	TST_DATA& sdata = *psocket->send;	// 발신버퍼

	// decoding 된 쿼리스트링도 로깅하자
	conft("%s(), querystr=%s:", __func__, req.query_string);

	CQueryString qs(req.query_string, CQueryString::ALL);

	if (*qs.Get("caller") && *qs.Get("dtmf")) {
		const char* caller = qs.Get("caller");
		const char* dir = qs.Get("dir");
		const char* dtmf = qs.Get("dtmf");
		PAMI_RESPONSE resp = amiSendDtmf(caller, *dir ? dir : "callee", dtmf);
		sdata.com_len = sprintf(sdata.s,
			"HTTP/%s %d %s\r\n"
			// "Content-Type: text/html;charset=utf-8\r\n"
			"Connection: close\r\n\r\n"
			, req.http_version
			, resp->result
			, resp->msg
		);
		free(resp);
	} else {
		sdata.com_len = sprintf(sdata.s,
			"HTTP/%s %d %s\r\n\r\n"		// 400는 헤더를 첨부하지 않는다. 이후 모든 것은 body로 처리된다
			// "Content-Type: text/html;charset=utf-8\r\n"
			// "Connection: close\r\n\r\n"
			, req.http_version
			, 400
			, "request error"
		);
	}

	write(psocket->sd, sdata.s, sdata.com_len);
	clock_gettime(CLOCK_REALTIME, &sdata.trans_time);

	return tst_disconnect;
}





TST_STAT http_transfer(PTST_SOCKET psocket)
{
	REQUEST_INFO& req = *(PREQUEST_INFO)psocket->user_data->s;
	// TST_DATA& rdata = *psocket->recv;	// 수신버퍼
	TST_DATA& sdata = *psocket->send;	// 발신버퍼

	// decoding 된 쿼리스트링도 로깅하자
	conft("%s(), querystr=%s:", __func__, req.query_string);

	CQueryString qs(req.query_string, CQueryString::ALL);

	if (*qs.Get("caller") && *qs.Get("callee")) {
		const char* caller = qs.Get("caller");
		const char* callee = qs.Get("callee");
		const char* header = qs.Get("header");
		PAMI_RESPONSE resp = amiBlindTransfer(caller, callee, header);
		sdata.com_len = sprintf(sdata.s,
			"HTTP/%s %d %s\r\n"
			// "Content-Type: text/html;charset=utf-8\r\n"
			"Connection: close\r\n\r\n"
			, req.http_version
			, resp->result
			, resp->msg
		);
		free(resp);
	} else {
		sdata.com_len = sprintf(sdata.s,
			"HTTP/%s %d %s\r\n\r\n"		// 400는 헤더를 첨부하지 않는다. 이후 모든 것은 body로 처리된다
			// "Content-Type: text/html;charset=utf-8\r\n"
			// "Connection: close\r\n\r\n"
			, req.http_version
			, 400
			, "request error"
		);
	}

	write(psocket->sd, sdata.s, sdata.com_len);
	clock_gettime(CLOCK_REALTIME, &sdata.trans_time);

	return tst_disconnect;
}

TST_STAT http_alive(PTST_SOCKET psocket)
{
	REQUEST_INFO& req = *(PREQUEST_INFO)psocket->user_data->s;
	// TST_DATA& rdata = *psocket->recv;	// 수신버퍼
	TST_DATA& sdata = *psocket->send;	// 발신버퍼

	// conft("%s(), querystr=%s:", __func__, req.query_string);

	CQueryString qs(req.query_string, CQueryString::ALL);
	int nStatus = 200;

	if (*qs.Get("id") && *qs.Get("check")) {
		const char* id = qs.Get("id");
		const char* check = qs.Get("check");
		int id_len = strlen(id);
		int check_len = strlen(check);
		if (id_len < 5 || check_len < 5 || id[1] != check[4] || id[3] != check[0]) {
			nStatus = 400;
		}
		sdata.com_len = sprintf(sdata.s,
			"HTTP/%s %d %s\r\n"
			// "Content-Type: text/html;charset=utf-8\r\n"
			"Connection: keep\r\n\r\n"
			, req.http_version
			, nStatus
			, nStatus == 200 ? "OK" :"FAILED"
		);
	} else {
		nStatus = 400;
		sdata.com_len = sprintf(sdata.s,
			"HTTP/%s %d %s\r\n\r\n"			// 400는 헤더를 첨부하지 않는다. 이후 모든 것은 body로 처리된다
			// "Content-Type: text/html;charset=utf-8\r\n"
			// "Connection: keep\r\n\r\n"
			, req.http_version
			, nStatus
			, "FAILED"
		);
	}

	write(psocket->sd, sdata.s, sdata.com_len);
	clock_gettime(CLOCK_REALTIME, &sdata.trans_time);

	return nStatus == 200 ? tst_suspend : tst_disconnect;
}

