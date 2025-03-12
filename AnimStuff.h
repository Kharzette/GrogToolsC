#pragma once
#include	<stdint.h>
#include	<json-c/json_types.h>

typedef struct	Anim_t			Anim;
typedef struct	Skeleton_t		Skeleton;
typedef struct	Accessor_t		Accessor;
typedef struct	BufferView_t	BufferView;


Anim	*AnimStuff_GrabAnim(const struct json_object *pAnim,
	const Accessor *pAccs, const BufferView *pBVs,
	const uint8_t *pBuf, const Skeleton *pSkel);

Skeleton	*AnimStuff_GrabSkeleton(const struct json_object *pNodes,
	const struct json_object *pSkins,
	const uint8_t *pBin,
	const Accessor *pAccs,
	const BufferView *pBVs);