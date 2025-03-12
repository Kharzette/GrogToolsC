#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<stdint.h>
#include	<stdbool.h>
#include	"UtilityLib/StringStuff.h"
#include	"UtilityLib/ListStuff.h"
#include	"UtilityLib/GraphicsDevice.h"
#include	"UtilityLib/MiscStuff.h"
#include	"MeshLib/Character.h"
#include	"MeshLib/Static.h"
#include	"MeshLib/GSNode.h"
#include	"MeshLib/Skin.h"
#include	"MeshLib/Mesh.h"
#include	"MeshLib/AnimLib.h"
#include	"MeshLib/Anim.h"
#include	"MeshLib/SubAnim.h"
#include	"MaterialLib/Layouts.h"
#include	"MaterialLib/StuffKeeper.h"
#include	<json-c/json_tokener.h>
#include	<json-c/json_object.h>
#include	<json-c/json_object_iterator.h>
#include	<json-c/linkhash.h>
#include	"json-c/arraylist.h"
#include	"json-c/json_util.h"
#include	"AnimStuff.h"
#include	"MeshStuff.h"
#include	"glTFTypes.h"
#include	"glTFFile.h"


static int	sGetSceneNodeCount(const struct json_object *pScenes)
{
	const struct json_object	*pArr	=json_object_array_get_idx(pScenes, 0);

	json_object_object_foreach(pArr, pKey, pVal)
	{
		enum json_type	t	=json_object_get_type(pVal);
		if(t == json_type_array)
		{
			const struct json_object	*pCount	=json_object_array_get_idx(pVal, 0);
			return	json_object_get_int(pCount);
		}
		printf("KeyValue: %s : %s\n", pKey, json_object_get_string(pVal));
	}
	return	-1;
}

static Accessor	*sReadAccessors(const struct json_object *pAcc)
{
	int	numAcc	=json_object_array_length(pAcc);

	Accessor	*pRet	=malloc(sizeof(Accessor) * numAcc);

	for(int i=0;i < numAcc;i++)
	{
		printf("Accessor %d\n", i);

		const struct json_object	*pArr	=json_object_array_get_idx(pAcc, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{
			enum json_type	t	=json_object_get_type(pVal);
			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
				json_object_get_string(pVal));
			
			if(0 == strncmp("bufferView", pKey, 10))
			{
				assert(t == json_type_int);

				pRet[i].mBufferView	=json_object_get_int(pVal);
			}
			else if(0 == strncmp("componentType", pKey, 13))
			{
				assert(t == json_type_int);

				pRet[i].mComponentType	=json_object_get_int(pVal);
			}
			else if(0 == strncmp("count", pKey, 5))
			{
				assert(t == json_type_int);

				pRet[i].mCount	=json_object_get_int(pVal);
			}
			else if(0 == strncmp("min", pKey, 3))
			{
				assert(t == json_type_array);

				GLTF_GetVec3(pVal, pRet[i].mMin);
			}
			else if(0 == strncmp("max", pKey, 3))
			{
				assert(t == json_type_array);

				GLTF_GetVec3(pVal, pRet[i].mMax);
			}
			else if(0 == strncmp("type", pKey, 4))
			{
				assert(t == json_type_string);

				const char	*pType	=json_object_get_string(pVal);

				if(0 == strncmp("SCALAR", pType, 6))
				{
					pRet[i].mType	=TYPE_SCALAR;
				}
				else if(0 == strncmp("VEC2", pType, 4))
				{
					pRet[i].mType	=TYPE_VEC2;
				}
				else if(0 == strncmp("VEC3", pType, 4))
				{
					pRet[i].mType	=TYPE_VEC3;
				}
				else if(0 == strncmp("VEC4", pType, 4))
				{
					pRet[i].mType	=TYPE_VEC4;
				}
				else if(0 == strncmp("MAT2", pType, 4))
				{
					pRet[i].mType	=TYPE_MAT2;
				}
				else if(0 == strncmp("MAT3", pType, 4))
				{
					pRet[i].mType	=TYPE_MAT3;
				}
				else if(0 == strncmp("MAT4", pType, 4))
				{
					pRet[i].mType	=TYPE_MAT4;
				}
				else
				{
					assert(false);
				}
			}
			else
			{
				assert(false);
			}
		}
	}
	return	pRet;
}

static BufferView	*sGetBufferViews(const struct json_object *pBV)
{
	int	numBV	=json_object_array_length(pBV);

	BufferView	*pRet	=malloc(sizeof(BufferView) * numBV);

	memset(pRet, 0, sizeof(BufferView) * numBV);

	for(int i=0;i < numBV;i++)
	{
		printf("BufferView %d\n", i);

		const struct json_object	*pArr	=json_object_array_get_idx(pBV, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{
			enum json_type	t	=json_object_get_type(pVal);
			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
				json_object_get_string(pVal));
			
			if(0 == strncmp("buffer", pKey, 6))
			{
				assert(t == json_type_int);

				pRet[i].mBufIdx	=json_object_get_int(pVal);
			}
			else if(0 == strncmp("byteLength", pKey, 10))
			{
				assert(t == json_type_int);

				pRet[i].mByteLength	=json_object_get_int(pVal);
			}
			else if(0 == strncmp("byteOffset", pKey, 10))
			{
				assert(t == json_type_int);

				pRet[i].mByteOffset	=json_object_get_int(pVal);
			}
			else if(0 == strncmp("target", pKey, 6))
			{
				assert(t == json_type_int);

				pRet[i].mTarget	=json_object_get_int(pVal);
			}
			else
			{
				assert(false);
			}
		}
	}
	return	pRet;
}


Character	*GLCV_ExtractChar(GraphicsDevice *pGD,
	const AnimLib *pALib,
	const StuffKeeper *pSK, const GLTFFile *pGF)
{
	assert(pGF != NULL);

	//characters have:
	//base transform
	//skin
	//bound
	//mesh parts

	const struct json_object	*pAcc	=json_object_object_get(pGF->mpJSON, "accessors");
	if(pAcc == NULL)
	{
		printf("gltf has no accessors.\n");
		return	NULL;
	}

	Accessor	*pAcs	=sReadAccessors(pAcc);

	const struct json_object	*pBV	=json_object_object_get(pGF->mpJSON, "bufferViews");
	if(pBV == NULL)
	{
		printf("gltf has no bufferViews.\n");
		return	NULL;
	}

	BufferView	*pBVs	=sGetBufferViews(pBV);

	const struct json_object	*pNodes	=json_object_object_get(pGF->mpJSON, "nodes");
	if(pNodes == NULL)
	{
		printf("gltf has no nodes.\n");
		return	NULL;
	}

	const struct json_object	*pSkins	=json_object_object_get(pGF->mpJSON, "skins");
	if(pSkins == NULL)
	{
		printf("gltf has no skins.\n");
		return	NULL;
	}

	Skin		*pSkin	=MeshStuff_GrabSkins(pSkins,
							pGF->mpBinChunk, pAcs, pBVs);
	Skeleton	*pSkel	=AnimStuff_GrabSkeleton(pNodes,
							pSkins, pGF->mpBinChunk, pAcs, pBVs);

	const SkellyMap	*pSMap	=NULL;
	if(pALib == NULL)
	{
		printf("Warning! No animlib so character bone indexes might mismatch later!\n");
	}
	else
	{
		pSMap	=AnimLib_GetMapping(pALib, pSkel);
	}

	const struct json_object	*pMeshes	=json_object_object_get(pGF->mpJSON, "meshes");
	if(pMeshes == NULL)
	{
		printf("gltf has no meshes.\n");
		return	NULL;
	}

	int	numMeshes	=json_object_array_length(pMeshes);

	Mesh	*pMeshArr[numMeshes];

	for(int i=0;i < numMeshes;i++)
	{
		pMeshArr[i]	=MeshStuff_MakeMeshIndex(pGD, pSK, pMeshes,
						pGF->mpBinChunk, pAcs, pBVs, pSMap, false, i);
	}

	Character	*pChar	=Character_Create(pSkin, pMeshArr, numMeshes);

	return	pChar;
}

Static	*GLCV_ExtractStatic(GraphicsDevice *pGD,
	const StuffKeeper *pSK, const GLTFFile *pGF)
{
	assert(pGF != NULL);

	const struct json_object	*pAcc	=json_object_object_get(pGF->mpJSON, "accessors");
	if(pAcc == NULL)
	{
		printf("gltf has no accessors.\n");
		return	NULL;
	}

	Accessor	*pAcs	=sReadAccessors(pAcc);

	const struct json_object	*pBV	=json_object_object_get(pGF->mpJSON, "bufferViews");
	if(pBV == NULL)
	{
		printf("gltf has no bufferViews.\n");
		return	NULL;
	}

	BufferView	*pBVs	=sGetBufferViews(pBV);

	const struct json_object	*pMeshes	=json_object_object_get(pGF->mpJSON, "meshes");
	if(pMeshes == NULL)
	{
		printf("gltf has no meshes.\n");
		return	NULL;
	}

	const struct json_object	*pNodes	=json_object_object_get(pGF->mpJSON, "nodes");
	if(pNodes == NULL)
	{
		printf("gltf has no nodes.\n");
		return	NULL;
	}
	
	int	numMeshes	=json_object_array_length(pMeshes);

	Mesh	*pMeshArr[numMeshes];

	for(int i=0;i < numMeshes;i++)
	{
		pMeshArr[i]	=MeshStuff_MakeMeshIndex(pGD, pSK, pMeshes,
						pGF->mpBinChunk, pAcs, pBVs, NULL, true, i);
	}

	mat4	xForms[numMeshes];

	MeshStuff_GetPartTransforms(pNodes, xForms, true, numMeshes);

	Static	*pStat	=Static_Create(pMeshArr, xForms, numMeshes);

	return	pStat;
}

void	GLCV_ExtractAndAddAnimation(const GLTFFile *pGF, AnimLib **ppALib)
{
	assert(pGF != NULL);
	assert(ppALib != NULL);

	const struct json_object	*pAcc	=json_object_object_get(pGF->mpJSON, "accessors");
	if(pAcc == NULL)
	{
		printf("gltf has no accessors.\n");
		return;
	}

	Accessor	*pAccs	=sReadAccessors(pAcc);

	const struct json_object	*pBV	=json_object_object_get(pGF->mpJSON, "bufferViews");
	if(pBV == NULL)
	{
		printf("gltf has no bufferViews.\n");
		return;
	}

	BufferView	*pBVs	=sGetBufferViews(pBV);

	const struct json_object	*pScenes	=json_object_object_get(pGF->mpJSON, "scenes");
	int	numNodes	=sGetSceneNodeCount(pScenes);
	if(numNodes < 1)
	{
		printf("gltf has no nodes.\n");
		return;
	}

	const struct json_object	*pNodes	=json_object_object_get(pGF->mpJSON, "nodes");
	if(pNodes == NULL)
	{
		printf("gltf has no nodes.\n");
		return;
	}

	const struct json_object	*pSkins	=json_object_object_get(pGF->mpJSON, "skins");
	if(pSkins == NULL)
	{
		printf("gltf has no skins.\n");
		return;
	}

	if(numNodes != json_object_array_length(pNodes))
	{
		printf("Warning!  Scenes node count != nodes array length!\n");
	}

	Skeleton	*pSkel	=AnimStuff_GrabSkeleton(pNodes, pSkins, pGF->mpBinChunk, pAccs, pBVs);

	const struct json_object	*pAnims	=json_object_object_get(pGF->mpJSON, "animations");
	if(pAnims == NULL)
	{
		printf("gltf has no animations.\n");
		return;
	}

	bool	bNeedRemap	=false;
	if(*ppALib == NULL)
	{
		*ppALib		=AnimLib_Create(pSkel);
		bNeedRemap	=true;
	}

	int	numAnims	=json_object_array_length(pAnims);
	for(int i=0;i < numAnims;i++)
	{
		const struct json_object	*pArr	=json_object_array_get_idx(pAnims, i);

		Anim	*pAnim	=AnimStuff_GrabAnim(pArr, pAccs, pBVs, pGF->mpBinChunk, pSkel);

		if(bNeedRemap)
		{
			AnimLib_AddForeign(*ppALib, pAnim, pSkel);
		}
		else
		{
			AnimLib_Add(*ppALib, pAnim);
		}
	}
}