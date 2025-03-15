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
#include	"MeshLib/GSNode.h"
#include	"MeshLib/SubAnim.h"
#include	"glTFTypes.h"
#include	"glTFFile.h"


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
//		printf("AnimKeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
//			json_object_get_string(pVal));

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

	//free fake bone targets
	free(pTargets);

	//free junk channel anims
	for(int i=0;i < (numNCs * 3);i++)
	{
		free(ppChanSubs[i]);
	}

	//free samplers
	free(pSamplers);

	//free nodes
	{
		NodeChannel	*pTmp, *pCur;

		HASH_ITER(hh, pNCs, pCur, pTmp)
		{
			HASH_DEL(pNCs, pCur);
			free(pCur);
		}
	}
	
	return	Anim_Create(pName, ppMerged, numNCs);
}

Skeleton	*AnimStuff_GrabSkeleton(const struct json_object *pNodes,
									const struct json_object *pSkins,
									const uint8_t *pBin,
									const Accessor *pAccs,
									const BufferView *pBVs)
{
	int	numNodes	=json_object_array_length(pNodes);
	int	numSkins	=json_object_array_length(pSkins);

	//exporting metarig too?
	assert(numSkins == 1);
	
	int		*pJoints		=NULL;
	int		numJoints		=0;
	
	const struct json_object	*pArr	=json_object_array_get_idx(pSkins, 0);
	
	json_object_object_foreach(pArr, pKey, pVal)
	{
		enum json_type	t	=json_object_get_type(pVal);
//		printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
//			json_object_get_string(pVal));
		
		if(0 == strncmp("inverseBindMatrices", pKey, 19))
		{
			assert(t == json_type_int);
		}
		else if(0 == strncmp("joints", pKey, 19))
		{
			assert(t == json_type_array);
			
			numJoints	=json_object_array_length(pVal);			
			
			pJoints	=malloc(sizeof(int) * numJoints);
			
			for(int j=0;j < numJoints;j++)
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

	GSNode	**ppGNs	=malloc(sizeof(GSNode *) * numNodes);

	for(int i=0;i < numNodes;i++)
	{
		vec3		scale	={1,1,1};
		vec4		rot		={0,0,0,1};
		vec3		trans	={0,0,0};
		UT_string	*pName	=NULL;

		int		numKids		=0;

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

				utstring_new(pName);

				utstring_printf(pName, "%s", json_object_get_string(pVal));
			}
			else if(0 == strncmp("rotation", pKey, 8))
			{
				assert(t == json_type_array);

				GLTF_GetVec4(pVal, rot); 
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
			}
			else if(0 == strncmp("children", pKey, 8))
			{
				assert(t == json_type_array);

				numKids	=json_object_array_length(pVal);
			}
			else if(0 == strncmp("mesh", pKey, 4))
			{
				assert(t == json_type_int);
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

		ppGNs[i]	=malloc(sizeof(GSNode));

		ppGNs[i]->szName		=pName;
		ppGNs[i]->mNumChildren	=numKids;
		ppGNs[i]->mIndex		=i;

		if(numKids > 0)
		{
			ppGNs[i]->mpChildren	=malloc(sizeof(GSNode *) * numKids);
		}
		else
		{
			ppGNs[i]->mpChildren	=NULL;
		}

		glm_vec3_copy(trans, ppGNs[i]->mKeyValue.mPosition);
		glm_vec3_copy(scale, ppGNs[i]->mKeyValue.mScale);
		glm_vec4_copy(rot, ppGNs[i]->mKeyValue.mRotation);
	}

	//fix up the children
	for(int i=0;i < numNodes;i++)
	{
		if(ppGNs[i]->mNumChildren <= 0)
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

				for(int j=0;j < ppGNs[i]->mNumChildren;j++)
				{
					const struct json_object	*pIdx	=
						json_object_array_get_idx(pVal, j);

					ppGNs[i]->mpChildren[j]	=ppGNs[json_object_get_int(pIdx)];
				}
			}
		}
	}

	//set name with index so it shows up in the
	//skeleton editor for easier debuggery
//	for(int i=0;i < numNodes;i++)
//	{
//		utstring_printf(pGNs[i].szName, "%d,%d", i, pJoints[i]);
//	}

	//indexes are not the same from file to file
	//need to build a consistent name to index
	//and keep it somewhere and fix indexes
	//when new stuff is loaded

	//convert root node to left handed
//	GSNode_ConvertToLeftHanded(&pGNs[pJoints[0]]);

	//seems like joint 0 is the root
	Skeleton	*pRet	=Skeleton_Create(ppGNs[pJoints[0]]);

	free(pJoints);
	free(ppGNs);

	return	pRet;
}
	

static Sampler	*sMakeAnimSamplers(const struct json_object *pSamplers)
{
	int	numSamp	=json_object_array_length(pSamplers);

	Sampler	*pRet	=malloc(sizeof(Sampler) * numSamp);

	for(int i=0;i < numSamp;i++)
	{
//		printf("Samp %d\n", i);

		const struct json_object	*pArr	=json_object_array_get_idx(pSamplers, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{
//			enum json_type	t	=json_object_get_type(pVal);
//			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
//				json_object_get_string(pVal));

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

	//count up the bones affected
	NodeChannel	*pNCs		=NULL;
	int			numAffected	=0;
	for(int i=0;i < numChannels;i++)
	{
		int	targNode	=-1;

		const struct json_object	*pArr	=json_object_array_get_idx(pChannels, i);
		json_object_object_foreach(pArr, pKey, pVal)
		{
			pKey	=pKey;	//shutup warning

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
		int		samp		=-1;
		int		targNode	=-1;

//		printf("Channel %d\n", i);

		const struct json_object	*pArr	=json_object_array_get_idx(pChannels, i);

		json_object_object_foreach(pArr, pKey, pVal)
		{
			pKey	=pKey;

			enum json_type	t	=json_object_get_type(pVal);
//			printf("KeyValue: %s : %s,%s\n", pKey, json_type_to_name(t),
//				json_object_get_string(pVal));

			if(t == json_type_int)
			{
				samp	=json_object_get_int(pVal);
			}
			else
			{
				json_object_object_foreach(pVal, pKey2, pVal2)
				{
//					enum json_type	t2	=json_object_get_type(pVal);
//					printf("KeyValue2: %s : %s,%s\n", pKey2, json_type_to_name(t2),
//						json_object_get_string(pVal2));
					
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
							pNC->sampTrans	=samp;
						}
						else if(0 == strncmp(pPath, "rotation", 8))
						{
							pNC->sampRot	=samp;
						}
						else if(0 == strncmp(pPath, "scale", 5))
						{
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
	KeyFrame **ppTargets, int numNC)
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
		const Accessor	*pInpRot	=&pAccs[pSamps[pNC->sampRot].accInput];
		const Accessor	*pOutRot	=&pAccs[pSamps[pNC->sampRot].accOutput];

		const Accessor	*pInpTrans	=&pAccs[pSamps[pNC->sampTrans].accInput];
		const Accessor	*pOutTrans	=&pAccs[pSamps[pNC->sampTrans].accOutput];

		const Accessor	*pInpScale	=&pAccs[pSamps[pNC->sampScale].accInput];
		const Accessor	*pOutScale	=&pAccs[pSamps[pNC->sampScale].accOutput];

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

		const BufferView	*pInpRotBV		=&pBVs[pInpRot->mBufferView];
		const BufferView	*pInpTransBV	=&pBVs[pInpTrans->mBufferView];
		const BufferView	*pInpScaleBV	=&pBVs[pInpScale->mBufferView];

		const float	*pRTimes	=(float *)&pBuf[pInpRotBV->mByteOffset];
		const float	*pTTimes	=(float *)&pBuf[pInpTransBV->mByteOffset];
		const float	*pSTimes	=(float *)&pBuf[pInpScaleBV->mByteOffset];

		const BufferView	*pOutRotBV		=&pBVs[pOutRot->mBufferView];
		const BufferView	*pOutTransBV	=&pBVs[pOutTrans->mBufferView];
		const BufferView	*pOutScaleBV	=&pBVs[pOutScale->mBufferView];

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
			
			//coordinate system convert
//			pRKeys[j].mRotation[1]	=-pRKeys[j].mRotation[1];
//			pRKeys[j].mRotation[2]	=-pRKeys[j].mRotation[2];
		}

		//grab translate keys
		for(int j=0;j < pOutTrans->mCount;j++)
		{
			KeyFrame_Identity(&pTKeys[j]);

			memcpy(&pTKeys[j].mPosition,
				&pBuf[pOutTransBV->mByteOffset
					+ (j * sizeof(vec3))], sizeof(vec3));
			
			//coordinate system convert
//			pTKeys[j].mPosition[0]	=-pTKeys[j].mPosition[0];
		}

		//grab scale keys
		for(int j=0;j < pOutScale->mCount;j++)
		{
			KeyFrame_Identity(&pSKeys[j]);

			memcpy(&pSKeys[j].mScale,
				&pBuf[pOutScaleBV->mByteOffset
					+ (j * sizeof(vec3))], sizeof(vec3));

			//coordinate system convert
//			pTKeys[j].mScale[0]	=-pTKeys[j].mScale[0];
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