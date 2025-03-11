#pragma once
#include	<stdint.h>

typedef struct	Anim_t			Anim;
typedef struct	Skeleton_t		Skeleton;
typedef struct	Accessor_t		Accessor;
typedef struct	BufferView_t	BufferView;


Anim	*AnimStuff_GrabAnim(const struct json_object *pAnim,
	const Accessor *pAccs, const BufferView *pBVs,
	const uint8_t *pBuf, const Skeleton *pSkel);
