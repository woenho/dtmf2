# * ---------------------
# * UTF-8 한글확인 용
# * ---------------------
 
#.SUFFIXES = .cpp .o
CXX=g++
CC=gcc

PROG = processmon_r
SRCS = ws_client.c logger.c
OBJS = $(SRCS:.c=.o)
CIVETWEB_LIB = libcivetweb.a

# civetweb system_common base compile
# make clean lib slib WITH_DEBUG=1 WITH_CPP=1  SSL_LIB=libssl.so.0 <-- openssl 1.0 버전사용
# make install-lib install-slib install-headers WITH_DEBUG=1 WITH_CPP=1  SSL_LIB=libssl.so.0 ==> -DSSL_LIB=\"libssl.so.0\"
# make clean lib slib WITH_DEBUG=1 WITH_CPP=1  SSL_LIB=libssl.so.1.1 <-- openssl 1.1 버전사용
# make install-lib install-slib install-headers WITH_DEBUG=1 WITH_CPP=1  SSL_LIB=libssl.so.1.1 ==> -DSSL_LIB=\"libssl.so.1.1\"
# ldconfig

CFLAGS_RELEASE = -O2 -fPIC
CFLAGS_DEBUG = -O0 -g -ggdb3 -DDEBUG
CFLAGS_CIVETWEB = -Iinclude -DLINUX -DUSE_STACK_SIZE=102400 -DOPENSSL_API_1_1 -DUSE_WEBSOCKET -DNO_SSL 

CFLAGS_CIVETWEB_LIB = $(CFLAGS_CIVETWEB)

CFLAGS  = -Wall -Wextra -Wshadow -Wformat-security -Winit-self
CFLAGS += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -Wno-format-truncation
CFLAGS += -DUSE_CHECK_RESP_TIME -DUSE_RESP_MSG -DUSE_RESPONSE_SYNC -DLOG_LOCK
CFLAGS += -Ic++/4.8.2
CFLAGS += $(CFLAGS_CIVETWEB)

ifeq (release, $(findstring release, $(MAKECMDGOALS))) #"make clean release"으로 clean 다음에 연이어 release가 나올 수가 있다.
	CFLAGS += $(CFLAGS_RELEASE)
	CFLAGS_CIVETWEB_LIB += $(CFLAGS_RELEASE)
else
	CFLAGS += $(CFLAGS_DEBUG) -DCON_DEBUG
	CFLAGS_CIVETWEB_LIB += $(CFLAGS_DEBUG)
ifeq (debugtrace, $(findstring debugtrace, $(MAKECMDGOALS))) #"make clean debugtrace"으로 clean 다음에 연이어 debugtrace 나올 수가 있다.
	CFLAGS += -DDEBUG_TRACE
	PROG = processmon_t
	CIVETWEB_LIB = libcivetweb_t.a
else
	PROG = processmon_d
	CIVETWEB_LIB = libcivetweb_d.a
endif
endif

OS = $(shell uname -a | grep Debian | wc -l)

ifeq ($(OS), 1)
	CFLAGS += -DDEBIAN
endif

LFLAGS  = -lm -ldl -lpthread -lcrypto

.PHONY: all clean release debug check debugtrace install

all: $(PROG)
release: all
debug: all
debugtrace: all

$(PROG): $(CIVETWEB_LIB) $(INFOBJS) $(OBJS)
	$(CXX) -o $@ $(LFLAGS) $(OBJS) $(INFOBJS) $(CIVETWEB_LIB)

%.o: %.c
	$(CC) -o $@ $< -c $(CFLAGS)

%.o: %.cpp
	$(CXX) -o $@ $< -c $(CFLAGS)

$(CIVETWEB_LIB):
	$(info $(CFLAGS_CIVETWEB_LIB))
	$(CC) -c src/civetweb.c -o civetweb.o $(CFLAGS_CIVETWEB_LIB)

	ar cq $@ civetweb.o

clean:
	rm -f $(CIVETWEB_LIB) $(PROG) processmon *.o

check:
	$(info os=$(OS))

trace:
	$(info CFLAGS=$(CFLAGS))

install:
	mkdir -p ~/bin
	chmod 0755 procmon
	cp procmon ~/bin	# 복사하면 크론탭 멈출 수 있음
	cp $(PROG) ~/bin


	