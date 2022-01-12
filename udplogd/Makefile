# * ---------------------
# * UTF-8 한글확인 용
# * ---------------------

TARGET = udplogd

#.SUFFIXES = .cpp .o
CXX=g++
CC=gcc

SRCS =  udplogd.c \
		logger.c

OBJS = $(SRCS:.c=.o)

CFLAGS_RELEASE = -O2 -fPIC
CFLAGS_DEBUG = -O0 -g -ggdb3 -DDEBUG
CFLAGS  = -Wall -Wextra -Wshadow -Wformat-security -Winit-self
CFLAGS += -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function
CFLAGS += -DUSE_MILISECOND_TIME

ifeq (release, $(findstring release, $(MAKECMDGOALS))) #"make clean release"으로 clean 다음에 연이어 release가 나올 수가 있다.
	CFLAGS += $(CFLAGS_RELEASE)
else
	CFLAGS += $(CFLAGS_DEBUG)
	TARGET = udplogd_d
endif

ifeq (debugtrace, $(findstring debugtrace, $(MAKECMDGOALS))) #"make clean debugtrace"으로 clean 다음에 연이어 debugtrace 나올 수가 있다.
	CFLAGS += -DDEBUGTRACE
endif

OS = $(shell uname -a | grep Debian | wc -l)

ifeq ($(OS), 1)
	CFLAGS += -DDEBIAN
endif

LFLAGS  = -lm -ldl

.PHONY: all clean debug release debugtrace install

all: $(TARGET) logclient
debug: all
release: all
debugtrace: all

%.o: %.c
	$(CC) -o $@ $< -c $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(LFLAGS) $(OBJS)
	mkdir -p ~/bin
	cp $@ ~/bin

logclient:
	$(CC) -o logclient.o logclient.c -c $(CFLAGS)
	$(CC) -o logclient logclient.o $(LFLAGS)

install:
	cp $(TARGET) ~/bin

clean:
	rm -f $(TARGET) *.o
	rm -f logclient


