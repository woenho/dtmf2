/* ---------------------
 * UTF-8 한글확인 용
 * ---------------------*/
 
#if !defined(__LOGGER_H__)
#define __LOGGER_H__
/*
    conp(...)       타임스탬프(X) 줄 바꿈(O), 무조건 콘솔은 출력
    conpt(...)      타임스탬프(O) 줄 바꿈(O), 무조건 콘솔은 출력
    conf(...)       타임스탬프(X) 줄 바꿈(O)
    conft(...)      타임스탬프(O) 줄 바꿈(O)
    confn(...)      타임스탬프(X) 줄 바꿈(X), 로그데몬에서는 사용금지
    conftn(...)     타임스탬프(O) 줄 바꿈(X), 로그데몬에서는 사용금지
*/

enum con_callmode {
    CON_CALLMODE_CONF = 1,
    CON_CALLMODE_CONFN = 2,
    CON_CALLMODE_CONP = 3,
    CON_CALLMODE_CONPN = 4,
    // 아래는 시간정보 있음
    CON_CALLMODE_CONFT = 5,
    CON_CALLMODE_CONFTN = 6,
    CON_CALLMODE_CONPT = 7,
    CON_CALLMODE_CONPTN = 8
};

#define concat(dst, src) strncat((dst), (src), ((sizeof(dst)) - strlen(dst) - 1))
#define concatf(dst, ...) snprintf((dst) + strlen((dst)), sizeof((dst)) - strlen((dst)), __VA_ARGS__)


void con_logfile (const char *file);
void _con_writef (enum con_callmode cm, const char *file, int line, const char *function, const char *fmt, ...);


#if defined(CON_DEBUG)
#define con_debug(...) _con_writef(CON_CALLMODE_CONPT, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define con_debug(...)
#endif

#define conp(...) _con_writef(CON_CALLMODE_CONP, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define conpt(...) _con_writef(CON_CALLMODE_CONPT, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define conf(...) _con_writef(CON_CALLMODE_CONF, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define conft(...) _con_writef(CON_CALLMODE_CONFT, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define confn(...) _con_writef(CON_CALLMODE_CONFN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define conftn(...) _con_writef(CON_CALLMODE_CONFTN, __FILE__, __LINE__, __func__, __VA_ARGS__)


#endif