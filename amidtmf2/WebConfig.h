/* ---------------------
 * UTF-8 한글확인 용
 * ---------------------*/
 

#include <map>
#include <list>
#include <queue>
#include <iostream>
#include <fstream>
#include <string>
#include <string>

using namespace std;


#ifndef __WEB_CONFIG_H__
#define __WEB_CONFIG_H__

class CWebConfig
{
public :
	/**
	* @brief 생성자
	*/
	CWebConfig();

	/**
	* @brief 생성자
	*
	* @param pcFile 환경파일 경로
	*/
	CWebConfig(const char* pcFile);

	/**
	* @brief 복사생성자
	*
	* @param Other 복사객체
	*/
	CWebConfig(const CWebConfig&Other);

	/**
	* @brief 대입연산자
	*
	* @param Other 대입객체
	* @return 성공: 대입된 객체참조자
	* @return 실패: 없음
	*/
	CWebConfig& operator=(const CWebConfig&Other);

	/**
	* @brief 소멸자
	*/
	virtual ~CWebConfig();

	/**
	* @brief 환경파일 열기
	* 
	* @param pcFilePath 환경파일 경로
	* @return 성공: >0 (환경변수 갯수)
	* @return 실패: -1
	*/
	int Open(const char * const pcFilePath);

	/**
	* @brief 환경변수값 얻기
	* 
	* @param pcSection 섹션
	* @param pcName 변수명
	* @param pcDefault 디폴트값, 검색되지 않을 경우
	* @return 성공: 변수값
	* @return 실패: ""
	*/
	// const char* Gets(const char* const pcSection, const char* const pcName, const char* pcDefault="");
	std::string Get(const char * const pcSection, const char * const pcName, const char *pcDefault="");

	/**
	* @brief 환경변수 갯수 얻기
	* 
	* @return 성공: >=0 (환경변수 갯수)
	* @return 실패: 없음 
	*/
	size_t Size() { return m_mapConf.size(); }


private:
	/**
	* @brief 복사생성,대입연산 초기화
	* 
	* @param Other 복사객체
	*/
	inline void InitCopy(const CWebConfig&Other);

	/**
	* @brief 환경변수 설정
	* 
	* @param fs 파일스트림
	* @return 성공: 설정된 환경변수갯수
	* @return 실패: -1
	*/
	inline int Set(std::ifstream& fs);


private :
    map <string, string> m_mapConf;				///< 환경변수 저장맵
    static const int msc_nMaxLine=1000;			///< 최대 라인수
    static const int msc_nMaxChar=200;			///< 최대 문자열크기
};

#endif
