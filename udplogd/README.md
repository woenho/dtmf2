설명:
udplogd는 udp를 이용한 log daemon 서버이다
udp를 사용하기에 너무 많은 로그가 발생한다면 유실되는 것이 있을 것이다.
udp로 소실 될 정도의 로그를 프로그램이 직접 로깅 한다면 프로세스의 수행속도가 현저히 떨어질 것이다
또한 프로그램이 멀티쓰레드로 동작하며 한 개의 로그파일을 사용하는 경우,
자원경합의 문제도 심각하게 발생할 뿐만 아니라 프로세스가 비정상 종료가 될 수도 있다.
이를 방지하기 위하여 로그는 별도의 로그데몬을 통하는 것이 추천된다.

컴파일 하면 예시로 logclient 파일이 생성된다.
이는 간단한 로그 클라이언트로 로그서버(udplogd)의 작동상태를 시험해 볼 수 있도록 했다.

로그 디폴트 포트는 26000 이다
로그파일의 디폴트 경로는 /var/log/udplogd/udplogd_{log port number}.log 이다
그러므로 디폴트포트가 적용된다면  /var/log/udplogd/udplogd_26000.log 이다

로그파일은 로그메시지를 수신하면 파일 디스크립터를 열고
로그파일은 0.5초 이내에 메시지를 수신하면 계속 같은 파일디스크립터를 사용한다
0.5초가 지나도 새로운 로그가 들오오지 않으면 파일 디스크립터를 닫는다.

첫번째 인자에 유효한 ip address format(x.x.x.x) 를 지정하면 지정된 ip adress 에서 방신한 로그만 로깅한다.

실행:
udplogd [valid ip address [port [logpath]]]
logclient {udplogd ip} {udplogd port} {log mode number} {\"log message\"}



