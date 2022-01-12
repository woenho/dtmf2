# * ---------------------
# * UTF-8 한글확인 용
# * ---------------------
 
.SUFFIXES = .cpp .o
CXX=g++

PROGRAM = amidtmf
TARGET = amidtmf2
SRCS =  http.cpp \
		processevents.cpp \
		amiaction.cpp \
		amiproc.cpp \
		routes.cpp \
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

LFLAGS  = -lm -ldl -lpthread -lmemcached -ljson-c -lcrypto -L../lib 

ifeq (release, $(findstring release, $(MAKECMDGOALS))) #"make clean release"으로 clean 다음에 연이어 release가 나올 수가 있다.
	CFLAGS += $(CFLAGS_RELEASE)
	LFLAGS += -ltst -lAsyncThreadPool
else
	CFLAGS += $(CFLAGS_DEBUG)
	LFLAGS += -ltst_d -lAsyncThreadPool_d
ifeq (debugtrace, $(findstring debugtrace, $(MAKECMDGOALS))) #"make clean debugtrace"으로 clean 다음에 연이어 debugtrace 나올 수가 있다.
	CFLAGS += -DDEBUGTRACE
endif
endif

.PHONY: all clean debug release debugtrace check install trace

all: $(OBJS) $(TARGET)

$(TARGET):
	$(CXX) -o $@ $(OBJS) $(LFLAGS)

debug: all
release: all
debugtrace: all

%.o: %.cpp
	$(CXX) -o $@ $< -c $(CFLAGS)

check:
	$(info os=$(OS))
	echo `g++ -v`

trace:
	$(info CFLAGS=$(CFLAGS))

install:
	mkdir -p ~/bin
	chmod 0777 dtmf
	cp -p dtmf ~/bin	# 복사하면 크론탭 멈출 수 있음
	cp $(PROGRAM).conf ~/bin
	cp $(TARGET) $(PROGRAM)
	cp $(PROGRAM) ~/bin

clean:
	rm -f $(TARGET) *.o


