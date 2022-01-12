/* ---------------------
 * UTF-8 한글확인 용
 * ---------------------*/
 

#include "util.h"

#define		MAX_RETRY			5
#define		BIND_SLEEP_TIME		1

CWebConfig g_cfg;

struct sockaddr_in	g_LogAddr;
int					g_logPort = -1;

sigset_t g_sigsBlock;	///< 시그널 구조
sigset_t g_sigsOld;		///< 시그널 구조

char* getSHA256(const char* data) {

    unsigned char digest[SHA256_DIGEST_LENGTH];
    size_t sha_len = SHA256_DIGEST_LENGTH * 2 + 1;
    SHA256((const unsigned char*)data, strlen(data), (unsigned char*)&digest);

    char* mdString = (char*)malloc(sha_len);
    bzero(mdString, sha_len);
    int i;
    for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(&mdString[i * 2], "%02x", (unsigned int)digest[i]);

    return mdString;
}

char* getSHA512(const char* data) {

	unsigned char digest[SHA512_DIGEST_LENGTH];
	size_t sha_len = SHA512_DIGEST_LENGTH * 2 + 1;
    SHA512((const unsigned char*)data, strlen(data), (unsigned char*)&digest);

	char* mdString = (char*)malloc(sha_len);
	bzero(mdString, sha_len);
    int i;
	for (i = 0; i < SHA512_DIGEST_LENGTH; i++)
		sprintf(&mdString[i * 2], "%02x", (unsigned int)digest[i]);

	return mdString;
}

char* get_auth(const char* szChannel, const char* key, char* path)
{
    size_t auth_len;
    char* auth = NULL;
    char buf[1024] = { 0 };

    auth = get_memcached(key);

    if (auth)
    {
#ifdef DEBUG_TRACE
        conft("[%s] key=%s, mem auth :%s:", szChannel, key, auth);
#endif
    }
    else
    {
        snprintf(buf, sizeof(buf), "export GOOGLE_APPLICATION_CREDENTIALS=/home/cqop/credentials/%s.json;gcloud auth application-default print-access-token > %s"
            , !strcmp(key, "gstt1") ? "vaiman-1-last"
            : !strcmp(key, "gdf")   ? "ars-ai-38229be70266"
            : !strcmp(key, "gtts")  ? "google-tts-f66a845f6f5a"
            : "ars-ai-469a4757828e"
            , path);
        int ret = system(buf); // -1: errmsg set, 127: error, other: 샐행된 명령이 exit(xxx) 로 리턴한 xxx 값
        if (ret != 0) {
            conft("[%s] error system(%s)", szChannel, buf);
            goto get_auth_exit;
        }
        auth = load_file(path, &auth_len);
        if (auth) {
#ifdef DEBUG
            conft("[%s] key=%s, file auth=%s :%s:", szChannel, key, path, auth);
#endif
        } else {
            conft("[%s] key=%s, Auth file open failed!", szChannel, key);
        }
		remove(path);// 생성된 크리덴셜 값은 임시값이므로 당근 삭제
	}

get_auth_exit:

    return auth;
}

char* get_memcached(const char* key)
{
    memcached_server_st* servers = NULL;
    memcached_st* memc;
    memcached_return rc;
    char* value = NULL;
	char* host = NULL;

    memc = memcached_create(NULL);
	host = strdup(g_cfg.Get("MEMCACHED", "host").c_str());
	int port = atoi(g_cfg.Get("MEMCACHED", "port").c_str());
#if defined DEBUG_TRACE
	// conft("MEMCACHED_HOST=%s, get key=%s", host, key);
	conft("get MEMCACHED_HOST=%s, port=%d key=%s", host, port, key);
#endif
    servers = memcached_server_list_append(servers, host?host:"localhost", port, &rc); // localhost 는 환경변수로 대체
    rc = memcached_server_push(memc, servers);

    if (rc == MEMCACHED_SUCCESS) {
#if defined DEBUG_TRACE
		conft("Added mamcached server successfully");
#endif
        uint32_t flags = 0;
        size_t value_length = 0;
        value = memcached_get(memc, key, strlen(key), &value_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) {
#if defined DEBUG_TRACE
			conft("Couldn't get key : %s", memcached_strerror(memc, rc));
#endif
        }
    }
    else
    {
        fprintf(stderr, "Couldn't add memcached server: %s\n", memcached_strerror(memc, rc));
    }

	memcached_server_list_free(servers);
    memcached_free(memc);

	if (host) free(host);

    return value;
}

int set_memcached(const char* key, const char* value, time_t tts)
{
	memcached_server_st* servers = NULL;
	memcached_st* memc;
	memcached_return rc;
	char* host = NULL;

	memc = memcached_create(NULL);
	host = strdup(g_cfg.Get("MEMCACHED","host").c_str());
	int port = atoi(g_cfg.Get("MEMCACHED", "port").c_str());
#if defined DEBUG_TRACE
//	conft("MEMCACHED_HOST=%s\n", host);
	conft("set MEMCACHED_HOST=%s, port=%d key=%s, data=%s", host, port, key, value);
#endif
	servers = memcached_server_list_append(servers, host ? host : "localhost", port, &rc); // localhost 는 환경변수로 대체
	rc = memcached_server_push(memc, servers);

	if (rc == MEMCACHED_SUCCESS) {
#if defined DEBUG_TRACE
		conft("Added mamcached server successfully");
#endif
		uint32_t flags = 0;
		rc = memcached_set(memc, key, strlen(key), value, strlen(value), tts, flags);
		if (rc != MEMCACHED_SUCCESS) {
#if defined DEBUG_TRACE
			conft("Couldn't set key : %s", memcached_strerror(memc, rc));
#endif
		}
	}
	else
	{
		conft("Couldn't add memcached server: %s", memcached_strerror(memc, rc));
	}

	memcached_server_list_free(servers);
	memcached_free(memc);

	if (host) free(host);

	return rc;
}

char* load_file(char* path, size_t* load_len)
{
    int fd;
    char* load = 0;
    struct stat sb;

    fd = open(path, O_RDONLY | O_NONBLOCK, S_IROTH);
    if (fd == -1) {
        printf("%s file open failed!\n", path);
        return NULL;
    }

    if (fstat(fd, &sb) == -1) {
        printf("%s, fstat error\n", path);
        close(fd);
        return NULL;
    }

	if (!sb.st_size || sb.st_size > 102400000) // max 100MB
		return NULL;

    *load_len = sb.st_size;
    load = (char*)malloc(*load_len + 1);
    size_t read_len = read(fd, load, *load_len);
    close(fd);

    if (read_len != *load_len)
    {
        free(load);
        return NULL;
    }

    load[*load_len] = '\0';

    return load;
}

json_object* decode_json_resp(json_object* jobj, const char* _key, enum json_type _type) {

    enum json_type type;

    json_object_object_foreach(jobj, key, jsub)
    {
        type = json_object_get_type(jsub);
        if (_key == NULL && _type == type && type == json_type_object) {
            return jsub;
        }
        if (type == _type && type == json_type_array) {
            json_object* array;
            json_object_object_get_ex(jobj, _key, &array);
            return array;
        }
        if (!strcasecmp(_key, key))
            return jsub;
    }
    return NULL;
}

int signal_init(void (*sa)(int, siginfo_t*, void*), bool debug_sig_message)
{

#if 0	
	// 특정 시그널 빼고 등록
	if ((sigfillset(&g_sigsBlock) == -1)
		|| (sigdelset(&g_sigsBlock, SIGINT) == -1)
		|| (sigdelset(&g_sigsBlock, SIGKILL) == -1)
		|| (sigdelset(&g_sigsBlock, SIGUSR1) == -1)
		|| (sigdelset(&g_sigsBlock, SIGSEGV) == -1)
		|| (sigdelset(&g_sigsBlock, SIGALRM) == -1)
		|| (sigdelset(&g_sigsBlock, SIGTERM) == -1)
		|| (sigprocmask(SIG_SETMASK, &g_sigsBlock, &g_sigsOld) == -1)
		)
#else
	/// 모든 시그널 등록 
	if ((sigemptyset(&g_sigsBlock) == -1) || (sigprocmask(SIG_SETMASK, &g_sigsBlock, &g_sigsOld) == -1))
#endif		
	{
		conpt("Signal Set (Pid:%d) (Er:%d,%s)", getpid(), errno, (char*)strerror(errno));
		return -1;
	}

	struct sigaction act;
	act.sa_flags = SA_SIGINFO;// SA_NOCLDSTOP, SA_NOCLDWAIT, SA_ONSTACK , SA_RESTART, SA_NODEFER, SA_RESETHAND, ....
	act.sa_sigaction = sa;
#if 0
	// 특정 시그널 만 처리하도록 함수 등록
	if ((sigemptyset(&act.sa_mask) == -1)
		|| (sigaction(SIGINT, &act, NULL) == -1)
		|| (sigaction(SIGUSR1, &act, NULL) == -1)
		|| (sigaction(SIGSEGV, &act, NULL) == -1)
		|| (sigaction(SIGALRM, &act, NULL) == -1)
		|| (sigaction(SIGTERM, &act, NULL) == -1)
		)
	{
		conpt("Sigaction (Pid:%d) (Er:%d,%s)", getpid(), errno, (char*)strerror(errno));
		return -1;
	}
#else
	/// 모든 시그널을 함수로 보내자
	if ((sigemptyset(&act.sa_mask) == -1))
	{
		conpt("sigemptyset() (Pid:%d) (Er:%d,%s)", getpid(), errno, (char*)strerror(errno));
		return -1;
	}

	// signal number 는 os마다 다르다. shell command 'kill -l' 로 확인하자
	int nSigno;
	for (nSigno = SIGHUP; nSigno <= 31; nSigno++)
	{
		if (nSigno == SIGKILL || nSigno == SIGSTOP) /// kill (cannot be caught )
			continue;
		if (nSigno == SIGBUS) // core dump 뜨고 싶을 때 사용
			continue;

		if ((sigaction(nSigno, &act, NULL) == -1))
		{
			conpt("sigaction(%d) (Pid:%d) (Er:%d,%s)", nSigno, getpid(), errno, (char*)strerror(errno));
			return -1;
		}
#ifdef DEBUG
		else if (debug_sig_message)
		{
			conpt("sigaction(%d) (Pid:%d) success", nSigno, getpid());
		}
#endif
	}
#endif

	return 0;
}

int	tcp_create(int port_no)
{
	struct	sockaddr_in	addr_in;
	int		optval;
	int		fd;
	int		retry;

	memset(&addr_in, 0x00, sizeof(addr_in));

	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = INADDR_ANY;
	addr_in.sin_port = htons((u_short)port_no);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		conft("socket create error port = %d, errno = %d", port_no, errno);
		return -1;
	}

	optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

	for (retry = 0; retry < MAX_RETRY; retry++)
	{
		if (bind(fd, (struct sockaddr*)(&addr_in), sizeof(struct sockaddr)) < 0)
		{
			if (errno == EINVAL)// EINVAL -> 이미 바인드 성공했음...
			{
				// OK
				break;
			}
			else if (errno == EADDRINUSE)
			{
				conft("bind error. 사용중인 port(%d)임", port_no);
				optval = 1;
				setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
			}
			else
			{
				conft("bind error. port=%d, errno=%d", port_no, errno);
			}
			sleep(BIND_SLEEP_TIME);
		}
		else
		{
			break;
		}
	}

	if (retry >= MAX_RETRY)
	{
		close(fd);
		return -2;
	}

	if (listen(fd, 100) == -1)
	{
		close(fd);
		return -9;
	}

	optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));

	//struct linger optLinger={1,0};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 버려 버린다(바로리턴)
	//struct linger optLinger={1,5};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(송신완료까지블럭, 5초만 블럭인 시스템도 있고)
	struct linger optLinger = { 0,0 };//linger on! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(바로리턴,백그라운드처리로time_wait)
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&optLinger, sizeof(optLinger));

	return fd;

}

int	tcp_connect(const char* host, int port_no, struct	sockaddr_in* paddr)
{
	int		fd;
	struct	sockaddr_in addr_in;
	struct	hostent* hp;

	bzero((char*)&addr_in, sizeof(addr_in));

	addr_in.sin_family = AF_INET;

	if ((hp = gethostbyname(host)) == NULL)
	{
		if (atoi(&(host[0])) > 0)
			addr_in.sin_addr.s_addr = inet_addr(host);
		else
			return(-1);
	}
	else
	{
		addr_in.sin_addr.s_addr = ((struct in_addr*)(hp->h_addr))->s_addr;
	}

	addr_in.sin_port = htons((u_short)port_no);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return(-3);

	if (connect(fd, (struct sockaddr*)&addr_in, sizeof(addr_in)) < 0)
	{
		close(fd);
		return(-4);
	}

	if (paddr)
		memcpy(paddr, &addr_in, sizeof(addr_in));

	int optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));

	//struct linger optLinger={1,0};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 버려 버린다(바로리턴)
	//struct linger optLinger={1,5};//linger off! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(송신완료까지블럭, 5초만 블럭인 시스템도 있고)
	struct linger optLinger = { 0,0 };//linger on! 송신 소켓버퍼에 아직 남아있는 데이터는 보내고 연결종료(바로리턴,백그라운드처리로time_wait)
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&optLinger, sizeof(optLinger));

	return(fd);
}


int mk_int(char* data, int nLen)
{
	char szBuf[32] = { 0, };
	memcpy(szBuf, data, nLen);
	return atoi(szBuf);
}

char* mk_char(char* data, int nData, int nLen)
{
	char szBuf[32];
	if (nLen > 0)
	{
		sprintf(szBuf, "%0*d", nLen, nData);
		memcpy(data, szBuf, nLen);
	}
	else
	{
		sprintf(szBuf, "%d", nData);
		strcpy(data, szBuf);
	}
	return data;
}

int mk_hex(char* data, int nLen)
{
	char szBuf[32] = { 0, };
	memcpy(szBuf, data, nLen);
	return((int)strtol(szBuf, (char**)NULL, 16));
}

char* mk_hexchar(char* data, int nData, int nLen)
{
	char szBuf[32] = { 0, };
	sprintf(szBuf, "%0*X", nLen, nData);
	memcpy(data, szBuf, nLen);
	return data;
}

char* move_str(char* dest, char* src, int nLen)
{
	memcpy(dest, src, nLen);
	dest[nLen] = '\0';
	return dest;
}

int	udp_create(int port_no)
{
	struct	sockaddr_in	addr_in;
	int		fd;

	memset(&addr_in, 0x00, sizeof(addr_in));

	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = INADDR_ANY;
	addr_in.sin_port = htons((u_short)port_no);

	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		conft("socket create error port = %d, errno = %d", port_no, errno);
		return -1;
	}

	if (bind(fd, (struct sockaddr*)(&addr_in), sizeof(struct sockaddr)) < 0)
	{
		if (errno == EADDRINUSE)
		{
			conft("bind error. 사용중인 port(%d)임", port_no);
			close(fd);
			return -1;
		}
		else
		{
			conft("bind error. port=%d, errno=%d", port_no, errno);
			close(fd);
			return -2;
		}
	}

	return fd;

}

int udp_set_server(struct sockaddr_in* pServerAddr, const char* lpszUDPServerAddress, u_short nUDPServerPort)
{
	memset(pServerAddr, 0, sizeof(struct sockaddr_in));

	pServerAddr->sin_family = AF_INET;
	pServerAddr->sin_addr.s_addr = inet_addr(lpszUDPServerAddress);

	if (pServerAddr->sin_addr.s_addr == INADDR_NONE)
	{
		struct hostent* phost;
		phost = gethostbyname(lpszUDPServerAddress);
		if (phost != NULL)
			pServerAddr->sin_addr.s_addr = ((struct in_addr*)phost->h_addr)->s_addr;
		else
		{
			return 0;
		}
	}

	pServerAddr->sin_port = htons(nUDPServerPort);

	return 1;
}

int udp_send(int _logPort, struct sockaddr_in* pServerAddr, const char* szLog)
{
	ssize_t nLen = strlen(szLog);

	if (sendto(_logPort, szLog, nLen, 0, (struct sockaddr*)pServerAddr, sizeof(struct sockaddr_in)) != nLen)
	{
		return 0;
	}

	return (int)nLen;
}

char* trim(char* str)
{
	if (!str)
		return str;

	char* p = str + strlen(str) - 1;

	while (p >= str && *p <= ' ')
		p--;

	*++p = '\0';

	if (p == str)
		return str;

	char* src = str;

	while (*src <= ' ')
		src++;

	if (src == str)
		return str;

	while (src <= p)
		*str++ = *src++;

	return str;
}

char* rtrim(char* str)
{
	if (!str)
		return str;

	char* p = str + strlen(str) - 1;

	while (p >= str && *p <= ' ')
		p--;

	*++p = '\0';

	return str;
}

char* ltrim(char* str)
{
	if (!str)
		return str;

	char* src = str;
	char* p = str + strlen(str);

	while (*src <= ' ')
		src++;

	if (src == str)
		return str;

	while (src <= p)
		*str++ = *src++;

	return str;
}

#if 0
void erase_tcp_buffer(int sd, char* buf, int size)
{
	// 수신 버퍼에 남은것은 모두 읽어야 없어진다... 어쩌겠나 ....
	// socketopt(REUSEADDR) 사용시에는 버리지 않으면 다음에 연결된 녀석이 이 값을 보낸것 처럼 처리 된다
	int nCheckLen;
	while ((nCheckLen = tcp_check_read(sd)) > 0)
	{
		if (nCheckLen > size)
			nCheckLen = size;
		ssize_t nLen = read(sd, buf, nCheckLen);
		debug("read for clean socket data: sd->%d, nCheckLen->%d, len->%zu", sd, nCheckLen, nLen);
	}
}

#endif

bool isNumeric(const char* data, int nLen)
{
	char* ptr = (char*)data;
	if (nLen <= 0)
	{
		while (*ptr >= '0' && *ptr <= '9')
		{
			ptr++;
		}
		return (*ptr == 0);
	}
	while (*ptr >= '0' && *ptr <= '9' && --nLen >= 0)
	{
		ptr++;
	}
	return (*ptr == 0 || nLen < 0);
}

char* get_messageid()
{
	struct timeval now;
	gettimeofday(&now, (struct timezone*)NULL);
	char* time = (char*)malloc(21);
	snprintf(time, 21, "%010ld%010ld", now.tv_sec, now.tv_usec);

	return time;
}

char* get_datetime()
{
	time_t	now;
	struct	tm tm_s;
	char* szDateTime = (char*)malloc(15);

	time(&now);
	localtime_r(&now, &tm_s);

	snprintf(szDateTime, 15, "%04d%02d%02d%02d%02d%02d"
		, tm_s.tm_year + 1900
		, tm_s.tm_mon + 1
		, tm_s.tm_mday
		, tm_s.tm_hour
		, tm_s.tm_min
		, tm_s.tm_sec
	);
	return szDateTime;
}

int get_datetime_buf(char* time_buffer)
{
	time_t	now;
	struct	tm tm_s;

	time(&now);
	localtime_r(&now, &tm_s);

	return sprintf(time_buffer, "%04d%02d%02d%02d%02d%02d"
								, tm_s.tm_year + 1900
								, tm_s.tm_mon + 1
								, tm_s.tm_mday
								, tm_s.tm_hour
								, tm_s.tm_min
								, tm_s.tm_sec
							);
}


int euckr2utf8(char* source, char* dest, int dest_size)
{
	iconv_t it;
	char* pout;
	size_t in_size, out_size;

	it = iconv_open("UTF-8", "EUC-KR");
	in_size = strlen(source);
	out_size = dest_size;
	pout = dest;
	if (iconv(it, &source, &in_size, &pout, &out_size) == (size_t)-1)
		return(-1);
	iconv_close(it);
	return(pout - dest);
	/* return(out_size); */
}

int utf82euckr(char* source, char* dest, int dest_size)
{
	iconv_t it;
	char* pout;
	size_t in_size, out_size;

	it = iconv_open("EUC-KR", "UTF-8");
	in_size = strlen(source);
	out_size = dest_size;
	pout = dest;
	if (iconv(it, &source, &in_size, &pout, &out_size) == (size_t) - 1)
		return(-1);
	iconv_close(it);
	return(pout - dest);
	/* return(out_size); */
}


char* urlDecodeNewString(const char* str) {
	char eStr[] = "00"; /* for a hex code */
	int len = strlen(str);
	char* dStr = (char*)malloc(len + 1);
	
	bzero(dStr, len+1);

	int i,j; /* the counter for the string */

	for (i=0, j=0; i < len; ++i, ++j) {

		if (str[i] == '%') {
			if (str[i + 1] == 0 || str[i + 2] == 0) {
				break;
			}

			if (isxdigit(str[i + 1]) && isxdigit(str[i + 2])) {

				/* combine the next to numbers into one */
				eStr[0] = str[i + 1];
				eStr[1] = str[i + 2];

				/* convert it to decimal */
				long int x = strtol(eStr, NULL, 16);

				dStr[j] = x;
				i++;
				i++;
			}
		} else {
			dStr[j] = str[i];
		}
	}

	return dStr;
}

char* urlDecodeRewite(char* str)
{
	char eStr[] = "00"; /* for a hex code */
	int len = strlen(str);
	char* dStr = str;

	int i, j; /* the counter for the string */

	for (i = 0, j = 0; i < len; ++i, ++j) {

		if (str[i] == '%') {
			if (str[i + 1] == 0 || str[i + 2] == 0) {
				break;
			}

			if (isxdigit(str[i + 1]) && isxdigit(str[i + 2])) {

				/* combine the next to numbers into one */
				eStr[0] = str[i + 1];
				eStr[1] = str[i + 2];

				/* convert it to decimal */
				long int x = strtol(eStr, NULL, 16);

				dStr[j] = x;
				i++;
				i++;
			}
		}
		else {
			if( j < i )
				dStr[j] = str[i];
		}
	}
	dStr[j] = '\0';
	return dStr;
}