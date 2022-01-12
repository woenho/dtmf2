/*
* amiproc.cpp 에 있는 ami_event() 함수가 입력되는 이벤트메시지를 분리하여 AMI_EVENTS 구조체에 넣는다
* 이 파일은 g_process 에 등록된 이벤트 처리 함수들을 모아 놓는다
* 즉 실제 이벤트를 처리하는 함수들은 여기에 모아놓는다
*/
#include "amiaction.h"
#include "processevents.h"


ATP_STAT event_hangup(AMI_EVENTS& events)
{
	ATP_STAT next = stat_suspend;

	logging_events(events);


	return next;
}


ATP_STAT event_dialbegin(AMI_EVENTS& events)
{
	ATP_STAT next = stat_suspend;

	logging_events(events);


	return next;
}

ATP_STAT event_varset(AMI_EVENTS& events)
{
	ATP_STAT next = stat_suspend;

	logging_events(events);

	return next;
}

ATP_STAT event_userevent(AMI_EVENTS& events)
{
	ATP_STAT next = stat_suspend;

	const char* szEventName = NULL;
	const char* szChannel = NULL;
	const char* szUniqueid = NULL;
	const char* szExten = NULL;
	const char* szCallerIDNum = NULL;

	//char buf[10240] = { 0 };
	// int len, rc=0;
	CallInfo ci = { 0 };

	szEventName = get_amivalue(events, "UserEvent");
	szChannel = get_amivalue(events, "Channel");
	szUniqueid = get_amivalue(events, "Uniqueid");
	if (!strcmp(szEventName, "Hangup"))
		szExten = get_amivalue(events, "Callee"); // hangup push function 타면 exten 이 's' 로 변환한다, 그래서 body에 Callee 추가했다
	else
		szExten = get_amivalue(events, "Exten");
	szCallerIDNum = get_amivalue(events, "CallerIDNum");

	char* p;
	for (p = (char*)szUniqueid; *p; p++)
		if (*p == '.')
			*p = '0';

	try {

		if (!strcmp(szEventName, "DTMF_START")) {
			logging_events(events);

#if defined(DEBUG)
			conpt("--- %s, %s, %s, %s, %s\n", szEventName, szCallerIDNum, szExten, szChannel, szUniqueid);
#endif

			// memcached에 다이알로그 등록
			ci.nWebLoaded = 0;
			strncpy(ci.szExten, szExten, sizeof(ci.szExten) - 1);
			strncpy(ci.szChannel, szChannel, sizeof(ci.szChannel) - 1);
			strncpy(ci.szUniqueid, szUniqueid, sizeof(ci.szUniqueid) - 1);
			if (set_callinfo(szCallerIDNum, &ci)) {
				conft("/set_callinfo error, caller=%s", szCallerIDNum);
			}
			else {
				conft("/set_callinfo, caller=%s, Channel=%s", szCallerIDNum, szChannel);
			}

		}
		else if (!strcmp(szEventName, "DTMF_STOP")) {
			// memcached에 다이알로그 삭제
			logging_events(events);

#if defined(DEBUG)
			conpt("--- %s, %s\n", szEventName, szCallerIDNum);
#endif
			// delete 는 memcached 내부적으로 막혀있다. 그래서 1초뒤에 사라지게 하고 새로 설정될 값은 "" 이다..
			set_memcached(szCallerIDNum, "", 1);

		}
		else {
			throw util_exception(999, "Invalid UserEvent type. event name:%s, channel=%s", szEventName, szChannel);
		}

	}
	catch (util_exception& e) {
		if (e.code() != 999) {
			conft("m_code=%d, %s", e.code(), e.what());
		}
	}
	catch (...) {
		conft("errno=%d, %s", errno, strerror(errno));
	}

	return next;
}

ATP_STAT event_dialend(AMI_EVENTS& events)
{
	/*
		Event: DialEnd
		Privilege: call,all
		Channel: PJSIP/800020005-00000006
		ChannelState: 6
		ChannelStateDesc: Up
		CallerIDNum: 800020005
		CallerIDName: 유선20005
		ConnectedLineNum: 800020003
		ConnectedLineName: 더미유선02
		Language: en
		AccountCode:
		Context: DEVELOPMENT
		Exten: 800020003
		Priority: 6
		Uniqueid: 1636598043.6
		Linkedid: 1636598043.6
		DestChannel: PJSIP/800020003-00000007
		DestChannelState: 6
		DestChannelStateDesc: Up
		DestCallerIDNum: 800020003
		DestCallerIDName: 더미유선02
		DestConnectedLineNum: 800020005
		DestConnectedLineName: 유선20005
		DestLanguage: en
		DestAccountCode:
		DestContext: DEVELOPMENT
		DestExten:
		DestPriority: 1
		DestUniqueid: 1636598043.7
		DestLinkedid: 1636598043.6
		DialStatus: ANSWER
	*/

	const char* szExten = NULL;
	const char* szChannel = NULL;
#if !defined(USE_USEREVENT_CALLSTARTED)
	const char* szUniqueid = NULL;
#endif
	const char* szDestChannel = NULL;
	const char* szDestUniqueid = NULL;
	const char* szCallerIDNum = NULL;
	const char* szDialStatus = NULL;
	char* p;

	// char buf[10240] = { 0 };
	// int len, rc=0;
	CallInfo ci = { 0 };

	try {

		szDialStatus = get_amivalue(events, "DialStatus");

		if (!strcmp(get_amivalue(events, "Context"), "DTMF") && !strcmp(szDialStatus, "ANSWER")) {
			logging_events(events);

			szCallerIDNum = get_amivalue(events, "CallerIDNum");
#if defined(USE_USEREVENT_CALLSTARTED)
			if (get_callinfo(szCallerIDNum, &ci)) {
				conft("memcached get not found, caller=%s\n", szCallerIDNum);
				throw util_exception(999, "memcached get not found");
			}
#endif
			// memcached에 다이알로그 등록
			szExten = get_amivalue(events, "Exten");
			szChannel = get_amivalue(events, "Channel");
			// 채널 및 exten  이 일치하는가?

			szDestChannel = get_amivalue(events, "DestChannel");
			szDestUniqueid = get_amivalue(events, "DestUniqueid");

			for (p = (char*)szDestUniqueid; *p; p++)
				if (*p == '.')
					*p = '0';

#if !defined(USE_USEREVENT_CALLSTARTED)
			szUniqueid = get_amivalue(events, "Uniqueid");

			for (p = (char*)szUniqueid; *p; p++)
				if (*p == '.')
					*p = '0';

			ci.nWebLoaded = 0;
			strncpy(ci.szExten, szExten, sizeof(ci.szExten) - 1);
			strncpy(ci.szChannel, szChannel, sizeof(ci.szChannel) - 1);
			strncpy(ci.szUniqueid, szUniqueid, sizeof(ci.szUniqueid) - 1);

#endif

			strncpy(ci.szDestChannel, szDestChannel, sizeof(ci.szDestChannel) - 1);
			strncpy(ci.szDestUniqueid, szDestUniqueid, sizeof(ci.szDestUniqueid) - 1);
			if (set_callinfo(szCallerIDNum, &ci)) {
				conft("/set_callinfo error, caller=%s", szCallerIDNum);
			}
			else {
				conft("/set_callinfo, caller=%s, exten=%s, Channel=%s, DestChannel=%s", szCallerIDNum, szExten, szChannel, szDestChannel);
			}

		}
	}
	catch (exception& e) {
		conft("errno=%d, %s", errno, e.what());
	}
	catch (...) {
		conft("errno=%d, %s", errno, strerror(errno));
	}

	return stat_suspend;
}


