/* ---------------------
 * UTF-8 한글확인 용
 * ---------------------*/
 


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/netfilter_ipv4.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/netfilter_ipv4.h>
#include <netinet/tcp.h>

#include "logger.h"

//~ #define concat(dst, src) strncat((dst), (src), ((sizeof(dst)) - strlen(dst) - 1))
//~ #define concatf(dst, ...) snprintf((dst) + strlen((dst)), sizeof((dst)) - strlen((dst)), __VA_ARGS__)

static char *logfile = NULL;
static char logfile_buf[128] = { 0, };
//static const char *timestamp_format = "%Y-%m-%d %H:%M:%S ";
static const char* timestamp_format = "%04d-%02d-%02d %02d:%02d:%02d.%.3ld";

LOG_MODE logmode = logmode_file;
char logserver[128] = { 0, };
unsigned short logport;
struct sockaddr_in logaddr;
int logfd = 0;

#if defined(LOG_LOCK)
pthread_mutex_t mutex_log;
#endif

void con_logmode(LOG_MODE mode)
{
    logmode = mode;
}

void con_udpclose()
{
    if (logmode == logmode_udp)
        close(logfd);
    logfd = 0;
}

void con_logudp(const char* server, unsigned short port)
{
    con_logmode(logmode_udp);

    struct	sockaddr_in	addr_in;
    memset(&addr_in, 0x00, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = INADDR_ANY;
    addr_in.sin_port = htons(0);// client 0 is random, 결정은 sendto() 최초호출 시
    if ((logfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        conpt("logclient socket create error errno = %d\n", errno);
        return ;
    }

    memset(&logaddr, 0, sizeof(struct sockaddr_in));
    logaddr.sin_family = AF_INET;
    struct	hostent* hp;
    if ((hp = gethostbyname(server)) == NULL)
    {
        if (atoi(&(server[0])) > 0)
            logaddr.sin_addr.s_addr = inet_addr(server);
        else
            return;
    }
    else
    {
        logaddr.sin_addr.s_addr = ((struct in_addr*)(hp->h_addr))->s_addr;
    }
    logaddr.sin_port = htons(port);

}

void con_logfile (const char *file) {
    if (file == NULL) {
        logfile = NULL;
        return;
    }

    con_logmode(logmode_file);

    strncpy(logfile_buf, file, sizeof(logfile_buf)-1);
    logfile = logfile_buf;
#if defined(LOG_LOCK)
    pthread_mutex_init(&mutex_log, NULL);
#endif
}

char* chomp(char* s) {
    if (s == NULL)
        return NULL;
    size_t len = strlen(s);

    int n;
    for (n = len - 1; n >= 0; n--) {
        if (s[n] == '\n' || s[n] == '\r')
            s[n] = '\0';
        else
            return s;
    }
    return s;
}

void _con_writef(enum con_callmode cm, const char* file, int line, const char* function, const char* fmt, ...) {
    char buf[4096] = { 0 };   // 이것은 최종 결과가 나오는 곳입니다 tmp2 + strlen(timestamp_format)
    char tmp2[4070];        // va_arg 텍스트 출력

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp2, sizeof(tmp2)-1, fmt, ap);
    va_end(ap);

    // logfile 을 사용하는 경우는 무조건 USE_LOCAL_LOGTIME 가 설정되어야 한다
    // 무조건 udplogd 로 로깅하고 로칼타임을 사용하지 않는 경우는 
    // USE_LOCAL_LOGTIME 를 빼면 로그처리 성능 향상을 기대할 수 있다.

#if defined(USE_LOCAL_LOGTIME)
    char timestamp[64] = { 0, };     // timestamp
    struct timeval timenow;
    struct tm* ptm;
    gettimeofday(&timenow, NULL);
    ptm = localtime(&timenow.tv_sec);
    // strftime(timestamp, sizeof(timestamp), timestamp_format, ptm); // 텍스트 timestamp
    sprintf(timestamp, timestamp_format
        , ptm->tm_year + 1900
        , ptm->tm_mon + 1
        , ptm->tm_mday
        , ptm->tm_hour
        , ptm->tm_min
        , ptm->tm_sec
        , timenow.tv_usec / 1000
    );
#endif

    switch (cm) {
    case CON_CALLMODE_CONPT:
    case CON_CALLMODE_CONPTN:
    case CON_CALLMODE_CONFT:
    case CON_CALLMODE_CONFTN:
        if (logmode == logmode_udp) {
#if defined(USE_LOCAL_LOGTIME)
            snprintf(buf, sizeof(buf) - 1, "%c%s [%s:%d] %s", cm - 4, timestamp, file, line, tmp2);
#else
            snprintf(buf, sizeof(buf) - 1, "%c[%s:%d] %s", cm, file, line, tmp2);
#endif
        }
        else {
#if !defined(USE_LOCAL_LOGTIME)
            // 무조건 로칼타임 써야한다. 위의 것과 중복만 피하자
            char timestamp[64] = { 0, };     // timestamp
            struct timeval timenow;
            struct tm* ptm;
            gettimeofday(&timenow, NULL);
            ptm = localtime(&timenow.tv_sec);
            //strftime(timestamp, sizeof(timestamp), timestamp_format, ptm); // 텍스트 timestamp
            sprintf(timestamp, timestamp_format
                , ptm->tm_year + 1900
                , ptm->tm_mon + 1
                , ptm->tm_mday
                , ptm->tm_hour
                , ptm->tm_min
                , ptm->tm_sec
                , timenow.tv_usec / 1000
            );
#endif
            snprintf(buf, sizeof(buf), "%s [%s:%d] %s", timestamp, file, line, tmp2);
        }
        break;

    case CON_CALLMODE_CONP:
    case CON_CALLMODE_CONPN:
    case CON_CALLMODE_CONF:
    case CON_CALLMODE_CONFN:
        if (logmode == logmode_udp) {
            *buf = (char)cm;
            strncpy(buf+1, tmp2, sizeof(buf) - 2);
        }
        else {
            strncpy(buf, tmp2, sizeof(buf) - 1);
        }
        break;
    default:
        printf("___ log mode error [%s:%d] %s\n", file, line, tmp2);
        fflush(stdout);
        return;
    }

    // 마지막글자 newline('\n') 제거
    chomp(buf);

    if (cm == CON_CALLMODE_CONP || cm == CON_CALLMODE_CONPT) {
        printf("%s\n", buf);
        fflush(stdout);
    }

#if defined(CON_PRINT_CONSOLE_ONLY)
    return;
#endif

    if (logmode == logmode_udp) {
        sendto(logfd, buf, strlen(buf), 0, (struct sockaddr*)&logaddr, sizeof(struct sockaddr_in));
    } else {
        if (!logfile)
            return;
#if defined(LOG_LOCK)
        pthread_mutex_lock(&mutex_log);
#endif
        FILE* f = fopen(logfile, "a");
        if (f != NULL) {
            fprintf(f, "%s", buf);
            if (cm != CON_CALLMODE_CONFN && cm != CON_CALLMODE_CONFTN && cm != CON_CALLMODE_CONPN && cm != CON_CALLMODE_CONPTN)
                fprintf(f, "\n");
            fclose(f);
        }
#if defined(LOG_LOCK)
        pthread_mutex_unlock(&mutex_log);
#endif
    }
}

