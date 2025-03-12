#pragma once
#include	<cglm/call.h>

typedef struct	GLTFFile_t
{
	struct json_object	*mpJSON;
	uint8_t				*mpBinChunk;

	//vert format data
	int	*mpElements;
	int	*mpElAccess;
	int	mNumElements;

}	GLTFFile;

GLTFFile	*GLTF_Create(const char *szFileName);
GLTFFile	*GLTF_CreateFromGLB(const char *szFileName);
void		GLTF_GetVec3(const struct json_object *pVec, vec3 vec);
void		GLTF_GetVec4(const struct json_object *pVec, vec4 vec);