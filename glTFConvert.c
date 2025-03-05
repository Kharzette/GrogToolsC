#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<stdint.h>
#include	<stdbool.h>
#include	"UtilityLib/StringStuff.h"
#include	"MeshLib/Character.h"
#include	"MeshLib/GSNode.h"
#include	"MeshLib/Skin.h"
#include	<json-c/json_tokener.h>
#include	<json-c/json_object.h>
#include	<json-c/json_object_iterator.h>
#include	<json-c/linkhash.h>
#include	"json-c/arraylist.h"
#include	"json-c/json_util.h"

#define	TYPE_SCALAR	0
#define	TYPE_VEC2	1
#define	TYPE_VEC3	2
#define	TYPE_VEC4	3
#define	TYPE_MAT2	4
#define	TYPE_MAT3	5
#define	TYPE_MAT4	6


typedef struct	GLTFFile_t
{
	struct json_object	*mpJSON;
	uint8_t				*mpBinChunk;
}	GLTFFile;

typedef struct	Accessor_t
{
	int	mType;
	int	mComponentType;
	int	mCount;
	int	mBufferView;

	vec3	mMin, mMax;
}	Accessor;

typedef struct	BufferView_t
{
	int	mBufIdx;
	int	mByteLength;
	int	mByteOffset;
	int	mTarget;
}	BufferView;


static void	sGetVec4(const struct json_object *pVec, vec4 vec)
{
	assert(json_object_get_type(pVec) == json_type_array);

	for(int i=0;i < 4;i++)
	{
		const struct json_object	*pVal	=json_object_array_get_idx(pVec, i);

		vec[i]	=json_object_get_double(pVal);
	}
}

static void	sGetVec3(const struct json_object *pVec, vec3 vec)
{
	assert(json_object_get_type(pVec) == json_type_array);

	for(int i=0;i < 3;i++)
	{
		const struct json_object	*pVal	=json_object_array_get_idx(pVec, i);

		vec[i]	=json_object_get_double(pVal);
	}
}

//return an array of GSNode
static GSNode	*sMakeNodes(const struct json_object *pNodes)
{
	int	numNodes	=json_object_array_length(pNodes);

	GSNode	*pGNs	=malloc(sizeof(GSNode) * numNodes);

	int	meshIdx	=-1;
	int	skinIdx	=-1;

	for(int i=0;i < numNodes;i++)
	{
		vec3		scale	={1,1,1};
		vec4		rot		={0,0,0,1};
		vec3		trans	={0,0,0};
		UT_string	*pName	=NULL;

		int		numKids		=0;

		printf("Node %d\n", i);

		const struct json_object	*pArr	=json_object_array_get_idx(pNodes, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{
			enum json_type	t	=json_object_get_type(pVal);
			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
				json_object_get_string(pVal));
			
			if(0 == strncmp("name", pKey, 4))
			{
				assert(t == json_type_string);

				utstring_new(pName);

				utstring_printf(pName, "%s", json_object_get_string(pVal));
			}
			else if(0 == strncmp("rotation", pKey, 8))
			{
				assert(t == json_type_array);

				sGetVec4(pVal, rot);
			}
			else if(0 == strncmp("scale", pKey, 8))
			{
				assert(t == json_type_array);

				sGetVec3(pVal, scale);
			}
			else if(0 == strncmp("translation", pKey, 8))
			{
				assert(t == json_type_array);

				sGetVec3(pVal, trans);
			}
			else if(0 == strncmp("children", pKey, 8))
			{
				assert(t == json_type_array);

				numKids	=json_object_array_length(pVal);
			}
			else if(0 == strncmp("mesh", pKey, 4))
			{
				assert(t == json_type_int);

				meshIdx	=json_object_get_int(pVal);
			}
			else if(0 == strncmp("skin", pKey, 4))
			{
				assert(t == json_type_int);

				skinIdx	=json_object_get_int(pVal);
			}
			else
			{
				assert(false);
			}
		}

		pGNs[i].szName			=pName;
		pGNs[i].mNumChildren	=numKids;
		pGNs[i].mIndex			=i;

		if(numKids > 0)
		{
			pGNs[i].mpChildren	=malloc(sizeof(GSNode) * numKids);
		}
		else
		{
			pGNs[i].mpChildren	=NULL;
		}

		glm_vec3_copy(trans, pGNs[i].mKeyValue.mPosition);
		glm_vec3_copy(scale, pGNs[i].mKeyValue.mScale);
		glm_vec4_copy(rot, pGNs[i].mKeyValue.mRotation);
	}

	//fix up the children
	for(int i=0;i < numNodes;i++)
	{
		if(pGNs[i].mNumChildren <= 0)
		{
			continue;
		}

		const struct json_object	*pArr	=json_object_array_get_idx(pNodes, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{			
			enum json_type	t	=json_object_get_type(pVal);

			if(0 == strncmp("children", pKey, 8))
			{
				assert(t == json_type_array);

				for(int j=0;j < pGNs[i].mNumChildren;j++)
				{
					const struct json_object	*pIdx	=
						json_object_array_get_idx(pVal, i);

					pGNs[i].mpChildren[j]	=&pGNs[json_object_get_int(pIdx)];
				}
			}
		}
	}
	return	pGNs;
}

static int	sGetNodeCount(const struct json_object *pScenes)
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
		UT_string	*pName	=NULL;

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

				sGetVec3(pVal, pRet[i].mMin);
			}
			else if(0 == strncmp("max", pKey, 3))
			{
				assert(t == json_type_array);

				sGetVec3(pVal, pRet[i].mMax);
			}
			else if(0 == strncmp("type", pKey, 4))
			{
				assert(t == json_type_string);

				char	*pType	=json_object_get_string(pVal);

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
		UT_string	*pName	=NULL;

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

static mat4	*sGetIBP(const uint8_t *pBin, const Accessor *pAcc,
					const BufferView *pBVs)
{
#ifdef __AVX__
	mat4	*pRet	=aligned_alloc(32, sizeof(mat4) * pAcc->mCount);
#else
	mat4	*pRet	=aligned_alloc(16, sizeof(mat4) * pAcc->mCount);
#endif

	const BufferView	*pBV	=&pBVs[pAcc->mBufferView];

	assert(pBV->mBufIdx == 0);

	memcpy(pRet, &pBin[pBV->mByteOffset], pBV->mByteLength);

	return	pRet;
}

static Skin	*sMakeSkins(const struct json_object *pSkins,
						const uint8_t *pBin, const Accessor *pAccs,
						const BufferView *pBVs)
{
	int	numSkins	=json_object_array_length(pSkins);

	assert(numSkins == 1);

	Skin	*pRet	=NULL;
	
//	GSNode	*pGNs	=malloc(sizeof(GSNode) * numNodes);

	for(int i=0;i < numSkins;i++)
	{
		int			ibmAccessIndex	=-1;
		UT_string	*pName			=NULL;
		int			*pJoints		=NULL;
		mat4		*pIBPs			=NULL;

		printf("Skin %d\n", i);

		const struct json_object	*pArr	=json_object_array_get_idx(pSkins, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{
			enum json_type	t	=json_object_get_type(pVal);
			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
				json_object_get_string(pVal));
			
			if(0 == strncmp("inverseBindMatrices", pKey, 19))
			{
				assert(t == json_type_int);

				ibmAccessIndex	=json_object_get_int(pVal);

				pIBPs	=sGetIBP(pBin, &pAccs[ibmAccessIndex], pBVs);
			}
			else if(0 == strncmp("joints", pKey, 19))
			{
				assert(t == json_type_array);

				int	jarrLen	=json_object_array_length(pVal);

				pJoints	=malloc(sizeof(int) * jarrLen);

				for(int j=0;j < jarrLen;j++)
				{
					const struct json_object	*pJIdx	=
						json_object_array_get_idx(pVal, j);

					pJoints[j]	=json_object_get_int(pJIdx);
				}
			}
			else if(0 == strncmp("name", pKey, 4))
			{
				assert(t == json_type_string);

				utstring_new(pName);

				utstring_printf(pName, "%s", json_object_get_string(pVal));
			}
		}
		pRet	=Skin_Create(pIBPs, pAccs[ibmAccessIndex].mCount);
	}

	return	pRet;
}


Character	*GLCV_ExtractCharacter(const GLTFFile *pGF)
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

	const struct json_object	*pScenes	=json_object_object_get(pGF->mpJSON, "scenes");
	int	numNodes	=sGetNodeCount(pScenes);
	if(numNodes < 1)
	{
		printf("gltf has no nodes.\n");
		return	NULL;
	}

	const struct json_object	*pNodes	=json_object_object_get(pGF->mpJSON, "nodes");
	if(pNodes == NULL)
	{
		printf("gltf has no nodes.\n");
		return	NULL;
	}

	if(numNodes != json_object_array_length(pNodes))
	{
		printf("Warning!  Scenes node count != nodes array length!\n");
	}

	GSNode	*pGSNs	=sMakeNodes(pNodes);

	const struct json_object	*pSkins	=json_object_object_get(pGF->mpJSON, "skins");
	if(pSkins == NULL)
	{
		printf("gltf has no skins.\n");
		return	NULL;
	}

	Skin	*pSkin	=sMakeSkins(pSkins, pGF->mpBinChunk, pAcs, pBVs);

}