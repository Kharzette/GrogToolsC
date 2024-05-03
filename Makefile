CC=gcc
CFLAGS=-std=gnu2x -g -O0 -march=native	\
	-DCGLM_FORCE_DEPTH_ZERO_TO_ONE		\
	-DCGLM_FORCE_LEFT_HANDED			\
	-Inappgui_src/src					\
	-Inappgui_src/src/osapp				\
	-Inappgui_src/src/gui				\
	-Wall								\
	-Wl,-rpath='libs',--disable-new-dtags

SOURCES=$(wildcard *.c)
LIBS=-lgui -lcore -lcasino -ldraw2d -lgeom2d -losbs -losgui -lsewer -losapp
LDFLAGS=-Llibs -Lnappgui_src/build/Debug/bin

all: ColladaConvert

ColladaConvert: $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o ColladaConvert $(LIBS)