#pragma once
#include	<cglm/call.h>

typedef struct	SkellyEditor_t	SkellyEditor;
typedef struct	StuffKeeper_t	StuffKeeper;
typedef struct	CBKeeper_t		CBKeeper;
typedef struct	Input_t			Input;
typedef struct	AnimLib_t		AnimLib;
typedef struct	Character_t		Character;

SkellyEditor	*SKE_Create(StuffKeeper *pSK, GraphicsDevice *pGD,
			CBKeeper *pCBK, Input *pInp, UIStuff *pUI);
void	SKE_Destroy(SkellyEditor **ppSKE);
void	SKE_Update(SkellyEditor *pSKE, float deltaTime);

//some stuff loaded in main file
void	SKE_SetAnimLib(SkellyEditor *pSKE, AnimLib *pALib);
void	SKE_SetCharacter(SkellyEditor *pSKE, Character *pALib);

void	SKE_RenderShapes(SkellyEditor *pSKE, const vec3 lightDir);
void	SKE_MakeClayLayout(SkellyEditor *pSKE);