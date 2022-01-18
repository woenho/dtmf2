/* ---------------------
 * UTF-8 한글확인 용
 * ---------------------*/
 

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <stddef.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <assert.h>
#include <dirent.h>
#include <bits/siginfo.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/netfilter_ipv4.h>
#include <netinet/tcp.h>
#include <libmemcached/memcached.h>
#include <openssl/sha.h>
#include <iconv.h>

#include "WebConfig.h"
#include "logger.h"

#if 0
#define uint    unsigned int
#define ulong   unsigned long
#define uchar   unsigned char
#define ushort  unsigned short
#define u_int    unsigned int
#define u_long   unsigned long
#define u_char   unsigned char
#define u_short  unsigned short
#endif

#define	WRITE_LOG_OFLAG		(O_WRONLY|O_CREAT|O_APPEND)
#define	WRITE_LOG_MODE		(__S_IREAD | __S_IWRITE | S_IRGRP | S_IROTH)	//0644

#define DUMP_OFLAG			(O_WRONLY|O_CREAT|O_TRUNC)
#define DUMP_MODE			(0644)

extern CWebConfig g_cfg;

extern sigset_t g_sigsBlock;	///< 시그널 구조
extern sigset_t g_sigsOld;		///< 시그널 구조

#ifdef __cplusplus 
extern "C"
{
#endif

	char* getSHA256(const char* data);
	char* getSHA512(const char* data);

	char* load_file(char* path, size_t* load_len);
	char* get_auth(const char* szChannel, const char* key, char* path);
	char* get_memcached(const char* key);
	int set_memcached(const char* key, const char* value, time_t tts);
#if defined(USING_json_c)
	json_object* decode_json_resp(json_object* jobj, const char* _key, enum json_type _type);
#endif
	int signal_init(void (*sa)(int, siginfo_t*, void*), bool debug_sig_message);
	int	tcp_create(int port_no);
	int	tcp_connect(const char* host, int port_no, struct	sockaddr_in* paddr);
	int tcp_check_read(int sd);
	int tcp_check_write(int sd);
	int tcp_check_spaces(int sd);

	int	udp_create(int port_no);
	int udp_set_server(struct sockaddr_in* pServerAddr, const char* szUDPServerAddress, u_short nUDPServerPort);
	int udp_send(int _logPort, struct sockaddr_in* pServerAddr, const char* szLog);

	int mk_int(char* data, int nLen);
	char* mk_char(char* data, int nData, int nLen);
	int mk_hex(char* data, int nLen);
	char* mk_hexchar(char* data, int nData, int nLen);
	char* move_str(char* dest, char* src, int nLen);

	char* trim(char* str);
	char* rtrim(char* str);
	char* ltrim(char* str);

	void erase_tcp_buffer(int sd, char* buf, int size);
	bool isNumeric(const char* data, int nLen);
	char* get_messageid();
	char* get_datetime();
	int get_datetime_buf(char* time_buffer);

	int euckr2utf8(char* source, char* dest, int dest_size);	
	int utf82euckr(char* source, char* dest, int dest_size);

	// 걍 CivetServer::urlDecode() 사용하자
	char* urlDecodeRewite(char* str);
	char* urlDecodeNewString(const char* str);

#define IP_ADDR_STR_LEN (50) /* IPv6 hex string is 46 chars */
	// src_addr 은 반드시 IP_ADDR_STR_LEN 이상 길이읠 문자열 공간을 확보한후 호출 바람
	extern char* mg_get_client_addr(const struct mg_connection* conn, char* src_addr);

#define IP_ADDR_PORT_STR_LEN (60) /* IPv6 hex string is 46 chars */
	// src_addr 은 반드시 IP_ADDR_STR_LEN+10 이상 길이읠 문자열 공간을 확보한후 호출 바람
	extern char* mg_get_client_addrport(const struct mg_connection* conn, char* src_addrport);

#ifdef __cplusplus 
}
#endif

#ifdef __cplusplus 
using namespace std;
class util_exception : public exception {
	int m_code;
	string m_message;
public: 
	util_exception(string _m)  : m_code(0), m_message("util_exception : " + _m) {}
	util_exception(int _n, string _m) : m_code(_n), m_message("util_exception : " + _m) {}
	util_exception(int _n, const char* fmt, ...) : m_code(_n) {
		char log[4096];        // va_arg 텍스트 출력
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(log, sizeof(log), fmt, ap);
		va_end(ap);
		m_message = log;
	}
	virtual const char* what() { return m_message.c_str(); }
	int code() { return m_code; }
	virtual ~util_exception() _GLIBCXX_USE_NOEXCEPT {}; // 컴파일러애 따라서 ~exception이 순수가상이라서 반드시 _GLIBCXX_USE_NOEXCEPT {} 있어야 함
};
#endif

#if 0
extern int g_logPort;
extern struct sockaddr_in	g_LogAddr;

#define logging(...) \
	if( g_logPort < 0 ) \
	{	conft( __VA_ARGS__ ); }\
	else \
	{ \
		char szLogData[4096] = { 0, }; \
		time_t t; \
		struct tm lt; \
		time(&t); \
		localtime_r(&t,&lt); \
		int nLen = sprintf( szLogData,  "%02d:%02d:%02d [%s:%d] ", lt.tm_hour, lt.tm_min, lt.tm_sec, __FILE__, __LINE__); \
		snprintf( szLogData+nLen, 4096-nLen, __VA_ARGS__ ); \
		udp_send( g_logPort, &g_LogAddr, szLogData ); \
	}
#endif

#ifdef USE_DEBUG_MSG
#define debug(...) logging( __VA_ARGS__ )
#else
#define debug(...)
#endif

#define make_len(a)\
	{\
		char szLen[5];\
		char* p = (char*)a;\
		sprintf( szLen, "%04d", (int)strlen(p+8));\
		memcpy(p+4, szLen, 4);\
	}




#endif




