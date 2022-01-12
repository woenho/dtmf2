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

static char *logfile = NULL;
static char logfile_buf[128];
static const char *timestamp_format = "%Y-%m-%d %H:%M:%S";

#if defined(LOG_LOCK)
pthread_mutex_t mutex_log;
#endif

void con_logfile (const char *file) {
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

void _con_writef(enum con_callmode cm, const char* file, int line, const char* function, const char* fmt, ...) {
    char tmp[1024] = { 0 };   // 이것은 최종 결과가 나오는 곳입니다 tmp2 + 20("YYYY-mm-dd HH:MM:SS ")
    char tmp2[1000];        // va_arg 텍스트 출력
    char timestamp[64];     // timestamp

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp2, sizeof(tmp2), fmt, ap);
    va_end(ap);

    // TODO: 이번에는 쿼리 부분을 별도의 함수에 넣어야 합니다!
    struct timeval timenow;
    struct tm* ptm;
    gettimeofday(&timenow, NULL);
    ptm = localtime(&timenow.tv_sec);
    strftime(timestamp, sizeof(timestamp), timestamp_format, ptm); // 텍스트 timestamp

    switch (cm) {
    case CON_CALLMODE_CONFT:
    case CON_CALLMODE_CONPT:
    case CON_CALLMODE_CONFTN:
    case CON_CALLMODE_CONPTN:
        concatf(tmp, "%s [%s:%d] %s", timestamp, file, line, tmp2);
        break;

    case CON_CALLMODE_CONF:
    case CON_CALLMODE_CONP:
    case CON_CALLMODE_CONFN:
    case CON_CALLMODE_CONPN:
        strncpy(tmp, tmp2, sizeof(tmp) - 1);
        // concatf(tmp, "[%s:%d] %s", file, line, tmp2); -- 라인스킵만 필요한 경우도 있고... 특별한경우 사용
        break;
    default:
        printf("___ log mode error [%s:%d] %s\n", file, line, tmp2);
        return;
    }

    // 마지막글자 newline('\n') 제거
    chomp(tmp);

#if (defined(CON_PRINT_CONSOLE_TOO) || defined(CON_PRINT_CONSOLE_ONLY))
    printf("%s", tmp);
    if (cm != CON_CALLMODE_CONFTN && cm != CON_CALLMODE_CONFN)
        printf("\n", tmp);
    fflush(stdout);
#else
    if (cm == CON_CALLMODE_CONP || cm == CON_CALLMODE_CONPT) {
        printf("%s\n", tmp);
        fflush(stdout);
    }
#endif
    

#if defined(CON_PRINT_CONSOLE_ONLY)
    return;
#endif

    if (!logfile)
        return;

#if defined(LOG_LOCK)
    pthread_mutex_lock(&mutex_log);
#endif
    FILE* f = fopen(logfile, "a");
    if (f != NULL) {
        fprintf(f, "%s", tmp);
        if (cm != CON_CALLMODE_CONFTN && cm != CON_CALLMODE_CONFN)
            fprintf(f, "\n");
        fclose(f);
    }
#if defined(LOG_LOCK)
    pthread_mutex_unlock(&mutex_log);
#endif
}

