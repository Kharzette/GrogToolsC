CC=gcc
CFLAGS=-std=gnu23 -g -O0 -march=native				\
	-DCGLM_FORCE_DEPTH_ZERO_TO_ONE					\
	-DCGLM_FORCE_LEFT_HANDED						\
	-I../GrogLibsC/uthash/src							\
	-I../GrogLibsC/dxvk-native/include/native/windows	\
	-I../GrogLibsC/dxvk-native/include/native/directx	\
	-I../GrogLibsC/cglm/include						\
	-I../GrogLibsC										\
	-I../nappgui_src/src								\
	-I../nappgui_src/src/osapp							\
	-I../nappgui_src/src/gui							\
	-Wall											\
	-Wl,-rpath='libs',--disable-new-dtags
#	-fsanitize=address								

SOURCES=$(wildcard *.c)
LIBS=-lgui -lcore -lcasino -ldraw2d -lgeom2d -losbs -losgui -lsewer -losapp -lUtilityLib -lInputLib -lMeshLib -lMaterialLib -lm -lSDL3 -lFAudio -ljson-c
LDFLAGS=-L../libs -L../nappgui_src/build/Debug/bin -L../GrogLibsC/libs

all: MeshConvert

MeshConvert: $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o MeshConvert $(LIBS)	\
	../GrogLibsC/dxvk-native/build/src/dxgi/libdxvk_dxgi.so	\
	../GrogLibsC/dxvk-native/build/src/d3d11/libdxvk_d3d11.so