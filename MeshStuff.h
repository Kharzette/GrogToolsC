#pragma once
#include	<stdint.h>
#include	<cglm/call.h>

typedef struct	GLTFFile_t			GLTFFile;
typedef struct	Character_t			Character;
typedef struct	Static_t			Static;
typedef struct	AnimLib_t			AnimLib;
typedef struct	StuffKeeper_t		StuffKeeper;
typedef struct	GraphicsDevice_t	GraphicsDevice;
typedef struct	Mesh_t				Mesh;
typedef struct	Skin_t				Skin;
typedef struct	Accessor_t			Accessor;
typedef struct	BufferView_t		BufferView;

Mesh	*MeshStuff_MakeMeshIndex(GraphicsDevice *pGD,
	const StuffKeeper *pSK,
	const struct json_object *pMeshes,
	const uint8_t *pBin, const Accessor *pAccs,
	const BufferView *pBVs, bool bFlipZ, int index);

void	MeshStuff_GetStaticPartTransforms(const struct json_object *pNodes,
	mat4 xForms[], int numParts);

Skin	*MeshStuff_GrabSkins(const struct json_object *pSkins,
	const uint8_t *pBin, const Accessor *pAccs,
	const BufferView *pBVs);