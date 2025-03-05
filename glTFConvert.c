#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<stdint.h>
#include	<stdbool.h>
#include	"UtilityLib/StringStuff.h"
#include	"MeshLib/Character.h"
#include	"MeshLib/GSNode.h"
#include	<json-c/json_tokener.h>
#include	<json-c/json_object.h>
#include	<json-c/json_object_iterator.h>
#include	<json-c/linkhash.h>
#include	"json-c/arraylist.h"
#include	"json-c/json_util.h"


typedef struct	GLTFFile_t
{
	struct json_object	*mpJSON;
	uint8_t				*mpBinChunk;
}	GLTFFile;


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


Character	*GLCV_ExtractCharacter(const GLTFFile *pGF)
{
	assert(pGF != NULL);

	//characters have:
	//base transform
	//skin
	//bound
	//mesh parts

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

	GSNode	*pGSNs	=sMakeNodes(pNodes);
}