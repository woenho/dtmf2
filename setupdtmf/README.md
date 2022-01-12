```
DTMF게이트웨이(DTMF서버)를 자동으로 인스톨 하기 위하여 만든 프로젝트
```
<br />
asterisk 및 FreepBX는 sangoma에서 제공하는 설치 페이지를 참조한다.
기본 S/W들은 make 명령으로 작업을 순차적으로 수행한다<br />
기본적으로 시스템에 make와 gnu gcc g++ 컴파일 셋이  설치되어 있어야 한다.<br />
<br />
작업순서<br />
1. Asterisk 를 root 유저로 설치한다 (수정소스 반영)<br />
2. make root -> json memcached libev openssl  을 루트유저로 설치한다.<br />
3. make user -> processmon AsyncThreadPool amidtmf udplogd 를 dtmf 서버 기동 계정에서 작업한다.<br />
4. PBX환경을 설정한다<br />
<br />
설치완료 후 소스 폴더는 삭제한다<br />
asterisk 소스폴더는 보관한다<br />
<br />
<br />
설치 작업시에 주의 및 확인사항:<br />
CentOS 7에 맞게 설정이 진행되어 있다.
<br />
1. dialplan<br />
amidtmf2 에서는 사용되는 context 이름이 프로그램에 하드코딩으로 고정되어있다.<br />
하드코딩으로 하는 것이 좋을 것 같다. 이유는 asterisk 환경설정시 dtmf를 사용할 때 설정값에 혼란을 줄이기 위함이다.<br />
환경변수처리로 하면 관련정보들에 대한 이해가 많이 필요하다. 그러나 하드코딩이면 반드시 그렇게 설정해야한다고 명확하게 제시할 수 있다.<br />
하드코딩된 context 1: [DTMF] Inbound IP-PBX와의 트렁크에 연결된 context로 다른 트렁크 연결은 다른 context를 사용하라<br />
하드코딩된 context 2: [TRANSFER] BlindTransfer 라는 ami 명령을 요청할 때 사용되는 context롤 이 context에는 Transfer 지정이 반드시 되어 있어야 한다.<br />
전화가 걸려오면 UserEvent(DTMF_START,Callee: ${EXTEN}) 를 실행하고 inbound PBX로 Dial() 한다<br />
DialEnd 이벤트의 결과값이 ANSWER 면 채널정보를 emecached에 저장한다<br />
hangup 에 대한 userevent 발생용 context가 별도로 필요하다. 개발서버에서는 handlerHangupForDTMF 를 사용했다.<br />
UserEvent(DTMF_STOP,Callee: ${ARG1})가 실행되면 memcached에서 정보를 삭제한다<br />
asterisk 에서 pjsip 방식으로 refer tech를 처리하지 않고 sip refer 방식으로 처리되도록 프로그램밍 되어 있다.<br />
'PJSIP/sip:전화번호' 형식으로 반드시 sip: 가 부가 되어야한다.<br />
개발서버 dialplan 참조: Transfer(PJSIP/sip:${EXTEN}@TRUNK)<br />
refer header 추가 처리는 pjsip refer 처리함수에서 처리하도록 asterisk 소스가 변경되었다.<br />
Avaya CM 에서는 refer sip 메시지를 보낼 때 header를 추가하여 주면 이를 활용 한다고 한다.<br />
<br />
2. 가동시 필요한 필수 S/W 확인 및 설치<br />
설치되는 시스템에는 기본적으로 어떤 S/W 들이 설치되어 있는지 미리 다 알 수는 없다.<br />
그러므로 모든상황에서 자동설치를 할 수 있도록 대응 할 수는 없다.<br />
설치하며 필요한 S/W는 그 상황에 맞게 추가 설치 하여야 한다.<br />
필요 S/W의 판별은 두가지로 할 수 있다.<br />
아에 설치 되지 않은 경우와 꼭 필요한 최소버전 이하로 설치된 경우이다.<br />
<br />
2.1 기본적으로 추가설치가 필요한 주요한 third-paty S/W 목록<br />
2.1.1 Asterisk 16.18.0 + chan_pjsip.c<br />
	-> (REFER_HEADER 처리용으로 수정된 소스)<br />
2.1.2 FreePBX<br />
	-> Sangoma 에서 제공하는 Asterisk 관리용 webserver<br />
	-> Apache webserver, php, node 등 다양한 S/W를 필요로 한다.<br />
2.1.3 memcached<br />
	-> 현재 사용중인 채널정보 저장용 메모리캐싱시스템<br />
2.1.4 json-c<br />
	-> json parser, amidtmf2에서 직접 사용하지 않고 있으나 공통라이브러리에서 사용됨<br />
2.1.5 openssl<br />
	-> ssl, hash, crypto 라이브러리 등을 제공<br />
<br />
2.2 DTMF용 기본 S/W 목록<br />
amidtmf2 는 설치시 문제가 야기될 수 있는 가능성을 줄이고자 될 수 있으면 외부라이브러리 없이 자체적으로 소스를 개발하도록했다.
2.2.1 AsyncThreadPool<br />
2.2.2 tstpool<br />
2.2.3 udplogd<br />
2.2.4 amidtmf2<br />
<br />
2.3 그 외는 설치되는 서버의 상황에 맞추어 S/W 설치작업이 필요하다.
<br />
3. 트렁크 설정<br />
Inbound PBX와의 트렁크는 로그인이 없는 PJSIP 트렁크로 작업을 추천한다.<br />
트렁크 이름은 무엇으로하든 상관없다. 연결된 context는 반드시 [DTMF] 로 한다
<br />
4. inboud pbx 에서의 refer 처리<br />
만일 inboud pbx가 asterisk 라면 [globals] 컨텍스트 중 TRANSFER_CONTEXT = from-internal-xfer 에 설정된 컨텍스트를 확인해야한다.<br />
테스트서버에 설정된 from-internal-xfer 컨텍스트 예제를 아래 첨부한다<br />
이 컨텍스트의 실행내역은 asterisk 콘솔이나 로그로 현재 남지 않고있다.<br />
------ 아래 -----------<br />
[from-internal-xfer]<br />
exten => _X.,1,NoOp(${EXTEN}, Refer to 상담원직원)<br />
same => n,Dial(PJSIP/${EXTEN},,r)<br />
same => n,Hangup()<br />
<br />
5. crontab 설정사항 확인<br />
<br />
6. logrotate 설정 확인<br />
<br />
<br />
설치된 DTMF G/W 의 정상동작 유무 확인 방법:<br />
1. memcached 가 잘 동작하는지 확인<br />
2. 동작되는 내역이 로그에 정상으로 나오는지 확인<br />
3. astersik 로그를 보며 postman을 활용하여 /dtmf, /transfer URL을 이용한 처리가 정상으로 동작하는지 확인<br />
4. IVR, releay 서버와 연동이 잘 되는지 확인<br />
5. dtmf info 명령으로 발생되는 event 목록 및 event 수신함수가 얼마나 자주 호출 되는지 확인하여 manage.conf 수정으로 쓸데없는 이벤트발생을 중지시킨다.<br />
   dtmf 는 event name log mode를 0,1,2 로 순차적으로 토글한다.<br />
<br />





