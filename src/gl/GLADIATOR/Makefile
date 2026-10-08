# Makefile - Lorica GL31 (OpenGL 3.1 core sobre GLES 3.0+)
#
#   make                 biblioteca estatica  liblorica_gl31.a
#   make SHARED=1        biblioteca compartida liblorica_gl31.so
#   make test            compila y corre las pruebas (backend simulado, no necesita GPU)
#   make SYSTEM_GLES=1   usar los headers GLES del sistema en vez de third_party/khronos
#   make PLATFORM_HEADER=mi_gles.h   header GLES propio (define LORICA_GL31_PLATFORM_HEADER)

CC       ?= gcc
AR       ?= ar
CFLAGS   ?= -std=gnu11 -Wall -Wextra -O2 -fPIC
CPPFLAGS ?=
PREFIX   ?= /usr/local

SOURCES = \
	gl31.c \
	gl31_attrib.c \
	gl31_buffer.c \
	gl31_caps.c \
	gl31_compute.c \
	gl31_draw.c \
	gl31_fbo.c \
	gl31_gl4.c gl31_dsa.c \
	gl31_query.c \
	gl31_render.c \
	gl31_shader_glsl.c \
	gl31_shader_link.c \
	gl31_state.c \
	gl31_sync.c \
	gl31_stubs.c \
	gl31_texture.c \
	gl31_uniform.c \
	gl31_vao.c \
	gl31_xfb.c

OBJECTS    = $(SOURCES:.c=.o)
STATIC_LIB = liblorica_gl31.a
SHARED_LIB = liblorica_gl31.so

ifndef SYSTEM_GLES
CPPFLAGS += -Ithird_party/khronos
endif
ifdef PLATFORM_HEADER
CPPFLAGS += -DLORICA_GL31_PLATFORM_HEADER=\"$(PLATFORM_HEADER)\"
endif

ifeq ($(SHARED),1)
TARGET = $(SHARED_LIB)
else
TARGET = $(STATIC_LIB)
endif

all: $(TARGET)

$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs $@ $^

$(SHARED_LIB): $(OBJECTS)
	$(CC) -shared -o $@ $^

%.o: %.c gl31.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# ---- pruebas ----
TESTS = tests/test_uniform tests/test_integration tests/test_gl32 tests/test_gl33 tests/test_gl4 tests/test_dsa

tests/%: tests/%.c gl31.h $(STATIC_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. $< $(STATIC_LIB) -lpthread -o $@

test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

clean:
	rm -f $(OBJECTS) $(STATIC_LIB) $(SHARED_LIB) $(TESTS)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include
	install -m 644 $(TARGET) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 gl31.h $(DESTDIR)$(PREFIX)/include/

.PHONY: all test clean install
