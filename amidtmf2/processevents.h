
#ifndef _PROCESS_EVENTS_H_
#define _PROCESS_EVENTS_H_

#include "amiaction.h"

// 이벤트를 처리할 함수들 목록
ATP_STAT event_hangup(AMI_EVENTS& events);
ATP_STAT event_dialbegin(AMI_EVENTS& events);
ATP_STAT event_dialend(AMI_EVENTS& events);
ATP_STAT event_varset(AMI_EVENTS& events);
ATP_STAT event_userevent(AMI_EVENTS& events);
ATP_STAT event_dialend(AMI_EVENTS& events);

#endif
