/* ---------------------
 * UTF-8 한글확인 용
 * ---------------------*/
 

#include "util.h"
#include "WebConfig.h"

CWebConfig::CWebConfig()
{
}

CWebConfig::CWebConfig(const char* pcFile)
{
	if (!pcFile) {
		printf("%s(), pcFile이 지정되지 않음.", __func__);
		return;
	}

	Open(pcFile);
	fflush(stdout);
}

CWebConfig::CWebConfig(const CWebConfig &Other) 
{
	InitCopy(Other);
}

CWebConfig& CWebConfig::operator=(const CWebConfig &Other)
{
	if (this != &Other) {
		InitCopy(Other);
	}
	return *this;
}

CWebConfig::~CWebConfig() 
{
	m_mapConf.clear();
}

inline void CWebConfig::InitCopy(const CWebConfig &Other)
{
	m_mapConf.clear();
	m_mapConf = Other.m_mapConf;// 얉은 복사가 아니고 깊은 복사를 한다. 즉 Othre.m_mapConf와 this.m_mapConf는 멤버만 복사하고 다른 객체임
#ifndef NO_CFG_TRACE
	printf("%s() (This:%p, size=%zu) (src:%p, size=%zu)", __func__, &m_mapConf, m_mapConf.size(), &(Other.m_mapConf), Other.m_mapConf.size());
#endif
}

int CWebConfig::Open(const char * const pcFile)
{
	if (!pcFile) {
		printf("%s(), pcFile이 지정되지 않음.", __func__);
		return -1;
	}

	m_mapConf.clear();

	int nRet;
	ifstream fs;

	fs.open(pcFile, ios_base::in);
    //if (fs.fail()) {
    if (!fs.is_open()) {
		printf("%s(), pcFile을 읽지 못함(%s)", __func__, pcFile);
		nRet = -1;
	}
	else {
		nRet = Set(fs);
		fs.close();
	}

	return nRet;
}

inline int CWebConfig::Set(ifstream& fs) 
{
	int nRet=-1;

	try 
	{
		int i;
		char cLine[msc_nMaxChar + 1], cSection[msc_nMaxChar + 1];
		char cName[msc_nMaxChar + 1], cValue[msc_nMaxChar + 1];
		char *pcName,*pcValue;
		string strKey;		// 섹션@@이름
		string strValue;	// 값

		// 환경파일 최대 라인까지만 읽음
		for(i=0;i<msc_nMaxLine;i++)
		{
			cLine[0]=0x00;
			fs.getline(cLine, sizeof(cLine));

			if (fs.gcount() == 0) 
				break;
			
			if (cLine[0] == '#') {
#ifndef NO_CFG_TRACE
				printf("[comment line] %s\n", cLine);
#endif
				continue;
			}

			// 2018-05-18 add 3 line by woenho 라인 중간에도 #이 나오면 이후는 주석처리한다
			pcValue = strpbrk(cLine, "#");
			if (pcValue)
				*pcValue = '\0';


			if ((pcName=strchr(cLine,'['))) {
				cSection[0]=0x00;
				if ((pcValue=strchr(pcName,']'))) {
					*pcValue = 0x00;
					strcpy(cSection,++pcName);
					//printf("[CFG:Set] (Section:%s)\n", cSection);
				}
			}
			else {
#if 0
				/* edit woenho 2018-05-17 : 값 중에 구분자가 있으면 오류나서 수정함 또한 전역변수를 쓰는 함수라서 쓰레드사용시 오류남*/
				char *pcSave;
				pcName = strtok_r(cLine," \t:=\r\n",&pcSave);
				pcValue = strtok_r(NULL," \t:=\r\n",&pcSave);
				
				if (pcName && pcValue) {
					strKey = cSection;
					strKey += "@@";
					strKey += pcName;
					strValue = pcValue;
					m_mapConf[strKey] = strValue;
				}
#else
				
				memset(cName, 0x00, sizeof(cName));
				memset(cValue, 0x00, sizeof(cValue));

				pcValue = strpbrk(cLine, ":=");
				//printf("[CFG:Set] pcValue=%s, cLine=%s\n", pcValue, cLine);
				if (pcValue)
				{
					memcpy(cName, cLine, pcValue - cLine);
					strcpy(cValue, pcValue+1);
					trim(cName);
					trim(cValue);				
				}

				if (*cName && *cValue) {
					strKey = cSection;
					strKey += "@@";
					strKey += cName;
					strValue = cValue;
					m_mapConf[strKey] = strValue;
					nRet++;
#ifndef NO_CFG_TRACE
					printf("[CFG:Set] (Key:%s)(Value:%s)(Cnt:%zu)\n"
						, strKey.c_str(), strValue.c_str()
						, m_mapConf.size()
					);
#endif
				}
#endif
			}
		}
	} catch(exception& e){
		nRet = -1;
		// 실행화면에도 표시하자
		printf("[CFG:Set] Exception (%s)", e.what());
	} catch (...) {
		printf("[CFG:Set] %s:%d, errno=%d, %s", __FILE__, __LINE__, errno, strerror(errno));
	}

	return nRet;
}

/*
// debian 의 stl 에서 이거 오동작함
const char* CWebConfig::Gets(const char* const pcSection, const char* const pcName, const char* pcDefault)
{
	return Get(pcSection, pcName, pcDefault).c_str();
}
*/
std::string CWebConfig::Get(const char * const pcSection, const char * const pcName, const char *pcDefault)
{
	if (!pcSection || !pcName) {
		conpt("[CFG:Get] !pcSection(%p) || !pcName(%p)", pcSection,pcName);
		return "";
	}

	string strKey="",strValue="";

	strKey = pcSection;
	strKey += "@@";
	strKey += pcName;

	map<string, string>::iterator it_map; 
	it_map = m_mapConf.find(strKey);
	if (it_map == m_mapConf.end()) {
		if (!pcDefault) {
			string strNull;
			return strNull;
		}
		else {
			string strDefault(pcDefault);
			return strDefault;
		}
	}
	else { 
		return m_mapConf[strKey];
	}
}

