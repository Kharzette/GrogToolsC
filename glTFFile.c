#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<stdint.h>
#include	<stdbool.h>
#include	"UtilityLib/StringStuff.h"
#include	<json-c/json_tokener.h>
#include	<json-c/json_object.h>
#include	<json-c/json_object_iterator.h>
#include	<json-c/linkhash.h>
#include	"json-c/arraylist.h"


typedef struct	GLTFFile_t
{
	struct json_object	*mpJSON;
	uint8_t				*mpBinChunk;
}	GLTFFile;


static void sReadJSON(FILE *pFile, GLTFFile *pGLTF, uint32_t len)
{
	char	*pBuf	=malloc(len + 1);

	int	numRead	=fread(pBuf, 1, len, pFile);

	pBuf[len]	=0;

	assert(numRead == len);

	pGLTF->mpJSON	=json_tokener_parse(pBuf);

	free(pBuf);

	json_object_object_foreach(pGLTF->mpJSON, pKey, pVal)
	{
		printf("KeyValue: %s : %s\n", pKey, json_object_get_string(pVal));
	}
}

static void	sReadChunk(FILE *pFile, GLTFFile *pGLTF)
{
	uint32_t	len;
	fread(&len, 4, 1, pFile);

	uint32_t	type;
	fread(&type, 4, 1, pFile);

	if(type == 0x4E4F534A)
	{
		//JSON
		sReadJSON(pFile, pGLTF, len);
	}
	else if(type == 0x004E4942)
	{
		//binary
		pGLTF->mpBinChunk	=malloc(len);
		fread(pGLTF->mpBinChunk, 1, len, pFile);
	}
}


GLTFFile	*GLTF_CreateFromGLB(const char *szFileName)
{
	assert(szFileName != NULL);

	FILE	*pFile	=fopen(szFileName, "rb");
	if(pFile == NULL)
	{
		printf("Couldn't open file %s\n", szFileName);
		return	NULL;
	}

	//check for magic
	uint32_t	magic;
	fread(&magic, 1, 4, pFile);

	if(magic != 0x46546C67)
	{
		printf("File %s not a glTF file.\n", szFileName);
		fclose(pFile);
		return	NULL;
	}

	uint32_t	version;
	fread(&version, 1, 4, pFile);
	if(version != 2)
	{
		printf("Warning version != 2\n");
	}

	uint32_t	length;
	fread(&length, 1, 4, pFile);

	GLTFFile	*pRet	=malloc(sizeof(GLTFFile));
	memset(pRet, 0, sizeof(GLTFFile));

	while(ftell(pFile) < length)
	{
		sReadChunk(pFile, pRet);
	}

	fclose(pFile);

	return	pRet;
}

GLTFFile	*GLTF_Create(const char *szFileName)
{
	assert(szFileName != NULL);

	FILE	*pFile	=fopen(szFileName, "rb");
	if(pFile == NULL)
	{
		printf("Couldn't open file %s\n", szFileName);
		return	NULL;
	}

	fseek(pFile, 0L, SEEK_END);

	long	len	=ftell(pFile);

	fseek(pFile, 0L, SEEK_SET);

	GLTFFile	*pRet	=malloc(sizeof(GLTFFile));
	memset(pRet, 0, sizeof(GLTFFile));

	sReadJSON(pFile, pRet, len);

	fclose(pFile);

	struct json_object	*pBuffs	=json_object_object_get(pRet->mpJSON, "buffers");
	if(pBuffs == NULL)
	{
		printf("gltf has no bin file.\n");
		json_object_put(pRet->mpJSON);
		free(pRet);
		return	NULL;
	}

	char		*pBinFileName;
	uint64_t	binLength;

	struct json_object	*pArr	=json_object_array_get_idx(pBuffs, 0);

	json_object_object_foreach(pArr, pKey, pVal)
	{
		enum json_type	t	=json_object_get_type(pVal);
		if(t == json_type_int)
		{
			binLength	=json_object_get_uint64(pVal);
		}
		else
		{
			pBinFileName	=json_object_get_string(pVal);
		}
	}

	UT_string	*pPath	=SZ_StripFileName(szFileName);
	if(pPath == NULL)
	{
		utstring_printf(pPath, "%s", pBinFileName);
	}
	else
	{
		//concats
		utstring_printf(pPath, "/%s", pBinFileName);
	}

	pRet->mpBinChunk	=malloc(binLength);

	FILE	*pBinFile	=fopen(utstring_body(pPath), "rb");
	if(pBinFile == NULL)
	{
		printf("Failed to open binfile %s\n", pBinFileName);
		json_object_put(pBuffs);
		json_object_put(pRet->mpJSON);
		free(pRet);
		return	NULL;
	}

	fread(pRet->mpBinChunk, 1, binLength, pBinFile);

	fclose(pBinFile);

	return	pRet;
}