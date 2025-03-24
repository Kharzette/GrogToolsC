#pragma once
#include	<cglm/call.h>

typedef struct	RayCaster_t			RayCaster;
typedef struct	GraphicsDevice_t	GraphicsDevice;
typedef struct	StuffKeeper_t		StuffKeeper;
typedef struct	CBKeeper_t			CBKeeper;
typedef struct	Input_t				Input;
typedef struct	AnimLib_t			AnimLib;
typedef struct	Character_t			Character;
typedef struct	Static_t			Static;

RayCaster	*RC_Create(StuffKeeper *pSK, GraphicsDevice *pGD, CBKeeper *pCBK, Input *pInp);
void		RC_Destroy(RayCaster **ppRC);
void		RC_Update(RayCaster *pRC, float deltaTime);
void		RC_Render(RayCaster *pRC);

//some stuff loaded in main file
void	RC_SetAnimLib(RayCaster *pRC, AnimLib *pALib);
void	RC_SetCharacter(RayCaster *pRC, Character *pChar);
void	RC_SetStatic(RayCaster *pRC, Static *pStat);

void	RC_RenderShapes(RayCaster *pRC, const vec3 lightDir);
void	RC_MakeClayLayout(RayCaster *pRC);