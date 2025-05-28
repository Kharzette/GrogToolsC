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


typedef struct	StaticVert_t
{
	vec4		PositionU;
	uint32_t	NormVCol[4];
}	StaticVert;

typedef struct	CharacterVert_t
{
	vec4		PositionU;
	uint32_t	NormVCol[4];
	uint32_t	Bone[4];
}	CharacterVert;

//static forward decs
static void	sFillVertFormat(const struct json_object *pVAttr, VertFormat *pVF);
static int	sSizeForAccType(const Accessor *pAcc);
static mat4	*sGetIBP(const uint8_t *pBin, const Accessor *pAcc, const BufferView *pBVs);
static StaticVert	*sMakeStaticMeshData(GraphicsDevice *pGD,
	const VertFormat *pVF, const BufferView *pBVs,
	const Accessor *pAccs, const uint8_t *pBin,
	int *pNumVerts, int *pVertSize);
static CharacterVert	*sMakeCharacterMeshData(GraphicsDevice *pGD,
	const VertFormat *pVF, const BufferView *pBVs,
	const Accessor *pAccs, const uint8_t *pBin,
	int *pNumVerts, int *pVertSize);


//for nodes that are just tied to static mesh parts
void	MeshStuff_GetStaticPartTransforms(const struct json_object *pNodes,
	mat4 xForms[], int numParts)
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

//		printf("Node %d\n", i);

		const struct json_object	*pArr	=json_object_array_get_idx(pNodes, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{
			enum json_type	t	=json_object_get_type(pVal);
//			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
//				json_object_get_string(pVal));
			
			if(0 == strncmp("name", pKey, 4))
			{
				assert(t == json_type_string);
			}
			else if(0 == strncmp("rotation", pKey, 8))
			{
				assert(t == json_type_array);

				GLTF_GetVec4(pVal, rot);

				//coordinate system fix
				rot[0]	=-rot[0];
				rot[1]	=-rot[1];
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

				//coordinate system fix
				trans[2]	=-trans[2];
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

		KeyFrame_GetMatrixOtherWay(&kf, xForms[meshIdx]);
	}
}

Skin	*MeshStuff_GrabSkins(const struct json_object *pSkins,
	const uint8_t *pBin, const Accessor *pAccs,
	const BufferView *pBVs)
{
	int	numSkins	=json_object_array_length(pSkins);
	
	assert(numSkins == 1);

	uint8_t	*pJoints	=NULL;
	int		numJoints	=0;
	Skin	*pRet		=NULL;
	
	for(int i=0;i < numSkins;i++)
	{
		int			ibmAccessIndex	=-1;
		UT_string	*pName			=NULL;
		mat4		*pIBPs			=NULL;
		
//		printf("Skin %d\n", i);
		
		const struct json_object	*pArr	=json_object_array_get_idx(pSkins, i);
		
		json_object_object_foreach(pArr, pKey, pVal)
		{
			enum json_type	t	=json_object_get_type(pVal);
//			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
//				json_object_get_string(pVal));
			
			if(0 == strncmp("inverseBindMatrices", pKey, 19))
			{
				assert(t == json_type_int);
				
				ibmAccessIndex	=json_object_get_int(pVal);
				
				pIBPs	=sGetIBP(pBin, &pAccs[ibmAccessIndex], pBVs);
			}
			else if(0 == strncmp("joints", pKey, 19))
			{
				assert(t == json_type_array);

				numJoints	=json_object_array_length(pVal);			
			
				pJoints	=malloc(sizeof(uint8_t) * numJoints);
				
				for(int j=0;j < numJoints;j++)
				{
					const struct json_object	*pJIdx	=
						json_object_array_get_idx(pVal, j);

					int	jidx	=json_object_get_int(pJIdx);

					assert(jidx <= UINT8_MAX);
						
					pJoints[j]	=jidx;
				}
			}
			else if(0 == strncmp("name", pKey, 4))
			{
				assert(t == json_type_string);
				
				utstring_new(pName);
				
				utstring_printf(pName, "%s", json_object_get_string(pVal));
			}
		}
		pRet	=Skin_Create(pIBPs, pJoints, pAccs[ibmAccessIndex].mCount);
	}

	free(pJoints);
	
	return	pRet;
}

Mesh	*MeshStuff_MakeMeshIndex(GraphicsDevice *pGD,
	const StuffKeeper *pSK,
	const struct json_object *pMeshes,
	const uint8_t *pBin, const Accessor *pAccs,
	const BufferView *pBVs, bool bStatic, int index)
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
	int		numVerts, vSize;
	void	*pVData;
	if(bStatic)
	{
		pVData	=sMakeStaticMeshData(pGD, pVF, pBVs,
			pAccs, pBin, &numVerts, &vSize);
	}
	else
	{
		pVData	=sMakeCharacterMeshData(pGD, pVF, pBVs,
			pAccs, pBin, &numVerts, &vSize);
	}

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

	//vert format
	free(pVF->mpElAccess);
	free(pVF->mpElements);
	free(pVF);
	
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

//return numVerts, vertSize, and pointer to Data
static StaticVert	*sMakeStaticMeshData(GraphicsDevice *pGD,
	const VertFormat *pVF, const BufferView *pBVs,
	const Accessor *pAccs, const uint8_t *pBin,
	int *pNumVerts, int *pVertSize)
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

	*pVertSize	=sizeof(StaticVert);

	StaticVert	*pRet	=malloc(sizeof(StaticVert) * *pNumVerts);

	for(int i=0;i < *pNumVerts;i++)
	{
		vec4		pos;
		vec4		col		={	1,1,1,1	};
		vec4		norm;
		vec2		tex;
		uint16_t	idx		=69;

		for(int j=0;j < pVF->mNumElements;j++)
		{
			int	acc		=pVF->mpElAccess[j];
			int	bv		=pAccs[acc].mBufferView;
			int	glSize	=sSizeForAccType(&pAccs[acc]);
			int	ofs		=pBVs[bv].mByteOffset + i * glSize;

			switch(pVF->mpElements[j])
			{
				case	EL_POSITION:
					glm_vec3_copy((const float *)&pBin[ofs], pos);
					break;
				case	EL_POSITION2:
					glm_vec2_copy((const float *)&pBin[ofs], pos);
					break;
				case	EL_POSITION4:
					glm_vec4_copy((const float *)&pBin[ofs], pos);
					break;
				case	EL_BINDICES:
					assert(false);
					break;
				case	EL_BWEIGHTS:
					assert(false);
					break;
				case	EL_COLOR:
					if(pAccs[acc].mComponentType == CTYPE_BYTE)
					{
						vec4	col4, colLin;
						
						Misc_RGBAToVec4(pBin[ofs], col4);
						Misc_SRGBToLinear(col4, colLin);
						glm_vec4_copy(colLin, col);
					}
					else if(pAccs[acc].mComponentType == CTYPE_USHORT)
					{
						vec4	col4, colLin;

						Misc_RGBA16ToVec4(*((uint64_t *)&pBin[ofs]), col4);
						Misc_SRGBToLinear(col4, colLin);
						glm_vec4_copy(colLin, col);
					}
					else
					{
						assert(false);	//dunno!
					}
					break;
				case	EL_NORMAL:
					glm_vec3_copy((const float *)&pBin[ofs], norm);
					break;
				case	EL_TANGENT:
					assert(false);
					break;
				case	EL_TEXCOORD:
					glm_vec2_copy((const float *)&pBin[ofs], tex);
					break;
				case	EL_TEXCOORD4:
					assert(false);
					break;
				case	EL_DATA:
					//super annoying that blender makes these floats
					idx	=*(((float *)pBin) + pBVs[bv].mByteOffset + i);
//					printf(" %d,", idx);
					break;
				default:
					assert(false);					
			}
		}

		//right to left
		pos[2]	=-pos[2];
		norm[2]	=-norm[2];

		//position
		glm_vec3_copy(pos, pRet[i].PositionU);

		//UV
		pRet[i].PositionU[3]	=tex[0];
		norm[3]					=tex[1];

		//normal and color and index
		Misc_InterleaveVec4IdxToF16(norm, col, idx, pRet[i].NormVCol);
	}

	return	pRet;
}

//return numVerts, vertSize, and pointer to Data
static CharacterVert	*sMakeCharacterMeshData(GraphicsDevice *pGD,
	const VertFormat *pVF, const BufferView *pBVs,
	const Accessor *pAccs, const uint8_t *pBin,
	int *pNumVerts, int *pVertSize)
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

	*pVertSize	=sizeof(CharacterVert);

	CharacterVert	*pRet	=malloc(sizeof(CharacterVert) * *pNumVerts);

	for(int i=0;i < *pNumVerts;i++)
	{
		vec4		pos;
		vec4		col		={	1,1,1,1	};
		vec4		norm;
		vec2		tex;
		uint16_t	idx		=69;
		uint8_t		boneIdxs[4]	={	0,0,0,0	};
		vec4		weights		={	1,0,0,0	};

		for(int j=0;j < pVF->mNumElements;j++)
		{
			int	acc		=pVF->mpElAccess[j];
			int	bv		=pAccs[acc].mBufferView;
			int	glSize	=sSizeForAccType(&pAccs[acc]);
			int	ofs		=pBVs[bv].mByteOffset + i * glSize;

			switch(pVF->mpElements[j])
			{
				case	EL_POSITION:
					glm_vec3_copy((const float *)&pBin[ofs], pos);
					break;
				case	EL_POSITION2:
					glm_vec2_copy((const float *)&pBin[ofs], pos);
					break;
				case	EL_POSITION4:
					glm_vec4_copy((const float *)&pBin[ofs], pos);
					break;
				case	EL_BINDICES:
					memcpy(boneIdxs, &pBin[ofs], 4);
					break;
				case	EL_BWEIGHTS:
					memcpy(weights, &pBin[ofs], 16);
					break;
				case	EL_COLOR:
					if(pAccs[acc].mComponentType == CTYPE_BYTE)
					{
						vec4	col4, colLin;
						
						Misc_RGBAToVec4(pBin[ofs], col4);
						Misc_SRGBToLinear(col4, colLin);
						glm_vec4_copy(colLin, col);
					}
					else if(pAccs[acc].mComponentType == CTYPE_USHORT)
					{
						vec4	col4, colLin;

						Misc_RGBA16ToVec4(*((uint64_t *)&pBin[ofs]), col4);
						Misc_SRGBToLinear(col4, colLin);
						glm_vec4_copy(colLin, col);
					}
					else
					{
						assert(false);	//dunno!
					}
					break;
				case	EL_NORMAL:
					glm_vec3_copy((const float *)&pBin[ofs], norm);
					break;
				case	EL_TANGENT:
					assert(false);
					break;
				case	EL_TEXCOORD:
					glm_vec2_copy((const float *)&pBin[ofs], tex);
					break;
				case	EL_TEXCOORD4:
					assert(false);
					break;
				default:
					assert(false);
			}
		}

		//position
		glm_vec3_copy(pos, pRet[i].PositionU);

		//UV
		pRet[i].PositionU[3]	=tex[0];
		norm[3]					=tex[1];

		//normal and color and index
		Misc_InterleaveVec4IdxToF16(norm, col, idx, pRet[i].NormVCol);

		//bone data
		Misc_InterleaveBone(weights, boneIdxs, idx, pRet[i].Bone);
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
		else if(pKey[0] == '_')
		{
			//blender will export attributes that start with _
			pVF->mpElements[elIdx]	=EL_DATA;
		}
		else
		{
			//dunno what this is
			assert(false);
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