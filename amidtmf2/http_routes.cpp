#include "amiaction.h"
#include "http.h"

TST_STAT http_dtmf(PTST_SOCKET psocket)
{
	REQUEST_INFO& req = *(PREQUEST_INFO)psocket->user_data->s;
	// TST_DATA& rdata = *psocket->recv;	// 수신버퍼
	// TST_DATA& sdata = *psocket->send;	// 발신버퍼
	RESPONSE_INFO resp;

	// decoding 된 쿼리스트링도 로깅하자
	conft("%s(), querystr=%s:", __func__, req.query_string);

	CQueryString qs(req.query_string, CQueryString::ALL);

	if (*qs.Get("caller") && *qs.Get("dtmf")) {
		const char* caller = qs.Get("caller");
		const char* dir = qs.Get("dir");
		const char* dtmf = qs.Get("dtmf");
		PAMI_RESPONSE rc = amiSendDtmf(caller, *dir ? dir : "callee", dtmf);
		resp.http_version = req.http_version;
		resp.http_code = rc->result == 0 ? 200 : rc->result;
		resp.response_text = rc->msg;
		resp.html_text = resp.response_text;
		response_http(psocket, &resp);
		
		free(rc);
	} else {
		resp.http_version = req.http_version;
		resp.http_code = 400;
		resp.response_text = "syntax error";
		resp.html_text = resp.response_text;
		response_http(psocket, &resp);
	}

	return tst_disconnect;
}





TST_STAT http_transfer(PTST_SOCKET psocket)
{
	REQUEST_INFO& req = *(PREQUEST_INFO)psocket->user_data->s;
	// TST_DATA& rdata = *psocket->recv;	// 수신버퍼
	// TST_DATA& sdata = *psocket->send;	// 발신버퍼
	RESPONSE_INFO resp;

	// decoding 된 쿼리스트링도 로깅하자
	conft("%s(), querystr=%s:", __func__, req.query_string);

	CQueryString qs(req.query_string, CQueryString::ALL);

	if (*qs.Get("caller") && *qs.Get("callee")) {
		const char* caller = qs.Get("caller");
		const char* callee = qs.Get("callee");
		const char* header = qs.Get("header");
		PAMI_RESPONSE rc = amiBlindTransfer(caller, callee, header);
		resp.http_version = req.http_version;
		resp.http_code = rc->result == 0 ? 200 : rc->result;
		resp.response_text = rc->msg;
		resp.html_text = resp.response_text;
		response_http(psocket, &resp);

		free(rc);
	}
	else {
		resp.http_version = req.http_version;
		resp.http_code = 400;
		resp.response_text = "syntax error";
		resp.html_text = resp.response_text;
		response_http(psocket, &resp);
	}

	return tst_disconnect;
}

TST_STAT http_alive(PTST_SOCKET psocket)
{
	REQUEST_INFO& req = *(PREQUEST_INFO)psocket->user_data->s;
	// TST_DATA& rdata = *psocket->recv;	// 수신버퍼
	// TST_DATA& sdata = *psocket->send;	// 발신버퍼
	RESPONSE_INFO resp;

	// conft("%s(), querystr=%s:", __func__, req.query_string);

	CQueryString qs(req.query_string, CQueryString::ALL);
	resp.http_version = req.http_version;
	resp.http_code = 200;
	resp.response_text = "OK";
	if (*qs.Get("id") && *qs.Get("check")) {
		const char* id = qs.Get("id");
		const char* check = qs.Get("check");
		int id_len = strlen(id);
		int check_len = strlen(check);
		if (id_len < 5 || check_len < 5 || id[1] != check[4] || id[3] != check[0]) {
			resp.http_code = 404;
			resp.response_text = "syntax error";
			resp.html_text = resp.response_text;
		}
		response_http(psocket, &resp);
	} else {
		resp.http_code = 404;
		resp.response_text = "Not found";
		resp.html_text = resp.response_text;
		response_http(psocket, &resp);
	}

	return resp.http_code == 200 ? tst_suspend : tst_disconnect;
}

