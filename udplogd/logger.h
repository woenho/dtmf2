/* ---------------------
 * UTF-8 한글확인 용
 * ---------------------*/
 
#if !defined(__UDPLOGD_H__)
#define __UDPLOGD_H__
/*
    logf(...)       타임스탬프(X) 줄 바꿈(O), log daemon usefull
    logft(...)      타임스탬프(O) 줄 바꿈(O), log daemon usefull
    logfn(...)      타임스탬프(X) 줄 바꿈(X), log daemon usefull
    logftn(...)     타임스탬프(O) 줄 바꿈(X)
    logp(...)       타임스탬프(X) 줄 바꿈(O), 무조건 콘솔은 출력
    logpt(...)      타임스탬프(O) 줄 바꿈(O), 무조건 콘솔은 출력
    logpn(...)      타임스탬프(X) 줄 바꿈(x), 무조건 콘솔은 출력
    logptn(...)     타임스탬프(O) 줄 바꿈(x), 무조건 콘솔은 출력
    conp(...)       타임스탬프(X) 줄 바꿈(O), 무조건 콘솔은 출력, file & line
    conpt(...)      타임스탬프(O) 줄 바꿈(O), 무조건 콘솔은 출력, file & line
    conpn(...)      타임스탬프(X) 줄 바꿈(x), 무조건 콘솔은 출력, file & line
    conptn(...)     타임스탬프(O) 줄 바꿈(x), 무조건 콘솔은 출력, file & line
*/

typedef enum {
    LOGMODE_CONF = 1,
    LOGMODE_CONFN = 2,
    LOGMODE_CONP = 3,
    LOGMODE_CONPN = 4,
    LOGMODE_CONFT = 5,
    LOGMODE_CONFTN = 6,
    LOGMODE_CONPT = 7,
    LOGMODE_CONPTN = 8
}log_mode;

#define logcat(dst, src) strncat((dst), (src), ((sizeof(dst)) - strlen(dst) - 1))
#define logcatf(dst, ...) snprintf((dst) + strlen((dst)), sizeof((dst)) - strlen((dst)), __VA_ARGS__)


void log_logfile (const char *file);
void log_writef(log_mode mode, const char* fmt, ...);
void con_writef(log_mode mode, char* file, int line, const char* fmt, ...);

extern FILE* logfp;

#define logf(...) log_writef(LOGMODE_CONF, __VA_ARGS__)
#define logft(...) log_writef(LOGMODE_CONFT, __VA_ARGS__)
#define logfn(...) log_writef(LOGMODE_CONFN, __VA_ARGS__)
#define logftn(...) log_writef(LOGMODE_CONFTN, __VA_ARGS__)

#define logp(...) log_writef(LOGMODE_CONP, __VA_ARGS__)
#define logpt(...) log_writef(LOGMODE_CONPT, __VA_ARGS__)
#define logpn(...) log_writef(LOGMODE_CONPN, __VA_ARGS__)
#define logptn(...) log_writef(LOGMODE_CONPTN, __VA_ARGS__)

#define conp(...) con_writef(LOGMODE_CONP, __FILE__, __LINE__, __VA_ARGS__)
#define conpt(...) con_writef(LOGMODE_CONPT, __FILE__, __LINE__, __VA_ARGS__)
#define conpn(...) con_writef(LOGMODE_CONPN, __FILE__, __LINE__, __VA_ARGS__)
#define conptn(...) con_writef(LOGMODE_CONPTN, __FILE__, __LINE__, __VA_ARGS__)


#endif