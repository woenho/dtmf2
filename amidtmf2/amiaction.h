#ifndef _AMIACTION_H_
#define _AMIACTION_H_

#include "amiproc.h"

typedef struct CallInfo_t {
	char nWebLoaded;
	char szTime[15];
	char szExten[32];
	char szChannel[64];
	char szUniqueid[64];
	char szDestChannel[64];
	char szDestUniqueid[64];
}CallInfo;

int set_callinfo(const char* caller, CallInfo_t* info);
int get_callinfo(const char* caller, CallInfo_t* info);

ATP_STAT amiLogin(PATP_DATA atp_data);

PAMI_RESPONSE amiDeviceStatus(const char* device);
PAMI_RESPONSE amiSendDtmf(const char* caller, const char* dir, const char* dtmf);
PAMI_RESPONSE amiBlindTransfer(const char* caller, const char* callee, const char* header);








#endif
