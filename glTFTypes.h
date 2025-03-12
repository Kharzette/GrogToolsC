#pragma once
#include	<stdint.h>
#include	<cglm/call.h>
#include	"uthash.h"


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

typedef struct	VertFormat_t
{
	//vert format data
	int	*mpElements;
	int	*mpElAccess;
	int	mNumElements;

}	VertFormat;