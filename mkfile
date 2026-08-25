< /$objtype/mkfile

TARG=kterm
KRYON=/sys/src/kryon
BIN=/$objtype/bin
OUT=$O.out

CPPFLAGS=-I$KRYON/src/platform/plan9/include -I$KRYON/include -I./src \
	-DKRYON_BACKEND_LIBDRAW -DKRYON_PLATFORM_PLAN9 -DKRYON_NATIVE_PLAN9
CFLAGS=-FTVw

OFILES=\
	src/app_clipboard.$O\
	src/app_commands.$O\
	src/app_context_menu.$O\
	src/app_input.$O\
	src/app_menu.$O\
	src/app_profile.$O\
	src/app_search.$O\
	src/app_sessions.$O\
	src/app_terminal_view.$O\
	src/config.$O\
	src/input.$O\
	src/launch_options.$O\
	src/main.$O\
	src/palette.$O\
	src/profile.$O\
	src/selection.$O\
	src/session.$O\
	src/session_store.$O\
	src/terminal.$O\
	src/terminal_csi.$O\
	src/terminal_dcs.$O\
	src/terminal_keys.$O\
	src/terminal_modes.$O\
	src/terminal_mouse.$O\
	src/terminal_osc.$O\
	src/terminal_parser.$O\
	src/terminal_paste.$O\
	src/terminal_pty_plan9.$O\
	src/terminal_screen.$O\
	src/terminal_search.$O\
	src/terminal_sgr.$O\
	src/terminal_sixel.$O\
	src/terminal_text.$O\
	src/terminal_view.$O\

LIB=/$objtype/lib/libkryon.a /$objtype/lib/libstdio.a

all:V: $OUT

install:V: $BIN/$TARG

$BIN/$TARG: $OUT
	cp $OUT $BIN/$TARG

$OUT: $OFILES $LIB
	$LD -o $target $prereq -ldraw -lmemdraw -lthread

src/%.$O: src/%.c
	cd src && cpp -+ $CPPFLAGS $stem.c > $stem.i && $CC $CFLAGS -c $stem.i && mv $stem.i.$O $stem.$O && rm -f $stem.i

clean:V:
	rm -f src/*.[$OS] src/*.i [$OS].out $TARG
