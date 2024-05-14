CC=gcc
CFLAGS=-std=gnu2x -g -O0 -march=native				\
	-DCGLM_FORCE_DEPTH_ZERO_TO_ONE					\
	-DCGLM_FORCE_LEFT_HANDED						\
	-IGrogLibsC/SDL/include							\
	-IGrogLibsC/uthash/src							\
	-IGrogLibsC/dxvk-native/include/native/windows	\
	-IGrogLibsC/dxvk-native/include/native/directx	\
	-IGrogLibsC/cglm/include						\
	-IGrogLibsC										\
	-Inappgui_src/src								\
	-Inappgui_src/src/osapp							\
	-Inappgui_src/src/gui							\
	-Wall											\
	-Wl,-rpath='libs',--disable-new-dtags

SOURCES=$(wildcard *.c)
LIBS=-lgui -lcore -lcasino -ldraw2d -lgeom2d -losbs -losgui -lsewer -losapp -lUtilityLib -lInputLib -lMeshLib -lMaterialLib -lm
LDFLAGS=-Llibs -Lnappgui_src/build/Debug/bin -LGrogLibsC/libs

all: ColladaConvert

ColladaConvert: $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o ColladaConvert $(LIBS)	\
	GrogLibsC/SDL/build/libSDL3.so	\
	GrogLibsC/libpng/build/libpng.so	\
	GrogLibsC/AudioLib/FAudio/build/libFAudio.so	\
	GrogLibsC/dxvk-native/build/src/dxgi/libdxvk_dxgi.so	\
	GrogLibsC/dxvk-native/build/src/d3d11/libdxvk_d3d11.so