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
#include <pthread.h>

#include "logger.h"

#if defined(USE_MILISECOND_TIME)
//static const char* timestamp_format_default = "%04d-%02d-%02d %02d:%02d:%02d.%.3ld";
static const char* timestamp_format = "%04d-%02d-%02d %02d:%02d:%02d.%.3ld";
#else
//static const char* timestamp_format_default = "%Y-%m-%d %H:%M:%S";
static const char* timestamp_format = "%Y-%m-%d %H:%M:%S";
#endif

static char *logfile = NULL;
static char logfile_buf[128];

FILE* logfp = NULL;

void log_logfile (const char *file) {
    if (file == NULL) {
        logfile = NULL;
        return;
    }
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

void _log_writef(log_mode mode, const char* log) {
    char buf[4096 + 64] = { 0 };   // 이것은 최종 결과가 나오는 곳입니다 tmp2 + 20("YYYY-mm-dd HH:MM:SS ")
    char timestamp[64];     // timestamp

#if defined(USE_MILISECOND_TIME)
    struct timeval timenow;
#else
    time_t now;
#endif
    struct tm* ptm;

    switch (mode) {
    case LOGMODE_CONPT:
    case LOGMODE_CONPTN:
    case LOGMODE_CONFT:
    case LOGMODE_CONFTN:
#if defined(USE_MILISECOND_TIME)
        gettimeofday(&timenow, NULL);
        ptm = localtime(&timenow.tv_sec);
        sprintf(timestamp, timestamp_format
            , ptm->tm_year + 1900
            , ptm->tm_mon + 1
            , ptm->tm_mday
            , ptm->tm_hour
            , ptm->tm_min
            , ptm->tm_sec
            , timenow.tv_usec / 1000
        );
#else
        now = time(NULL);
        ptm = localtime(&now);
        strftime(timestamp, sizeof(timestamp), timestamp_format, ptm); // 텍스트 timestamp
#endif
        snprintf(buf, sizeof(buf), "%s %s", timestamp, log);
        break;

    case LOGMODE_CONP:
    case LOGMODE_CONPN:
    case LOGMODE_CONF:
    case LOGMODE_CONFN:
        strncpy(buf, log, sizeof(buf) - 1);
        break;

    }

    // 마지막글자 newline('\n') 제거
    chomp(buf);

    if (mode == LOGMODE_CONP
        || mode == LOGMODE_CONPT
        || mode == LOGMODE_CONPN
        || mode == LOGMODE_CONPTN
        ) {
        printf("%s", buf);
        if (mode == LOGMODE_CONP || mode == LOGMODE_CONPT) {
            printf("\n");
        }
        fflush(stdout);
    }

    if (!logfile)
        return;

    if (!logfp)
        logfp = fopen(logfile, "a");

    if (logfp) {
        fwrite(buf, 1, strlen(buf), logfp);
        if (mode == LOGMODE_CONF || mode == LOGMODE_CONFT || mode == LOGMODE_CONP || mode == LOGMODE_CONPT)
            fwrite("\n",1,1,logfp);
    }
    return;

}

void con_writef(log_mode mode, char* file, int line, const char* fmt, ...) {
    char tmp[4000];        // va_arg 텍스트 출력
    char tmp2[4096];        

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    snprintf(tmp2, sizeof(tmp2), "[%s:%d] %s", file, line, tmp);

    _log_writef(mode, tmp2);

}

void log_writef(log_mode mode, const char* fmt, ...) {
    char tmp[4096];        // va_arg 텍스트 출력

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    _log_writef(mode, tmp);
}

