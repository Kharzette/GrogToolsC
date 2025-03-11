#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<stdint.h>
#include	<stdbool.h>
#include	"uthash.h"
#include	<json-c/json_tokener.h>
#include	<json-c/json_object.h>
#include	<json-c/json_object_iterator.h>
#include	<json-c/linkhash.h>
#include	"json-c/arraylist.h"
#include	"json-c/json_util.h"
#include	"MeshLib/Skin.h"
#include	"MeshLib/Skeleton.h"
#include	"MeshLib/AnimLib.h"
#include	"MeshLib/Anim.h"
#include	"MeshLib/SubAnim.h"


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

#define	TARG_TRANSLATION	0
#define	TARG_ROTATION		1
#define	TARG_SCALE			2

#define	INTERP_LINEAR		0
#define	INTERP_STEP			1
#define	INTERP_CUBICSPLINE	2


typedef struct	NodeChannel_t
{
	int	id;

	int	sampTrans;
	int	sampRot;
	int	sampScale;

	UT_hash_handle	hh;
}	NodeChannel;

typedef struct	Sampler_t
{
	int	accInput;
	int	accOutput;
	int	interp;
}	Sampler;

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


//static forward decs
static Sampler		*sMakeAnimSamplers(const struct json_object *pAnim);
static NodeChannel	*sMakeChannels(const struct json_object *pAnim, int *numAffectedJoints);
static SubAnim		**sMakeSplitSubAnims(const NodeChannel *pNCList,
	const Sampler *pSamps, const Accessor *pAccs,
	const BufferView *pBVs, const uint8_t *pBuf,
	KeyFrame **ppTargets, int numNC);


Anim	*AnimStuff_GrabAnim(const struct json_object *pAnim,
	const Accessor *pAccs, const BufferView *pBVs,
	const uint8_t *pBuf, const Skeleton *pSkel)
{
	int			numNCs		=0;
	UT_string	*pName		=NULL;
	NodeChannel	*pNCs		=NULL;
	Sampler		*pSamplers	=NULL;

	json_object_object_foreach(pAnim, pKey, pVal)
	{
		enum json_type	t	=json_object_get_type(pVal);
		printf("AnimKeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
			json_object_get_string(pVal));

		if(t == json_type_string)
		{
			utstring_new(pName);
			utstring_printf(pName, "%s", json_object_get_string(pVal));
		}
		else if(t == json_type_array)
		{
			if(0 == strncmp(pKey, "samplers", 8))
			{
				pSamplers	=sMakeAnimSamplers(pVal);
			}
			else
			{
				pNCs	=sMakeChannels(pVal, &numNCs);
			}
		}
	}

	KeyFrame	*pTargets;
	SubAnim	**ppChanSubs	=sMakeSplitSubAnims(pNCs,
		pSamplers, pAccs, pBVs, pBuf, &pTargets, numNCs);

	SubAnim	**ppMerged	=malloc(sizeof(SubAnim *) * numNCs);

	for(int i=0;i < numNCs;i++)
	{
		int	ofs	=i * 3;

		ppMerged[i]	=SubAnim_Merge(ppChanSubs[ofs],
			ppChanSubs[ofs + 1], ppChanSubs[ofs + 2]);
		
		SubAnim_SetBone(ppMerged[i], Skeleton_GetBoneKeyByIndex(pSkel, i), i);
	}

	return	Anim_Create(pName, ppMerged, numNCs);
}


static Sampler	*sMakeAnimSamplers(const struct json_object *pSamplers)
{
	int	numSamp	=json_object_array_length(pSamplers);

	Sampler	*pRet	=malloc(sizeof(Sampler) * numSamp);

	for(int i=0;i < numSamp;i++)
	{
		vec3	scale		={1,1,1};
		vec4	rot			={0,0,0,1};
		vec3	trans		={0,0,0};
		int		samp		=-1;
		int		targNode	=-1;
		int		targPath	=-1;

		printf("Samp %d\n", i);

		const struct json_object	*pArr	=json_object_array_get_idx(pSamplers, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{
			enum json_type	t	=json_object_get_type(pVal);
			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
				json_object_get_string(pVal));

			if(0 == strncmp("input", pKey, 5))
			{
				pRet[i].accInput	=json_object_get_int(pVal);
			}
			else if(0 == strncmp("output", pKey, 6))
			{
				pRet[i].accOutput	=json_object_get_int(pVal);
			}
			else
			{
				const char	*pTerp	=json_object_get_string(pVal);
				if(0 == strncmp("LINEAR", pTerp, 6))
				{
					pRet[i].interp	=INTERP_LINEAR;
				}
				else if(0 == strncmp("STEP", pTerp, 4))
				{
					pRet[i].interp	=INTERP_STEP;
				}
				else if(0 == strncmp("CUBICSPLINE", pTerp, 11))
				{
					pRet[i].interp	=INTERP_CUBICSPLINE;
				}
				else
				{
					assert(false);
				}
			}
		}
	}
	return	pRet;
}

static NodeChannel	*sMakeChannels(const struct json_object *pChannels,
									int *numAffectedJoints)
{
	int	numChannels	=json_object_array_length(pChannels);

	int	meshIdx	=-1;
	int	skinIdx	=-1;

	//count up the bones affected
	NodeChannel	*pNCs		=NULL;
	int			numAffected	=0;
	for(int i=0;i < numChannels;i++)
	{
		int	targNode	=-1;

		const struct json_object	*pArr	=json_object_array_get_idx(pChannels, i);
		json_object_object_foreach(pArr, pKey, pVal)
		{
			enum json_type	t	=json_object_get_type(pVal);
			if(t != json_type_int)
			{
				json_object_object_foreach(pVal, pKey2, pVal2)
				{
					if(0 == strncmp("node", pKey2, 4))
					{
						targNode	=json_object_get_int(pVal2);

						NodeChannel	*pNC	=NULL;
						HASH_FIND_INT(pNCs, &targNode, pNC);
						if(pNC == NULL)
						{
							numAffected++;
							pNC		=malloc(sizeof(NodeChannel));
							pNC->id	=targNode;
							HASH_ADD_INT(pNCs, id, pNC);
						}
					}
				}		
			}
		}
	}

	//grab channel data
	for(int i=0;i < numChannels;i++)
	{
		vec3	scale		={1,1,1};
		vec4	rot			={0,0,0,1};
		vec3	trans		={0,0,0};
		int		samp		=-1;
		int		targNode	=-1;
		int		targPath	=-1;

		printf("Channel %d\n", i);

		const struct json_object	*pArr	=json_object_array_get_idx(pChannels, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{
			enum json_type	t	=json_object_get_type(pVal);
			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
				json_object_get_string(pVal));

			if(t == json_type_int)
			{
				samp	=json_object_get_int(pVal);
			}
			else
			{
				json_object_object_foreach(pVal, pKey2, pVal2)
				{
					enum json_type	t2	=json_object_get_type(pVal);
					printf("KeyValue2: %s : %s,%s\n", pKey2, json_type_to_name(t2),
						json_object_get_string(pVal2));
					
					if(0 == strncmp("node", pKey2, 4))
					{
						targNode	=json_object_get_int(pVal2);
					}
					else
					{
						const char	*pPath	=json_object_get_string(pVal2);

						NodeChannel	*pNC	=NULL;
						HASH_FIND_INT(pNCs, &targNode, pNC);
						assert(pNC != NULL);

						if(0 == strncmp(pPath, "translation", 11))
						{
							targPath		=TARG_TRANSLATION;
							pNC->sampTrans	=samp;
						}
						else if(0 == strncmp(pPath, "rotation", 8))
						{
							targPath		=TARG_ROTATION;
							pNC->sampRot	=samp;
						}
						else if(0 == strncmp(pPath, "scale", 5))
						{
							targPath		=TARG_SCALE;
							pNC->sampScale	=samp;
						}
						else
						{
							assert(false);
						}
					}
				}		
			}
		}
	}

	*numAffectedJoints	=numAffected;

	return	pNCs;
}

static SubAnim	**sMakeSplitSubAnims(const NodeChannel *pNCList,
	const Sampler *pSamps, const Accessor *pAccs,
	const BufferView *pBVs, const uint8_t *pBuf,
	KeyFrame **ppTargets,	int numNC)
{
	SubAnim	**ppSubs	=malloc(sizeof(SubAnim *) * numNC * 3);

	//these frames will be targeted by the fake split
	//animations to compute extra keys
	*ppTargets	=malloc(sizeof(KeyFrame) * numNC * 3);

	KeyFrame	*pTrg	=*ppTargets;
	for(int i=0;i < (numNC * 3);i++)
	{
		KeyFrame_Identity(&pTrg[i]);
	}

	for(const NodeChannel *pNC=pNCList;pNC != NULL;pNC=pNC->hh.next)
	{
		Accessor	*pInpRot	=&pAccs[pSamps[pNC->sampRot].accInput];
		Accessor	*pOutRot	=&pAccs[pSamps[pNC->sampRot].accOutput];

		Accessor	*pInpTrans	=&pAccs[pSamps[pNC->sampTrans].accInput];
		Accessor	*pOutTrans	=&pAccs[pSamps[pNC->sampTrans].accOutput];

		Accessor	*pInpScale	=&pAccs[pSamps[pNC->sampScale].accInput];
		Accessor	*pOutScale	=&pAccs[pSamps[pNC->sampScale].accOutput];

		//should be the same number of times as keys
		assert(pInpRot->mCount == pOutRot->mCount);
		assert(pInpTrans->mCount == pOutTrans->mCount);
		assert(pInpScale->mCount == pOutScale->mCount);

		//only doing floats for now
		assert(pInpRot->mComponentType == CTYPE_FLOAT);
		assert(pInpTrans->mComponentType == CTYPE_FLOAT);
		assert(pInpScale->mComponentType == CTYPE_FLOAT);
		assert(pOutRot->mComponentType == CTYPE_FLOAT);
		assert(pOutTrans->mComponentType == CTYPE_FLOAT);
		assert(pOutScale->mComponentType == CTYPE_FLOAT);

		assert(pOutRot->mType == TYPE_VEC4);
		assert(pOutTrans->mType == TYPE_VEC3);
		assert(pOutScale->mType == TYPE_VEC3);

		BufferView	*pInpRotBV		=&pBVs[pInpRot->mBufferView];
		BufferView	*pInpTransBV	=&pBVs[pInpTrans->mBufferView];
		BufferView	*pInpScaleBV	=&pBVs[pInpScale->mBufferView];

		float	*pRTimes	=(float *)&pBuf[pInpRotBV->mByteOffset];
		float	*pTTimes	=(float *)&pBuf[pInpTransBV->mByteOffset];
		float	*pSTimes	=(float *)&pBuf[pInpScaleBV->mByteOffset];

		BufferView	*pOutRotBV		=&pBVs[pOutRot->mBufferView];
		BufferView	*pOutTransBV	=&pBVs[pOutTrans->mBufferView];
		BufferView	*pOutScaleBV	=&pBVs[pOutScale->mBufferView];

		KeyFrame	*pRKeys	=malloc(sizeof(KeyFrame) * pOutRot->mCount);
		KeyFrame	*pTKeys	=malloc(sizeof(KeyFrame) * pOutTrans->mCount);
		KeyFrame	*pSKeys	=malloc(sizeof(KeyFrame) * pOutScale->mCount);

		//grab rot keys
		for(int j=0;j < pOutRot->mCount;j++)
		{
			KeyFrame_Identity(&pRKeys[j]);

			memcpy(&pRKeys[j].mRotation,
				&pBuf[pOutRotBV->mByteOffset
					+ (j * sizeof(vec4))], sizeof(vec4));
		}

		//grab translate keys
		for(int j=0;j < pOutTrans->mCount;j++)
		{
			KeyFrame_Identity(&pTKeys[j]);

			memcpy(&pTKeys[j].mPosition,
				&pBuf[pOutTransBV->mByteOffset
					+ (j * sizeof(vec3))], sizeof(vec3));
		}

		//grab scale keys
		for(int j=0;j < pOutScale->mCount;j++)
		{
			KeyFrame_Identity(&pSKeys[j]);

			memcpy(&pSKeys[j].mScale,
				&pBuf[pOutScaleBV->mByteOffset
					+ (j * sizeof(vec3))], sizeof(vec3));
		}

		int	subIdx	=pNC->id * 3;

		//ORDER MATTERS HERE!

		//translation
		ppSubs[subIdx]	=SubAnim_Create(pTTimes, pTKeys, pOutTrans->mCount,
							&pTrg[subIdx], pNC->id);
		subIdx++;

		//scale
		ppSubs[subIdx]	=SubAnim_Create(pSTimes, pSKeys, pOutScale->mCount,
							&pTrg[subIdx], pNC->id);
		subIdx++;

		//rotation
		ppSubs[subIdx]	=SubAnim_Create(pRTimes, pRKeys, pOutRot->mCount,
							&pTrg[subIdx], pNC->id);
	}
	return	ppSubs;
}