# * ---------------------
# * UTF-8 한글확인 용
# * ---------------------
 
.SUFFIXES = .cpp .o
CXX=g++

# sha1.c, sha1.h 는 참조로 프로젝트에 추가만 한다, 실제는 openssl 것을 사용한다

PROGRAM = amidtmf_r
TARGET = amidtmf2_r
SRCS =  http.cpp \
		http_routes.cpp \
		websocket.cpp \
		ws_routes.cpp \
		dtmfversion.cpp \
		processevents.cpp \
		amiaction.cpp \
		amiproc.cpp \
		logger.cpp \
		util.cpp \
		WebConfig.cpp \
		amidtmf2.cpp
OBJS = $(SRCS:.cpp=.o)

CFLAGS_RELEASE = -O2 -fPIC
CFLAGS_DEBUG = -O0 -g -ggdb3 -DDEBUG -fno-default-inline
CFLAGS  = -Wall -Wextra -Wshadow -Wformat-security -Winit-self -fpermissive -Wno-strict-aliasing
CFLAGS += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function
CFLAGS += -DUSE_USEREVENT_CALLSTARTED -I../include
CFLAGS += -D_REENTRANT -D_PTHREADS

LFLAGS  = -lm -ldl -lpthread -lmemcached -lcrypto -L../lib 

ifeq (release, $(findstring release, $(MAKECMDGOALS))) #"make clean release"으로 clean 다음에 연이어 release가 나올 수가 있다.
	CFLAGS += $(CFLAGS_RELEASE)
	LFLAGS += -ltst -lAsyncThreadPool
else
	TARGET = amidtmf2_d
	PROGRAM = amidtmf_d
	CFLAGS += $(CFLAGS_DEBUG)
ifeq (debugtrace, $(findstring debugtrace, $(MAKECMDGOALS))) #"make clean debugtrace"으로 clean 다음에 연이어 debugtrace 나올 수가 있다.
	CFLAGS += -DDEBUGTRACE
	TARGET = amidtmf2_t
	PROGRAM = amidtmf_t
	LFLAGS += -ltst_t -lAsyncThreadPool_t
else
	LFLAGS += -ltst_d -lAsyncThreadPool_d
endif
endif

DATE = $(shell date +'char dtmfCompileDate[20] = "%Y-%m-%d %H:%M:%S";')

.PHONY: all clean debug release debugtrace check install trace

all: version $(OBJS) $(TARGET)

$(TARGET):
	$(CXX) -o $@ $(OBJS) $(LFLAGS)

debug: all
release: all
debugtrace: all

%.o: %.cpp
	$(CXX) -o $@ $< -c $(CFLAGS)

version:
	echo '$(DATE)' > dtmfversion.cpp
	echo `$(CXX) -v`

check:
	$(info os=$(OS))
	echo `g++ -v`

trace:
	$(info CFLAGS=$(CFLAGS))

install:
	mkdir -p ~/bin
	chmod 0755 dtmf
	cp -p dtmf ~/bin	# 복사하면 크론탭 멈출 수 있음
	cp $(PROGRAM).conf ~/bin
	cp $(TARGET) $(PROGRAM)
	cp $(PROGRAM) ~/bin

clean:
	rm -f $(TARGET) *.o


