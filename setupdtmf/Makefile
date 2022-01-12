# * ---------------------
# * UTF-8 한글확인 용
# * ---------------------
# amidtmf 를 자동으로 셋업하기 위한 스크립트파일
# 전체를 한 번에 설치 할 수도 있고, 각 요소별 설치도 할 수도 있다
# 전체를 한 번에 삭제 할 수도 있고, 각 요소별 삭제를 할 수도 있다
#

CHKOS = $(shell uname -a | grep Debian | wc -l)
ifeq ($(CHKOS), 1)
	OS = DEBIN
else
	OS = `cat /etc/*release*|grep release|awk '{print $1}'|head -n 1`
endif

.PHONY: all memcached openssl processmon AsyncThreadPoll amidtmf udplogd uninstall

all:
	$(info --------------------------------------------------------)
	$(info $(shell cat README.md))
	$(info --------------------------------------------------------)

root: json memcached openssl 
	$(info root setup)
	$(info this id is $(shell whoami) ....)

user: processmon AsyncThreadPool amidtmf udplogd 
	$(info user setup)
	$(info this id is $(shell whoami) ....)
	mkdir -p /var/log/dtmf
	mkdir -p /var/log/udplogd

uninstall: unmemcached
	$(info ============ uninstall ==========)
	$(info this id is $(shell whoami) ....)
	yum -y remove json-c-devel json-c-doc.noarch json-c

udplogd:
	$(info this id is $(shell whoami) ....)
	$(info setup udplogd)
	$(MAKE) -C ../udplogd clean release install


amidtmf:
	$(info setup amidtmf)
	$(info this id is $(shell whoami) ....)
	$(MAKE) -C ../amidtmf2 clean release install

AsyncThreadPool:
	$(info setup AsyncThreadPool)
	$(info this id is $(shell whoami) ....)
	$(MAKE) -C ../AsyncThreadPool clean release

processmon:
	$(info setup processmon)
	$(info this id is $(shell whoami) ....)
	$(MAKE) -C ../processmon clean release install

openssl:
	$(info =============================)
	$(info setup openssl-1.1.1l)
	$(info this id is $(shell whoami) ....)
	tar xvfz openssl-1.1.1l.tar.gz
	cd openssl-1.1.1l; \
	./config --prefix=/usr/local/ssl --openssldir=/usr/local/ssl shared; \
	make all install; \
	echo "/usr/local/ssl/lib" > /etc/ld.so.conf.d/openssl-1.1.1l.conf; \
	ldconfig -v|grep ssl; \
	ln -s /usr/local/ssl/lib/libssl.so.1.1 /usr/lib64/libssl.so.1.1; \
	ln -s /usr/local/ssl/lib/libcrypto.so.1.1 /usr/lib64/libcrypto.so.1.1; \
	mv /usr/bin/openssl /usr/bin/openssl1.0.2; \
	ln -s /usr/local/ssl/bin/openssl /usr/bin/openssl; \
	openssl version; \
	/usr/local/ssl/bin/openssl version; \
	ldd /usr/bin/openssl

libev:
	$(info =============================)
	$(info setup libev-4.33)
	$(info this id is $(shell whoami) ....)
	$(info -----------------------------)
	tar xvfz libev-4.33.tar.gz
	cd libev-4.33; ./configure
	$(MAKE) -C libev-4.33 all install

json:
	$(info =============================)
	$(info setup json-c)
	$(info this id is $(shell whoami) ....)
	$(info -----------------------------)
	yum -y install json-c-devel json-c-doc.noarch json-c

memcached_install:
	yum install -y libmemcached libmemcached-devel memcached-devel memcached
	systemctl enable memcached

memcached: memcached_install
	$(info =============================)
	$(info setup memcached)
	$(info this id is $(shell whoami) ....)
	$(info $(shell service memcached start))
	$(info $(shell service  memcached status))

unmemcached:
	yum remove -y libmemcached libmemcached-devel memcached-devel memcached




