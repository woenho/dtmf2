
#ifndef _HTTP_H_
#define _HTTP_H_

#include "tst.h"
#include "AsyncThreadPool.h"

using namespace std;
using namespace tst;

typedef struct http_header {
	const char* name;  /* HTTP header name */
	const char* value; /* HTTP header value */
}HTTP_HEADER;

#define HTTP_MAX_HEADERS	30

typedef struct http_request_info {
	const char* request_method;		/* "GET", "POST", etc */
	char* request_uri;				/* URL-decoded URI (absolute or relative,* as in the request) */
	const char* query_string;		/* URL part after '?', not including '?', or NULL */
	const char* http_version;		/* E.g. "1.0", "1.1" */
	char* body_string;				/* post body or NULL */
	uint32_t content_length;		/* Length (in bytes) of the request body, can be -1 if no length was given. */
	int num_headers;				/* Number of HTTP headers */
	HTTP_HEADER http_headers[HTTP_MAX_HEADERS]; /* Allocate maximum headers */

	static PTST_USER alloc() {
		uint32_t s_len = sizeof(TST_USER) + sizeof(struct http_request_info);
		PTST_USER puser = (PTST_USER)calloc(s_len, 1);
		puser->s_len = s_len;
		return puser;
	}
}REQUEST_INFO, * PREQUEST_INFO;

typedef TST_STAT httpproc(PTST_SOCKET psocket);

extern map<const char*, void*> g_route;

TST_STAT http(PTST_SOCKET psocket);
// 이하는 routes.cpp 것
TST_STAT http_dtmf(PTST_SOCKET psocket);
TST_STAT http_transfer(PTST_SOCKET psocket);
TST_STAT http_alive(PTST_SOCKET psocket);


class CQueryString
{
public:

#define MAX_PARSE	128
#define ES_CR		0x01
#define ES_LF		0x02
#define ES_CRLF		(ES_CR|ES_LF )				// 0x03
#define ES_WS		0x04
#define ES_TAB		0x08
#define ES_WSTAB	(ES_WS|ES_TAB)				// 0x0C
#define ES_ALL		(ES_CR|ES_LF|ES_WS|ES_TAB)	// 0x0F

	typedef enum { NONE, CR = ES_CR, LF = ES_LF, CRLF = ES_CRLF, WS = ES_WS, TAB = ES_TAB, ALL = ES_ALL } CheckCRLF;

	std::map<std::string, std::string> m_mapQuery;

	CQueryString(const char* szQuery, CheckCRLF _m = NONE)
	{
		if (szQuery && *szQuery)
			Parse(szQuery, _m);
	}
	~CQueryString()
	{
		m_mapQuery.clear();
	}

	void inline CheckEscape(char* szString, CheckCRLF _m) {
		char* pChk = NULL;

		// websocket은 그냥 텍스트 데이타가 들어오면서 
		// '\r\n'이 마지막에 들어오는 경우 발생
		// none display라 헤깔릴 수 있음(postman 으로 시험시에 확인됨)
		// var1=data1&var2=data2 --> var1=data1\r\n&var2=data2\r\n으로 들어와도 처리
		// witespace는 ??

		for (pChk = szString; *pChk; pChk++) {
			if ((*pChk == ' ' && _m & ES_WS) || (*pChk == '\t' && _m & ES_TAB)) {
				// 모두 한자리 앞으로 이동
				for (char* p = pChk; *p; p++) {
					*p = *(p + 1);
				}
				pChk--;
			}
			else if ((*pChk == '\r' && _m & ES_CR) || (*pChk == '\n' && _m & ES_LF)) {
				*pChk = '\0';
			}
		}
	}

	int Parse(const char* szQuery, CheckCRLF _m = NONE)
	{
		char szName[MAX_PARSE];
		char szValue[MAX_PARSE];
		char* pNow;
		char* pSep;
		int nLen;
		char* querystr;
		if (szQuery && *szQuery)
			querystr = strdup(szQuery);
		else
			return 0;

		pNow = querystr;

#if defined(DEBUG)
		printf("CQueryString::Parse(), querystr=%s:\n", pNow);
#endif

		while (pNow && *pNow)
		{
			pSep = strchr(pNow, '=');
			if (!pSep || pSep == pNow)
				break;
			nLen = pSep - pNow;
			if (nLen >= MAX_PARSE)
				nLen = MAX_PARSE - 1;

			memcpy(szName, pNow, nLen);
			szName[nLen] = '\0';
			pNow = ++pSep;

			if (!*pNow)
				break;

			pSep = strchr(pNow, '&');
			if (pSep)
				nLen = pSep - pNow;
			else
				nLen = strlen(pNow);

			if (nLen >= MAX_PARSE)
				nLen = MAX_PARSE - 1;

			memcpy(szValue, pNow, nLen);
			szValue[nLen] = '\0';

			// 특수문자 제거 및 종단처리
			if (_m > NONE) {
				CheckEscape(szName, _m);
				CheckEscape(szValue, _m);
			}


			m_mapQuery[szName] = szValue;

			if (pSep)
				pNow = ++pSep;
			else
				break;
		}
		free(querystr);
		return m_mapQuery.size();
	}

	const char* Get(const char* szName)
	{
		std::map<std::string, std::string>::iterator it_map;
		it_map = m_mapQuery.find(szName);
		if (it_map == m_mapQuery.end())
			return "";

		return m_mapQuery[szName].c_str();
	}
};

#endif
