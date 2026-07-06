# Portable Makefile for cbs32.dll and cbs64.dll using MinGW/TDM-GCC
# Tested for XP compatibility when using 32-bit target with -D_WIN32_WINNT=0x0501

CC32 ?= i686-w64-mingw32-gcc
CXX32 ?= i686-w64-mingw32-g++
CC64 ?= x86_64-w64-mingw32-gcc
CXX64 ?= x86_64-w64-mingw32-g++

CFLAGS_COMMON = -Os -s -Wall -Wextra -Wl,--kill-at -Wl,--add-stdcall-alias
INCLUDES = -Iinclude

# XP-compatible 32-bit build
CFLAGS32 = $(CFLAGS_COMMON) -D_WIN32_WINNT=0x0501 -static-libgcc
LDFLAGS32 =

# 64-bit build (Win7+), static libgcc for portability
CFLAGS64 = $(CFLAGS_COMMON) -D_WIN32_WINNT=0x0601 -static-libgcc
LDFLAGS64 =

SRC = src/cbs32.c
OBJ32 = build/cbs32_x86.o
OBJ64 = build/cbs32_x64.o

all: cbs32.dll cbs64.dll

build/cbs32_x86.o: $(SRC) include/cbs32.h | build
	$(CC32) -c $(SRC) -o $@ $(CFLAGS32) $(INCLUDES)

build/cbs32_x64.o: $(SRC) include/cbs32.h | build
	$(CC64) -c $(SRC) -o $@ $(CFLAGS64) $(INCLUDES)

cbs32.dll: $(OBJ32) cbs32.def
	$(CC32) -shared -o $@ $(OBJ32) cbs32.def $(CFLAGS32) $(LDFLAGS32)

cbs64.dll: $(OBJ64) cbs32.def
	$(CC64) -shared -o $@ $(OBJ64) cbs32.def $(CFLAGS64) $(LDFLAGS64)

build:
	mkdir -p build

clean:
	rm -f build/*.o cbs32.dll cbs64.dll
