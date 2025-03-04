#pragma once

typedef struct	GLTFFile_t	GLTFFile;

GLTFFile	*GLTF_Create(const char *szFileName);
GLTFFile	*GLTF_CreateFromGLB(const char *szFileName);