APP_NAME = task_finder

CC_LINUX ?= gcc
CC_WIN ?= x86_64-w64-mingw32-gcc

CFLAGS_COMMON = -I./src -O2 -Wall -Wno-missing-braces

CFLAGS_LINUX = $(CFLAGS_COMMON) -I./lib/raylib/linux
LDFLAGS_LINUX = -L./lib/raylib/linux -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

CFLAGS_WIN = $(CFLAGS_COMMON) -I./lib/raylib/win
LDFLAGS_WIN = -L./lib/raylib/win -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -Wl,--stack,16777216

SRCS = src/main.c
HEADERS = src/score.h

.PHONY: all linux windows clean

all: linux windows

linux: $(APP_NAME)

windows: $(APP_NAME).exe

$(APP_NAME): $(SRCS) $(HEADERS)
	$(CC_LINUX) $(CFLAGS_LINUX) $(SRCS) -o $@ $(LDFLAGS_LINUX)
	@echo "==> Linux binarka uspesne sestavena: $(APP_NAME)"

$(APP_NAME).exe: $(SRCS) $(HEADERS)
	$(CC_WIN) $(CFLAGS_WIN) $(SRCS) -o $@ $(LDFLAGS_WIN)
	@echo "==> Windows binarka uspesne sestavena: $(APP_NAME).exe"

clean:
	rm -f $(APP_NAME) $(APP_NAME).exe
