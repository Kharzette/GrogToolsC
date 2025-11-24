#include	<d3d11_1.h>
#include	"uthash.h"
#include	"UtilityLib/GraphicsDevice.h"
#include	"UtilityLib/Mover.h"
#include	"UtilityLib/MiscStuff.h"
#include	"UtilityLib/PrimFactory.h"
#include	"MaterialLib/StuffKeeper.h"
#include	"MaterialLib/CBKeeper.h"
#include	"UILib/UIStuff.h"
#include	"MeshLib/AnimLib.h"
#include	"MeshLib/Mesh.h"
#include	"MeshLib/Static.h"
#include	"MeshLib/Character.h"
#include	"MeshLib/Skeleton.h"
#include	"MeshLib/GSNode.h"
#include	"MeshLib/Skin.h"
#include	"MeshLib/CommonPrims.h"
#include	"InputLib/Input.h"

#define	POPOUT_INTERVAL	(0.25f)
#define	INFO_BUF_SIZE	1024
#define	RESX			1280
#define	RAY_LEN			5
#define	RAY_MOVE_AMOUNT	(0.005f)

//static data
__attribute_maybe_unused__
static char	sShapeNames[4][8]	={	{"Box"}, {"Sphere"}, {"Capsule"}, {"Invalid"}	};
static char	sInfoText[INFO_BUF_SIZE];


typedef struct	RayCaster_t
{
	//grogstuff
	GraphicsDevice	*mpGD;
	StuffKeeper		*mpSK;
	CBKeeper		*mpCBK;
	Input			*mpInp;
	UIStuff			*mpUI;		//clay ui

	//loaded data
	AnimLib		*mpALib;
	Character	*mpChar;
	Static		*mpStat;

	//prims
	LightRay	*mpRay;

	//misc stuffz
	bool	mbVisible;
	vec3	mRayDir;
	vec3	mRayTarget;

	//popout mover
	Mover	*mpRCM;

}	RayCaster;


//statics
static void	sMakeInfoString(const RayCaster *pRC);

//keybind statics
static void	RandomizeRayEH(void *pContext, const SDL_Event *pEvt);
static void RCPopOutEH(void *pContext, const SDL_Event *pEvt);
static void	RayLeftEH(void *pContext, const SDL_Event *pEvt);
static void	RayRightEH(void *pContext, const SDL_Event *pEvt);
static void	RayUpEH(void *pContext, const SDL_Event *pEvt);
static void	RayDownEH(void *pContext, const SDL_Event *pEvt);


RayCaster	*RC_Create(StuffKeeper *pSK, GraphicsDevice *pGD,
			CBKeeper *pCBK, Input *pInp)
{
	RayCaster	*pRet	=malloc(sizeof(RayCaster));
	memset(pRet, 0, sizeof(RayCaster));

	//default ray
	pRet->mRayDir[0]	=0.1f;
	pRet->mRayDir[1]	=-0.4f;
	pRet->mRayDir[2]	=0.3f;
	glm_normalize(pRet->mRayDir);

	//keep refs to groggy stuff
	pRet->mpSK	=pSK;
	pRet->mpGD	=pGD;
	pRet->mpCBK	=pCBK;

	//movers
	pRet->mpRCM				=Mover_Create();

	//prims
	pRet->mpRay		=CP_CreateLightRay(RAY_LEN, 0.01f, pGD, pSK);

	//input handlers
	//press events
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_F3, RCPopOutEH, pRet);		//pop outsidebar
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_R, RandomizeRayEH, pRet);

	//held
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_LEFT, RayLeftEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_RIGHT, RayRightEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_UP, RayUpEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_DOWN, RayDownEH, pRet);

	return	pRet;
}

void	RC_Update(RayCaster *pRC, float deltaTime)
{
	Mover_Update(pRC->mpRCM, deltaTime);
}

void	RC_Destroy(RayCaster **ppRC)
{
	RayCaster	*pRC	=*ppRC;

	CP_DestroyLightRay(&pRC->mpRay);
	Mover_Destroy(&pRC->mpRCM);

	free(pRC);

	*ppRC	=NULL;
}

void	RC_Render(RayCaster *pRC)
{
	vec4	mvPos;
	Mover_GetPos(pRC->mpRCM, mvPos);

	if(mvPos[0] <= 0)
	{
		return;
	}

	vec4	rayCol		={	1.0f, 0.0f, 1.0f, 1.0f	};
	vec3	localScale	={	1,	1,	1				};

	//set origin point for light ray to org - half ray
	vec3	rayLoc;

	glm_vec3_scale(pRC->mRayDir, (RAY_LEN / 2.0f), rayLoc);
	glm_vec3_sub(pRC->mRayTarget, rayLoc, rayLoc);

	CBK_SetLocalScale(pRC->mpCBK, localScale);

	CP_DrawLightRay(pRC->mpRay, pRC->mRayDir, rayCol, rayLoc, pRC->mpCBK, pRC->mpGD);
}


void	RC_SetAnimLib(RayCaster *pRC, AnimLib *pALib)
{
	pRC->mpALib	=pALib;
}

void	RC_SetCharacter(RayCaster *pRC, Character *pChar)
{
	pRC->mpChar	=pChar;
}

void	RC_SetStatic(RayCaster *pRC, Static *pStat)
{
	pRC->mpStat	=pStat;
}


void	RC_MakeClayLayout(RayCaster *pRC)
{
	vec4	mvPos;
	Mover_GetPos(pRC->mpRCM, mvPos);

	if(mvPos[0] > 0)
	{
		sMakeInfoString(pRC);
		Clay_String	rcInfo;

		rcInfo.chars	=sInfoText;
		rcInfo.length	=strlen(sInfoText);

		CLAY(CLAY_ID("RightAligner"), {
			.layout = {
				.childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_TOP},
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
				.sizing = { .width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_GROW(0) }				
			}
		})
		{
			CLAY(CLAY_ID("RightBar"), {
				.layout = {
					.childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
					.sizing = { .width = CLAY_SIZING_FIXED(mvPos[0]),
						.height = CLAY_SIZING_FIT(0) }				
				}, .backgroundColor = {150, 150, 155, 55}
			})
			{
				CLAY(CLAY_ID("RayCaster"), {
					.layout = {
						.childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
						.sizing = { .width = CLAY_SIZING_FIXED(300),
							.height = CLAY_SIZING_FIT(0) }
					}
				})
				{
					CLAY_TEXT(rcInfo, CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 70, 70, 155} }));
				}
			}
		}
	}
}


//statics
static void	sMakeInfoString(const RayCaster *pRC)
{
	if(pRC->mpStat == NULL && pRC->mpChar == NULL && pRC->mpALib == NULL)
	{
		sprintf(sInfoText, "No collidable data loaded...\n");
	}
	else if(pRC->mpStat != NULL)
	{
		int	numParts	=Static_GetNumParts(pRC->mpStat);
		sprintf(sInfoText, "Static mesh with %d parts loaded.  Use arrow keys to adjust ray, or r to randomize direction.", numParts);
	}
	else if(pRC->mpChar != NULL && pRC->mpALib != NULL)
	{
		sprintf(sInfoText, "Character and AnimLib loaded.  Use arrow keys to adjust ray, or r to randomize direction.");
	}
}


//event handlers (eh)
static void	RandomizeRayEH(void *pContext, const SDL_Event *pEvt)
{
	RayCaster	*pRC	=(RayCaster *)pContext;

	assert(pRC);

	Misc_RandomDirection(pRC->mRayDir);
}

static void RCPopOutEH(void *pContext, const SDL_Event *pEvt)
{
	RayCaster	*pRC	=(RayCaster *)pContext;
	assert(pRC);

	vec4	startPos	={0};
	vec4	endPos		={0};

	endPos[0]	=300;

	vec4	mvPos;
	Mover_GetPos(pRC->mpRCM, mvPos);

	if(mvPos[0] > 0)
	{
		if(pRC->mbVisible)
		{
			//closing from partway open
			Mover_SetUpMove(pRC->mpRCM, mvPos, startPos,
				POPOUT_INTERVAL / 2.0f, 0.2f, 0.2f);
		}
		else
		{
			//opening from partway closed
			Mover_SetUpMove(pRC->mpRCM, mvPos, endPos,
				POPOUT_INTERVAL / 2.0f, 0.2f, 0.2f);
		}
	}
	else
	{
		if(!pRC->mbVisible)
		{
			Mover_SetUpMove(pRC->mpRCM, startPos, endPos,
				POPOUT_INTERVAL, 0.2f, 0.2f);
		}
	}

	pRC->mbVisible	=!pRC->mbVisible;
}

static void	RayLeftEH(void *pContext, const SDL_Event *pEvt)
{
	RayCaster	*pRC	=(RayCaster *)pContext;
	assert(pRC);

	vec4	mvPos;
	Mover_GetPos(pRC->mpRCM, mvPos);

	if(mvPos[0] > 0)
	{
		vec3	xVec, yVec, zVec;

		Misc_BuildBasisVecsFromDirection(pRC->mRayDir, xVec, yVec, zVec);

		glm_vec3_scale(xVec, -RAY_MOVE_AMOUNT, xVec);

		glm_vec3_add(pRC->mRayTarget, xVec, pRC->mRayTarget);
	}
}

static void	RayRightEH(void *pContext, const SDL_Event *pEvt)
{
	RayCaster	*pRC	=(RayCaster *)pContext;
	assert(pRC);

	vec4	mvPos;
	Mover_GetPos(pRC->mpRCM, mvPos);

	if(mvPos[0] > 0)
	{
		vec3	xVec, yVec, zVec;

		Misc_BuildBasisVecsFromDirection(pRC->mRayDir, xVec, yVec, zVec);

		glm_vec3_scale(xVec, RAY_MOVE_AMOUNT, xVec);

		glm_vec3_add(pRC->mRayTarget, xVec, pRC->mRayTarget);
	}
}

static void	RayUpEH(void *pContext, const SDL_Event *pEvt)
{
	RayCaster	*pRC	=(RayCaster *)pContext;
	assert(pRC);

	vec4	mvPos;
	Mover_GetPos(pRC->mpRCM, mvPos);

	if(mvPos[0] > 0)
	{
		vec3	xVec, yVec, zVec;

		Misc_BuildBasisVecsFromDirection(pRC->mRayDir, xVec, yVec, zVec);

		glm_vec3_scale(yVec, RAY_MOVE_AMOUNT, yVec);

		glm_vec3_add(pRC->mRayTarget, yVec, pRC->mRayTarget);
	}
}

static void	RayDownEH(void *pContext, const SDL_Event *pEvt)
{
	RayCaster	*pRC	=(RayCaster *)pContext;
	assert(pRC);

	vec4	mvPos;
	Mover_GetPos(pRC->mpRCM, mvPos);

	if(mvPos[0] > 0)
	{
		vec3	xVec, yVec, zVec;

		Misc_BuildBasisVecsFromDirection(pRC->mRayDir, xVec, yVec, zVec);

		glm_vec3_scale(yVec, -RAY_MOVE_AMOUNT, yVec);

		glm_vec3_add(pRC->mRayTarget, yVec, pRC->mRayTarget);
	}
}