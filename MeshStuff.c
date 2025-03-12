//static forward decs
#include	<stdio.h>
#include	<assert.h>
#include	<stdint.h>
#include	<stdbool.h>
#include	"cglm/call.h"
#include	<utstring.h>
#include	<json-c/linkhash.h>
#include	"json-c/json_util.h"
#include	"UtilityLib/GraphicsDevice.h"
#include	"UtilityLib/MiscStuff.h"
#include	"MeshLib/Mesh.h"
#include	"MeshLib/KeyFrame.h"
#include	"MeshLib/Skin.h"
#include	"MeshLib/AnimLib.h"
#include	"MaterialLib/Layouts.h"
#include	"glTFTypes.h"
#include	"glTFFile.h"
#include	"MeshStuff.h"


//static forward decs
static mat4	*sGetIBP(const uint8_t *pBin, const Accessor *pAcc, const BufferView *pBVs);
static void	*sMakeMeshData(GraphicsDevice *pGD,
	const VertFormat *pVF, const BufferView *pBVs,
	const Accessor *pAccs, const uint8_t *pBin,
	const SkellyMap *pSkelMap,
	bool bFlip, int *pNumVerts, int *pVertSize);


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


//for nodes that are just tied to static mesh parts
void	MeshStuff_GetPartTransforms(const struct json_object *pNodes,
	mat4 xForms[], bool bFlipZTranslations, int numParts)
{
	int	numNodes	=json_object_array_length(pNodes);

	int	meshIdx	=-1;

	for(int i=0;i < numNodes;i++)
	{
		vec3		scale	={1,1,1};
		vec4		rot		={0,0,0,1};
		vec3		trans	={0,0,0};
		KeyFrame	kf;

		KeyFrame_Identity(&kf);

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

				GLTF_GetVec4(pVal, rot);

				if(bFlipZTranslations)
				{
					rot[0]	=-rot[0];
					rot[1]	=-rot[1];
				}
			}
			else if(0 == strncmp("scale", pKey, 8))
			{
				assert(t == json_type_array);

				GLTF_GetVec3(pVal, scale);
			}
			else if(0 == strncmp("translation", pKey, 8))
			{
				assert(t == json_type_array);

				GLTF_GetVec3(pVal, trans);

				if(bFlipZTranslations)
				{
					trans[2]	=-trans[2];
				}
			}
			else if(0 == strncmp("children", pKey, 8))
			{
				assert(t == json_type_array);
			}
			else if(0 == strncmp("mesh", pKey, 4))
			{
				assert(t == json_type_int);

				meshIdx	=json_object_get_int(pVal);
			}
			else if(0 == strncmp("skin", pKey, 4))
			{
				assert(t == json_type_int);
			}
			else
			{
				assert(false);
			}
		}

		glm_vec3_copy(trans, kf.mPosition);
		glm_vec3_copy(scale, kf.mScale);
		glm_vec4_copy(rot, kf.mRotation);

		if(bFlipZTranslations)
		{
			KeyFrame_GetMatrixOtherWay(&kf, xForms[meshIdx]);
		}
		else
		{
			KeyFrame_GetMatrix(&kf, xForms[meshIdx]);
		}
	}
}


Skin	*MeshStuff_GrabSkins(const struct json_object *pSkins,
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
		pCountKey	=pCountKey;	//shutup warning
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
	const SkellyMap *pSkelMap,
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
					Misc_ConvertFlippedUVVec2ToF16(
						(const float *)&pBin[ofs + (j * glSize)],
						pSquishSpace[i] + (j * gSize));
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

	//remap bone indexes if needed
	if(pSkelMap != NULL)
	{
		for(int i=0;i < pVF->mNumElements;i++)
		{
			int	gSize	=grogSizes[i];
			if(pVF->mpElements[i] == EL_BINDICES)
			{
				for(int j=0;j < *pNumVerts;j++)
				{
					uint8_t	bi0	=*(uint8_t *)(pSquishSpace[i] + (j * gSize) + 0);
					uint8_t	bi1	=*(uint8_t *)(pSquishSpace[i] + (j * gSize) + 1);
					uint8_t	bi2	=*(uint8_t *)(pSquishSpace[i] + (j * gSize) + 2);
					uint8_t	bi3	=*(uint8_t *)(pSquishSpace[i] + (j * gSize) + 3);

					//remap
					*(uint8_t *)(pSquishSpace[i] + (j * gSize) + 0)	=pSkelMap->mBoneMap[bi0];
					*(uint8_t *)(pSquishSpace[i] + (j * gSize) + 1)	=pSkelMap->mBoneMap[bi1];
					*(uint8_t *)(pSquishSpace[i] + (j * gSize) + 2)	=pSkelMap->mBoneMap[bi2];
					*(uint8_t *)(pSquishSpace[i] + (j * gSize) + 3)	=pSkelMap->mBoneMap[bi3];
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

Mesh	*MeshStuff_MakeMeshIndex(GraphicsDevice *pGD,
	const StuffKeeper *pSK,
	const struct json_object *pMeshes,
	const uint8_t *pBin, const Accessor *pAccs,
	const BufferView *pBVs, const SkellyMap *pSMap,
	bool bFlipZ, int index)
{
	int	numMeshes	=json_object_array_length(pMeshes);

	assert(index < numMeshes);
	
	int			indAccessIndex	=-1;
	UT_string	*pName			=NULL;

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
		pAccs, pBin, pSMap, bFlipZ, &numVerts, &vSize);

	//index data
	const Accessor		*pIndAcc	=&pAccs[indAccessIndex];
	const BufferView	*pBVI		=&pBVs[pIndAcc->mBufferView];

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