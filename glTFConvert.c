#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<stdint.h>
#include	<stdbool.h>
#include	"uthash.h"
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

#define	TYPE_SCALAR	0
#define	TYPE_VEC2	1
#define	TYPE_VEC3	2
#define	TYPE_VEC4	3
#define	TYPE_MAT2	4
#define	TYPE_MAT3	5
#define	TYPE_MAT4	6

#define	CTYPE_BYTE		5120
#define	CTYPE_UBYTE		5121
#define	CTYPE_SHORT		5122
#define	CTYPE_USHORT	5123
#define	CTYPE_UINT		5125
#define	CTYPE_FLOAT		5126


typedef struct	GLTFFile_t
{
	struct json_object	*mpJSON;
	uint8_t				*mpBinChunk;

	//vert format data
	int	*mpElements;
	int	*mpElAccess;
	int	mNumElements;

}	GLTFFile;

typedef struct	VertFormat_t
{
	//vert format data
	int	*mpElements;
	int	*mpElAccess;
	int	mNumElements;

}	VertFormat;

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

static Skeleton	*sMakeSkeleton(const struct json_object *pNodes,
								const struct json_object *pSkins,
								const uint8_t *pBin, const Accessor *pAccs,
								const BufferView *pBVs)
							{
	int	numNodes	=json_object_array_length(pNodes);
	int	numSkins	=json_object_array_length(pSkins);

	assert(numSkins == 1);
	
	int		ibmAccessIndex	=-1;
	int		*pJoints		=NULL;
	mat4	*pIBPs			=NULL;
	
	const struct json_object	*pArr	=json_object_array_get_idx(pSkins, 0);
	
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
		}
	}

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
			pGNs[i].mpChildren	=malloc(sizeof(GSNode *) * numKids);
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
						json_object_array_get_idx(pVal, j);

					pGNs[i].mpChildren[j]	=&pGNs[json_object_get_int(pIdx)];
				}
			}
		}
	}

	return	Skeleton_Create(&pGNs[pJoints[0]]);
}

//for nodes that are just tied to static mesh parts
static void	sGetPartTransforms(const struct json_object	*pNodes,
	mat4 xForms[], bool bFlipZTranslations, int numParts)
{
	int	numNodes	=json_object_array_length(pNodes);

	int	meshIdx	=-1;
	int	skinIdx	=-1;

	for(int i=0;i < numNodes;i++)
	{
		vec3		scale	={1,1,1};
		vec4		rot		={0,0,0,1};
		vec3		trans	={0,0,0};
		GSNode		gsNode	={0};

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
			}
			else if(0 == strncmp("rotation", pKey, 8))
			{
				assert(t == json_type_array);

				sGetVec4(pVal, rot);

				if(bFlipZTranslations)
				{
					rot[0]	=-rot[0];
					rot[1]	=-rot[1];
				}
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

				if(bFlipZTranslations)
				{
					trans[2]	=-trans[2];
				}
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

		glm_vec3_copy(trans, gsNode.mKeyValue.mPosition);
		glm_vec3_copy(scale, gsNode.mKeyValue.mScale);
		glm_vec4_copy(rot, gsNode.mKeyValue.mRotation);

		if(bFlipZTranslations)
		{
			KeyFrame_GetMatrixOtherWay(&gsNode.mKeyValue, xForms[meshIdx]);
		}
		else
		{
			KeyFrame_GetMatrix(&gsNode.mKeyValue, xForms[meshIdx]);
		}
	}
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

static void	sFillVertFormat(const struct json_object *pVAttr, VertFormat *pVF)
{
	int	numElems	=0;

	//count up how many vertex elements
	json_object_object_foreach(pVAttr, pCountKey, pCountVal)
	{
		numElems++;
	}

	//save discovered vert format details to the struct
	pVF->mpElements		=malloc(sizeof(int) * numElems);
	pVF->mpElAccess		=malloc(sizeof(int) * numElems);
	pVF->mNumElements	=numElems;

	int	elIdx	=0;
	json_object_object_foreach(pVAttr, pKey, pVal)
	{
		enum json_type	t	=json_object_get_type(pVal);

		assert(t == json_type_int);

		//accessor index
		pVF->mpElAccess[elIdx]	=json_object_get_int(pVal);

		if(0 == strncmp("POSITION", pKey, 8))
		{
			pVF->mpElements[elIdx]	=EL_POSITION;
		}
		else if(0 == strncmp("NORMAL", pKey, 6))
		{
			pVF->mpElements[elIdx]	=EL_NORMAL;
		}
		else if(0 == strncmp("TANGENT", pKey, 7))
		{
			pVF->mpElements[elIdx]	=EL_TANGENT;
		}
		else if(0 == strncmp("TEXCOORD", pKey, 8))
		{
			pVF->mpElements[elIdx]	=EL_TEXCOORD;
		}
		else if(0 == strncmp("COLOR", pKey, 5))
		{
			pVF->mpElements[elIdx]	=EL_COLOR;
		}
		else if(0 == strncmp("JOINTS", pKey, 6))
		{
			pVF->mpElements[elIdx]	=EL_BINDICES;
		}
		else if(0 == strncmp("WEIGHTS", pKey, 7))
		{
			pVF->mpElements[elIdx]	=EL_BWEIGHTS;
		}

		elIdx++;
	}
}

static int	sSizeForCompType(int compType)
{
	int	compSize	=-1;

	switch(compType)
	{
		case	CTYPE_BYTE:
		case	CTYPE_UBYTE:
			compSize	=1;
			break;
		case	CTYPE_SHORT:
		case	CTYPE_USHORT:
			compSize	=2;
			break;
		case	CTYPE_UINT:
		case	CTYPE_FLOAT:
			compSize	=4;
			break;
		default:
			assert(false);
	}
	return	compSize;
}

static int	sSizeForVType(int vType)
{
	int	typeSize	=-1;

	switch(vType)
	{
		case	TYPE_MAT2:
			typeSize	=4;
			break;
		case	TYPE_MAT3:
			typeSize	=9;
			break;
		case	TYPE_MAT4:
			typeSize	=16;
			break;
		case	TYPE_SCALAR:
			typeSize	=1;
			break;
		case	TYPE_VEC2:
			typeSize	=2;
			break;
		case	TYPE_VEC3:
			typeSize	=3;
			break;
		case	TYPE_VEC4:
			typeSize	=4;
			break;
		default:
			assert(false);
	}
	return	typeSize;
}

static int	sSizeForAccType(const Accessor *pAcc)
{
	int	compSize	=sSizeForCompType(pAcc->mComponentType);

	int	typeSize	=sSizeForVType(pAcc->mType);

	return	compSize * typeSize;
}

//return numVerts, vertSize, and pointer to Data
static void	*sMakeMeshData(GraphicsDevice *pGD,
	const VertFormat *pVF, const BufferView *pBVs,
	const Accessor *pAccs, const uint8_t *pBin,
	bool bFlip, int *pNumVerts, int *pVertSize)
{
	//calc total size of the buffer needed
	int	vSize	=0;
	for(int i=0;i < pVF->mNumElements;i++)
	{
		int	acc	=pVF->mpElAccess[i];

		vSize	+=sSizeForAccType(&pAccs[acc]);
	}

	//grab element zero's count
	*pNumVerts	=pAccs[pVF->mpElAccess[0]].mCount;

	//ensure all elements have the same count
	for(int i=0;i < pVF->mNumElements;i++)
	{
		int	acc	=pVF->mpElAccess[i];

		assert(pAccs[acc].mCount == *pNumVerts);
	}

	int	totalSize	=*pNumVerts * vSize;

	//do some basic checks on sizes
	for(int i=0;i < pVF->mNumElements;i++)
	{
		int	acc	=pVF->mpElAccess[i];
		int	bv	=pAccs[acc].mBufferView;

		assert(pBVs[bv].mBufIdx == 0);

		int	elSize	=sSizeForAccType(&pAccs[acc]);

		assert(pBVs[bv].mByteLength == (elSize * *pNumVerts));
	}

	int	grogSizes[pVF->mNumElements];

	Layouts_GetGrogSizes(pVF->mpElements, grogSizes, pVF->mNumElements);

	*pVertSize	=0;

	//alloc temp space for squishing elements if needed
	void *pSquishSpace[pVF->mNumElements];
	for(int i=0;i < pVF->mNumElements;i++)
	{
		pSquishSpace[i]	=malloc(grogSizes[i] * *pNumVerts);
		*pVertSize	+=grogSizes[i];
	}

	for(int i=0;i < pVF->mNumElements;i++)
	{
		int	acc		=pVF->mpElAccess[i];
		int	bv		=pAccs[acc].mBufferView;
		int	glSize	=sSizeForAccType(&pAccs[acc]);
		int	gSize	=grogSizes[i];
		int	ofs		=pBVs[bv].mByteOffset;
		int	elTotal	=pBVs[bv].mByteLength;
		int	cSize	=sSizeForCompType(pAccs[acc].mComponentType);

		if(glSize == gSize)
		{
			memcpy(pSquishSpace[i], &pBin[ofs], elTotal);
			continue;
		}

		//I think in the future this code will have problems as it
		//attempts to work on more complex static meshes and such.
		//Hopefully the assert catches it, and can add new cases.
		for(int j=0;j < *pNumVerts;j++)
		{
			switch(pAccs[acc].mType)
			{
				case	TYPE_VEC2:
					if(bFlip)
					{
						Misc_ConvertFlippedUVVec2ToF16(
							(const float *)&pBin[ofs + (j * glSize)],
							pSquishSpace[i] + (j * gSize));
					}
					else
					{
						Misc_ConvertVec2ToF16(
							(const float *)&pBin[ofs + (j * glSize)],
							pSquishSpace[i] + (j * gSize));
					}
				break;
				case	TYPE_VEC3:
					Misc_ConvertVec3ToF16((const float *)&pBin[ofs + (j * glSize)],
						pSquishSpace[i] + (j * gSize));
					break;
				case	TYPE_VEC4:
					Misc_ConvertVec4ToF16((const float *)&pBin[ofs + (j * glSize)],
						pSquishSpace[i] + (j * gSize));
					break;
				default:
					assert(false);
			}
		}
	}

	//flip if needed
	if(bFlip)
	{
		for(int i=0;i < pVF->mNumElements;i++)
		{
			int	gSize	=grogSizes[i];
			if(pVF->mpElements[i] == EL_POSITION)
			{
				for(int j=0;j < *pNumVerts;j++)
				{
					float	z	=*(float *)(pSquishSpace[i] + (j * gSize) + 8);
					*(float *)(pSquishSpace[i] + (j * gSize) + 8)	=-z;
				}
			}
			else if(pVF->mpElements[i] == EL_POSITION4)
			{
				for(int j=0;j < *pNumVerts;j++)
				{
					float	z	=*(float *)(pSquishSpace[i] + (j * gSize) + 8);
					*(float *)(pSquishSpace[i] + (j * gSize) + 8)	=-z;
				}
			}
		}
	}

	//ram for the assembled verts
	void	*pVerts	=malloc(*pVertSize * *pNumVerts);

	//assemble one at a time... PainPeko
	for(int j=0;j < *pNumVerts;j++)
	{
		int	ofs		=0;
		for(int i=0;i < pVF->mNumElements;i++)
		{
			int	gSize	=grogSizes[i];

			//copy squished elements
			memcpy((pVerts + (j * *pVertSize) + ofs),
				pSquishSpace[i] + (j * gSize), gSize);

			ofs	+=gSize;
		}
	}

	//free squishface buffers
	for(int i=0;i < pVF->mNumElements;i++)
	{
		free(pSquishSpace[i]);
	}

	return	pVerts;
}

static Mesh	*sMakeMeshIndex(GraphicsDevice *pGD,
	const StuffKeeper *pSK,
	const struct json_object *pMeshes,
	const uint8_t *pBin, const Accessor *pAccs,
	const BufferView *pBVs, bool bFlipZ, int index)
{
	int	numMeshes	=json_object_array_length(pMeshes);

	assert(index < numMeshes);
	
	int			indAccessIndex	=-1;
	UT_string	*pName			=NULL;
	int			matIndex		=-1;		

	//the returned matched vert format
	VertFormat	*pVF	=malloc(sizeof(VertFormat));
	
	printf("Mesh %d\n", index);
	
	const struct json_object	*pArr	=json_object_array_get_idx(pMeshes, index);
	
	json_object_object_foreach(pArr, pKey, pVal)
	{
		enum json_type	t	=json_object_get_type(pVal);
		printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
			json_object_get_string(pVal));
		
		if(0 == strncmp("primitives", pKey, 10))
		{
			assert(t == json_type_array);

			const struct json_object	*pAtArr	=json_object_array_get_idx(pVal, 0);

			//in my test data this has vert attributes,
			//and an indices int, and a materal int
			json_object_object_foreach(pAtArr, pPrimKey, pPrimVal)
			{
				enum json_type	tprim	=json_object_get_type(pPrimVal);
				printf("AttrKeyValue: %s : %s,%s\n", pPrimKey,
					json_type_to_name(tprim),
					json_object_get_string(pPrimVal));

				if(0 == strncmp("attributes", pPrimKey, 10))
				{
					assert(tprim == json_type_object);

					sFillVertFormat(pPrimVal, pVF);
				}
				else if(0 == strncmp("indices", pPrimKey, 7))
				{
					assert(tprim == json_type_int);

					indAccessIndex	=json_object_get_int(pPrimVal);
				}
				else if(0 == strncmp("material", pPrimKey, 8))
				{
					assert(tprim == json_type_int);

					matIndex	=json_object_get_int(pPrimVal);
				}
			}
		}
		else if(0 == strncmp("name", pKey, 4))
		{
			assert(t == json_type_string);
			
			utstring_new(pName);
			
			utstring_printf(pName, "%s", json_object_get_string(pVal));
		}
	}

	//create mesh
	int	numVerts, vSize;
	void	*pVData	=sMakeMeshData(pGD, pVF, pBVs,
		pAccs, pBin, bFlipZ, &numVerts, &vSize);

	//index data
	Accessor	*pIndAcc	=&pAccs[indAccessIndex];
	BufferView	*pBVI		=&pBVs[pIndAcc->mBufferView];

	//calc total size of the buffer needed
	int	size	=sSizeForAccType(pIndAcc);

	int	totalSize	=pIndAcc->mCount * size;

	assert(totalSize == pBVI->mByteLength);

	Mesh	*pRet	=Mesh_Create(pGD, pSK, pName,
		pVData, &pBin[pBVI->mByteOffset],
		pVF->mpElements, pVF->mNumElements,
		numVerts, pIndAcc->mCount, vSize);
	
	utstring_free(pName);
	
	return	pRet;
}


Character	*GLCV_ExtractChar(GraphicsDevice *pGD,
	const StuffKeeper *pSK, const GLTFFile *pGF, Mesh **ppMesh)
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
		pMeshArr[i]	=sMakeMeshIndex(pGD, pSK, pMeshes,
						pGF->mpBinChunk, pAcs, pBVs, false, i);
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
		pMeshArr[i]	=sMakeMeshIndex(pGD, pSK, pMeshes,
						pGF->mpBinChunk, pAcs, pBVs, true, i);
	}

	mat4	xForms[numMeshes];

	sGetPartTransforms(pNodes, xForms, true, numMeshes);

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
	int	numNodes	=sGetNodeCount(pScenes);
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

	Skeleton	*pSkel	=sMakeSkeleton(pNodes, pSkins, pGF->mpBinChunk, pAccs, pBVs);

	const struct json_object	*pAnims	=json_object_object_get(pGF->mpJSON, "animations");
	if(pAnims == NULL)
	{
		printf("gltf has no animations.\n");
		return;
	}

	if(*ppALib == NULL)
	{
		*ppALib	=AnimLib_Create(pSkel);
	}

	int	numAnims	=json_object_array_length(pAnims);
	for(int i=0;i < numAnims;i++)
	{
		const struct json_object	*pArr	=json_object_array_get_idx(pAnims, i);

		Anim	*pAnim	=AnimStuff_GrabAnim(pArr, pAccs, pBVs, pGF->mpBinChunk, pSkel);

		AnimLib_Add(*ppALib, pAnim);
	}
}