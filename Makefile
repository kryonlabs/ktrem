CC ?= cc
ENGINE_DIR ?= ../kryon
BUILD_ROOT ?= build
PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
INSTALL ?= install

UNAME_S := $(shell uname -s 2>/dev/null)
UNAME_M := $(shell uname -m 2>/dev/null)
ifeq ($(UNAME_M),amd64)
    ARCH := x86_64
else
    ARCH := $(UNAME_M)
endif
ifeq ($(UNAME_S),Linux)
    PLATFORM := linux
else ifeq ($(UNAME_S),FreeBSD)
    PLATFORM := freebsd
else ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
else
    PLATFORM := $(UNAME_S)
endif

BUILD_DIR ?= $(BUILD_ROOT)/$(PLATFORM)-$(ARCH)
ENGINE_BUILD_DIR ?= $(ENGINE_DIR)/build/$(PLATFORM)-$(ARCH)
ENGINE_LIB = $(ENGINE_BUILD_DIR)/libkryon.a
RAYLIB_A = $(ENGINE_BUILD_DIR)/raylib/libraylib.a
LIBOQS_A = $(ENGINE_BUILD_DIR)/vendor/liboqs/lib/liboqs.a
CURL_A = $(ENGINE_BUILD_DIR)/vendor/curl/lib/libcurl.a
CMARK_A = $(ENGINE_BUILD_DIR)/vendor/cmark-gfm/src/libcmark-gfm.a
CMARK_EXT_A = $(ENGINE_BUILD_DIR)/vendor/cmark-gfm/extensions/libcmark-gfm-extensions.a
BOX2D_A = $(ENGINE_BUILD_DIR)/vendor/box2d/src/libbox2d.a

APP = $(BUILD_DIR)/bin/kapsule
TEST = $(BUILD_DIR)/tests/terminal_test
SRC_FILES := $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,$(BUILD_DIR)/src/%.o,$(SRC_FILES))
TEST_OBJS = $(BUILD_DIR)/tests/terminal_test.o $(BUILD_DIR)/src/terminal.o

RAY_SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
RAY_SDL_LDLIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
RAY_GL_CFLAGS ?= $(shell pkg-config --cflags libdrm gbm egl glesv2 2>/dev/null)
RAY_GL_LDLIBS ?= $(shell pkg-config --libs libdrm gbm egl glesv2 2>/dev/null)
RAY_LDLIBS ?= $(strip $(RAY_SDL_LDLIBS) $(RAY_GL_LDLIBS))
SYSTEM_THEME_PKG := $(shell if pkg-config --exists gtk+-3.0 2>/dev/null; then printf '%s' gtk+-3.0; fi)
SYSTEM_THEME_CFLAGS := $(shell if [ -n "$(SYSTEM_THEME_PKG)" ]; then pkg-config --cflags $(SYSTEM_THEME_PKG); fi)
SYSTEM_THEME_LDLIBS := $(shell if [ -n "$(SYSTEM_THEME_PKG)" ]; then pkg-config --libs $(SYSTEM_THEME_PKG); fi)
CURL_CODEC_LDLIBS ?= $(strip \
  $(shell pkg-config --libs libbrotlidec 2>/dev/null) \
  $(shell pkg-config --libs libbrotlicommon 2>/dev/null) \
  $(shell pkg-config --libs libzstd 2>/dev/null))
ifeq ($(PLATFORM),linux)
PLATFORM_LDLIBS ?= -ldl -lrt
else
PLATFORM_LDLIBS ?=
endif

CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS += -Isrc -I$(ENGINE_DIR)/include -I$(ENGINE_DIR)/vendor/clay \
	$(RAY_SDL_CFLAGS) $(RAY_GL_CFLAGS) $(SYSTEM_THEME_CFLAGS) \
	-DHAS_LIBOQS=1 -I$(ENGINE_BUILD_DIR)/vendor/liboqs/include \
	-DHAS_LIBCURL=1 -DCURL_STATICLIB -I$(ENGINE_BUILD_DIR)/vendor/curl/include \
	-DKRYON_HAS_CMARK_GFM=1 \
	-I$(ENGINE_DIR)/vendor/cmark-gfm/src -I$(ENGINE_DIR)/vendor/cmark-gfm/extensions \
	-I$(ENGINE_BUILD_DIR)/vendor/cmark-gfm/src -I$(ENGINE_BUILD_DIR)/vendor/cmark-gfm/extensions
LDLIBS += $(RAYLIB_A) $(BOX2D_A) $(RAY_LDLIBS) $(LIBOQS_A) \
	$(CURL_A) -lssl -lcrypto -lpthread $(CMARK_EXT_A) $(CMARK_A) \
	$(SYSTEM_THEME_LDLIBS) $(CURL_CODEC_LDLIBS) -lz -lm $(PLATFORM_LDLIBS)

.PHONY: all run test clean install engine

all: $(APP)

engine:
	$(MAKE) -C $(ENGINE_DIR) all

$(APP): engine $(OBJS) $(ENGINE_LIB) $(RAYLIB_A) | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(OBJS) \
		-Wl,--whole-archive $(ENGINE_LIB) -Wl,--no-whole-archive \
		$(LDLIBS)

$(TEST): $(TEST_OBJS) | $(BUILD_DIR)/tests
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_OBJS)

$(BUILD_DIR)/src/%.o: src/%.c src/*.h | $(BUILD_DIR)/src
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: tests/%.c src/terminal.h | $(BUILD_DIR)/tests
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/bin $(BUILD_DIR)/src $(BUILD_DIR)/tests:
	mkdir -p $@

run: $(APP)
	$(APP)

test: $(TEST)
	$(TEST)

install: $(APP)
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(APP) $(DESTDIR)$(BINDIR)/kapsule

clean:
	rm -rf $(BUILD_ROOT)
