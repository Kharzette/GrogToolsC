#include	<d3d11_1.h>
#include	"uthash.h"
#include	"UtilityLib/GraphicsDevice.h"
#include	"UtilityLib/Mover.h"
#include	"UtilityLib/PrimFactory.h"
#include	"MaterialLib/StuffKeeper.h"
#include	"MaterialLib/CBKeeper.h"
#include	"MaterialLib/UIStuff.h"
#include	"MeshLib/AnimLib.h"
#include	"MeshLib/Mesh.h"
#include	"MeshLib/Character.h"
#include	"MeshLib/Skeleton.h"
#include	"MeshLib/GSNode.h"
#include	"MeshLib/Skin.h"
#include	"InputLib/Input.h"


#define	NOT_COLLAPSING		0
#define	GROWING				1
#define	COLLAPSING			2
#define	INFO_BUF_SIZE		1024
#define	BONE_VERTICAL_SIZE	30
#define	COLLAPSE_INTERVAL	(0.25f)
#define	RADIUS_INCREMENT	(0.001f)
#define	LENGTH_INCREMENT	(0.001f)

//clay colours
#define COLOR_ORANGE		(Clay_Color) {225, 138, 50, 255}
#define COLOR_BLUE			(Clay_Color) {111, 173, 162, 255}
#define COLOR_GOLD			(Clay_Color) {255, 222, 162, 255}

//static data
static char	sShapeNames[4][8]	={	{"Box"}, {"Sphere"}, {"Capsule"}, {"Invalid"}	};
static char	sInfoText[INFO_BUF_SIZE];
static mat4	s90Rot;

//little hashy struct for tracking bone display data
typedef struct	BoneDisplayData_t
{
	const GSNode	*mpNode;

	bool	mbSelected;
	bool	mbInfluencing;	//some part of the mesh uses this bone
	bool	mbCollapsed;	//draw this node collapsed
	bool	mbAnimating;	//mid opening or closing

	UT_hash_handle	hh;
}	BoneDisplayData;

typedef struct	SkellyEditor_t
{
	//D3D stuff
	ID3D11InputLayout		*mpPrimLayout;
	ID3D11VertexShader		*mpWNormWPos;
	ID3D11PixelShader		*mpTriSolidSpec;

	//grogstuff
	GraphicsDevice	*mpGD;
	StuffKeeper		*mpSK;
	CBKeeper		*mpCBK;
	Input			*mpInp;
	UIStuff			*mpUI;		//clay ui

	//loaded data
	AnimLib		*mpALib;
	Character	*mpChar;

	//prims
	PrimObject	*mpSphere;	//for bone colliders
	PrimObject	*mpCube;	//for bone colliders
	PrimObject	*mpCapsule;	//for bone colliders

	//clay pointer stuff
	Clay_Vector2	mScrollDelta;

	//skelly editor data
	BoneDisplayData	*mpBDD;
	bool			mbSEVisible;		//user wants to see skelly editor
	bool			mbSEAnimating;		//stuff collapsing / growing
	bool			mbAdjustingMode;	//single bone shape adjust
	int				mAdjustingIndex;	//index of bone being changed

	//skelly popout movers
	Mover	*mpSEM;
	Mover	*mpBoneCollapse;
}	SkellyEditor;


//statics
static void sSetNodesAnimatingOff(BoneDisplayData *pBDD);
static void sSkeletonLayout(const GSNode *pNode, SkellyEditor *pSKE, int colState);
static bool sIsAnyNodeSelected(BoneDisplayData *pBDD);
static void	sMakeInfoString(const SkellyEditor *pSKE);
static void sRenderNodeCollisionShape(const SkellyEditor *pSKE,
				int nodeIndex, const vec3 lightDir, const vec4 colour);

//input event handlers
static void CollapseBonesEH(void *pContext, const SDL_Event *pEvt);
static void MarkUnusedBonesEH(void *pContext, const SDL_Event *pEvt);
static void DeleteBonesEH(void *pContext, const SDL_Event *pEvt);
static void	SkelPopOutEH(void *pContext, const SDL_Event *pEvt);
static void	AdjustBoneBoundEH(void *pContext, const SDL_Event *pEvt);
static void	BoxWidthIncreaseEH(void *pContext, const SDL_Event *pEvt);
static void	BoxWidthDecreaseEH(void *pContext, const SDL_Event *pEvt);
static void	LengthIncreaseEH(void *pContext, const SDL_Event *pEvt);
static void	LengthDecreaseEH(void *pContext, const SDL_Event *pEvt);
static void	BoxDepthIncreaseEH(void *pContext, const SDL_Event *pEvt);
static void	BoxDepthDecreaseEH(void *pContext, const SDL_Event *pEvt);
static void	RadiusIncreaseEH(void *pContext, const SDL_Event *pEvt);
static void	RadiusDecreaseEH(void *pContext, const SDL_Event *pEvt);
static void	SnapToJointEH(void *pContext, const SDL_Event *pEvt);
static void	MirrorEH(void *pContext, const SDL_Event *pEvt);
static void	MoveForwardEH(void *pContext, const SDL_Event *pEvt);
static void	MoveBackwardEH(void *pContext, const SDL_Event *pEvt);
static void	ChangeShapeEH(void *pContext, const SDL_Event *pEvt);


SkellyEditor	*SKE_Create(StuffKeeper *pSK, GraphicsDevice *pGD,
			CBKeeper *pCBK, Input *pInp)
{
	SkellyEditor	*pRet	=malloc(sizeof(SkellyEditor));
	memset(pRet, 0, sizeof(SkellyEditor));

	glm_rotate_make(s90Rot, -GLM_PI_2, (vec3){1, 0, 0});

	//keep refs to groggy stuff
	pRet->mpSK	=pSK;
	pRet->mpGD	=pGD;
	pRet->mpCBK	=pCBK;

	//movers
	pRet->mpSEM				=Mover_Create();
	pRet->mpBoneCollapse	=Mover_Create();

	//for prim draws
	pRet->mpPrimLayout		=StuffKeeper_GetInputLayout(pSK, "VPosNorm");
	pRet->mpWNormWPos		=StuffKeeper_GetVertexShader(pSK, "WNormWPosVS");
	pRet->mpTriSolidSpec	=StuffKeeper_GetPixelShader(pSK, "TriSolidSpecPS");

	//prims
	pRet->mpCube	=PF_CreateCube(1.0f, true, pGD);
	pRet->mpSphere	=PF_CreateSphere((vec3){0,0,0}, 1.0f, pGD);
	pRet->mpCapsule	=PF_CreateCapsule(1.0f, 4.0f, pGD);

	//input handlers
	//press events
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_c, CollapseBonesEH, pRet);	//collapse/uncollapse selected bones
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_n, MarkUnusedBonesEH, pRet);	//mark bones that aren't used in vert weights
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_DELETE, DeleteBonesEH, pRet);	//nuke selected bones
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_p, SkelPopOutEH, pRet);		//pop outsidebar
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_BACKSLASH, AdjustBoneBoundEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_j, SnapToJointEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_m, MirrorEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_COMMA, ChangeShapeEH, pRet);

	//hold events
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_y, BoxWidthIncreaseEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_u, BoxWidthDecreaseEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_i, LengthIncreaseEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_o, LengthDecreaseEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_LEFTBRACKET, BoxDepthIncreaseEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_RIGHTBRACKET, BoxDepthDecreaseEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_g, RadiusIncreaseEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_h, RadiusDecreaseEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_v, MoveForwardEH, pRet);
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_HELD, SDLK_b, MoveBackwardEH, pRet);

	return	pRet;
}

void	SKE_Destroy(SkellyEditor **ppSKE)
{
	SkellyEditor	*pSKE	=*ppSKE;

	//nuke bone display data
	BoneDisplayData	*pCur, *pTmp;
	HASH_ITER(hh, pSKE->mpBDD, pCur, pTmp)
	{
		HASH_DEL(pSKE->mpBDD, pCur);
		free(pCur);
	}

	free(pSKE);

	*ppSKE	=NULL;
}

void	SKE_Update(SkellyEditor *pSKE, float deltaTime)
{
	Mover_Update(pSKE->mpSEM, deltaTime);
	Mover_Update(pSKE->mpBoneCollapse, deltaTime);

	if(Mover_IsDone(pSKE->mpBoneCollapse))
	{
		pSKE->mbSEAnimating	=false;
		sSetNodesAnimatingOff(pSKE->mpBDD);
	}
}

void	SKE_SetAnimLib(SkellyEditor *pSKE, AnimLib *pALib)
{
	pSKE->mpALib	=pALib;	
}

void	SKE_SetCharacter(SkellyEditor *pSKE, Character *pChar)
{
	pSKE->mpChar	=pChar;
}


void	SKE_RenderShapes(SkellyEditor *pSKE, const vec3 lightDir)
{
	BoneDisplayData	*pCur;
//	GSNode			*pNode;

	int	numSelected	=0;
	for(pCur=pSKE->mpBDD;pCur != NULL;pCur=pCur->hh.next)
	{
		if(pCur->mbSelected)
		{
			numSelected++;

			if(pSKE->mbAdjustingMode && pCur->mpNode->mIndex == pSKE->mAdjustingIndex)
			{
				continue;
			}

			sRenderNodeCollisionShape(pSKE, pCur->mpNode->mIndex,
				lightDir, (vec4){1,1,1,1});
		}
	}

	if(numSelected <= 0)
	{
		return;
	}
	if(pSKE->mpChar == NULL)
	{
		return;
	}

	//do adjusting bone if needed
	if(pSKE->mbAdjustingMode)
	{
		sRenderNodeCollisionShape(pSKE, pSKE->mAdjustingIndex,
			lightDir, (vec4){1,0.6f,0,1});
	}
}

void	SKE_MakeClayLayout(SkellyEditor *pSKE)
{
	vec4	mvPos, bcPos;
	Mover_GetPos(pSKE->mpSEM, mvPos);
	Mover_GetPos(pSKE->mpBoneCollapse, bcPos);

	if(mvPos[0] > 0.0f)
	{
		Clay__OpenElement();
		CLAY_ID("SideBar");

		Clay_ElementDeclaration	cedSB	={	.layout = {
			.childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.sizing = { .width = CLAY_SIZING_FIXED(mvPos[0]),
			.height = CLAY_SIZING_FIT(0) },
//			.padding = {16, 16, 16, 16 },
//			.childGap = 16
			}, .backgroundColor = {150, 150, 155, 55}
		};

		Clay__ConfigureOpenElement(cedSB);

		Clay__OpenElement();
		CLAY_ID("SkellyEditor");

		Clay_ElementDeclaration	cedSE	={	.layout = {
			.childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.sizing = { .width = CLAY_SIZING_FIXED(mvPos[0]),
			.height = CLAY_SIZING_FIT(0) }
			}, .scroll = { .horizontal = true, .vertical = true }
		};
		Clay__ConfigureOpenElement(cedSE);

		if(pSKE->mpALib != NULL)
		{
			const Skeleton	*pSkel	=AnimLib_GetSkeleton(pSKE->mpALib);
			if(pSkel != NULL)
			{
				for(int i=0;i < pSkel->mNumRoots;i++)
				{
					sSkeletonLayout(pSkel->mpRoots[i], pSKE, NOT_COLLAPSING);
				}
			}
			else
			{
				CLAY_TEXT(CLAY_STRING("No Skeleton Loaded!"), CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 70, 70, 155} }));
			}
		}
		else
		{
			CLAY_TEXT(CLAY_STRING("No Skeleton Loaded!"), CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 70, 70, 155} }));
		}
		Clay__CloseElement();	//skelly editor

		if(sIsAnyNodeSelected(pSKE->mpBDD))
		{
			//create a bottom section for info
			sMakeInfoString(pSKE);
			Clay_String	csInfo;

			csInfo.chars	=sInfoText;
			csInfo.length	=strlen(sInfoText);

			CLAY({ .id = CLAY_ID("SkellyInfoBox"),
				.layout = {
					.childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
					.sizing = { .width = CLAY_SIZING_FIXED(mvPos[0]),
						.height = CLAY_SIZING_FIT(0)
					},
					.padding = {16, 16, 16, 16 },
					.childGap = 16
				},
				.border = { .color = {80, 80, 80, 255}, .width = 2 },
				.backgroundColor =  {150, 150, 155, 55}})
			{
				CLAY_TEXT(csInfo, CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 70, 70, 155} }));
			}
		}
		Clay__CloseElement();	//sidebar
	}

}


static void sOnHoverBone(Clay_ElementId eID, Clay_PointerData pnt, intptr_t userData)
{
	//clicked?
	if(pnt.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME)
	{
		printf("Click! %s\n", eID.stringId.chars);

		SkellyEditor	*pSKE	=(SkellyEditor *)userData;
		if(pSKE == NULL)
		{
			return;
		}
		if(pSKE->mpALib == NULL)
		{
			return;
		}
		if(pSKE->mbAdjustingMode)
		{
			return;
		}

		const Skeleton	*pSkel	=AnimLib_GetSkeleton(pSKE->mpALib);
		if(pSkel == NULL)
		{
			return;
		}

		const GSNode	*pBone	=Skeleton_GetConstBoneByName(pSkel, eID.stringId.chars);
		if(pBone == NULL)
		{
			return;
		}

		BoneDisplayData	*pBDD;

		//see if bone data already exists
		HASH_FIND_PTR(pSKE->mpBDD, &pBone, pBDD);
		if(pBDD == NULL)
		{
			pBDD	=malloc(sizeof(BoneDisplayData));
			memset(pBDD, 0, sizeof(BoneDisplayData));

			pBDD->mpNode	=pBone;

			HASH_ADD_PTR(pSKE->mpBDD, mpNode, pBDD);
		}

		//toggle selected
		pBDD->mbSelected	=!pBDD->mbSelected;
	}
}

static void sSetNodesAnimatingOff(BoneDisplayData *pBDD)
{
	BoneDisplayData	*pCur;

	//toggle collapse on selected and deselect
	for(pCur=pBDD;pCur != NULL;pCur=pCur->hh.next)
	{
		pCur->mbAnimating	=false;
	}
}

static bool sIsAnyNodeSelected(BoneDisplayData *pBDD)
{
	BoneDisplayData	*pCur;

	for(pCur=pBDD;pCur != NULL;pCur=pCur->hh.next)
	{
		if(pCur->mbSelected)
		{
			return	true;
		}
	}
	return	false;
}

//return -1 if more or less than 1 selected
static int	sGetSingleNodeSelected(BoneDisplayData *pBDD)
{
	int	ret			=-1;
	int	numSelected	=0;
	for(const BoneDisplayData *pCur=pBDD;pCur != NULL;pCur=pCur->hh.next)
	{
		if(pCur->mbSelected)
		{
			numSelected++;
			ret	=pCur->mpNode->mIndex;
		}
	}

	if(numSelected == 1)
	{
		return	ret;
	}

	return	-1;
}

static bool	sIsSelected(const BoneDisplayData *pBDD, const GSNode *pNode)
{
	BoneDisplayData	*pFound;

	HASH_FIND_PTR(pBDD, &pNode, pFound);
	if(pFound == NULL)
	{
		return	false;
	}
	return	pFound->mbSelected;
}

static bool	sIsCollapsed(const BoneDisplayData *pBDD, const GSNode *pNode)
{
	BoneDisplayData	*pFound;

	HASH_FIND_PTR(pBDD, &pNode, pFound);
	if(pFound == NULL)
	{
		return	false;
	}
	return	pFound->mbCollapsed;
}

static bool	sIsAnimating(const BoneDisplayData *pBDD, const GSNode *pNode)
{
	BoneDisplayData	*pFound;

	HASH_FIND_PTR(pBDD, &pNode, pFound);
	if(pFound == NULL)
	{
		return	false;
	}
	return	pFound->mbAnimating;
}

static void sRenderNodeCollisionShape(const SkellyEditor *pSKE,
	int nodeIndex, const vec3 lightDir, const vec4 colour)
{
	if(pSKE->mpChar == NULL)
	{
		return;
	}

	const Skin	*pSkin	=Character_GetConstSkin(pSKE->mpChar);
	int			choice	=Skin_GetBoundChoice(pSkin, nodeIndex);
	if(choice == BONE_COL_SHAPE_INVALID)
	{
		return;
	}

	const Skeleton	*pSkel	=AnimLib_GetSkeleton(pSKE->mpALib);

	vec3	localScale		={	1,	1,	1	};

	mat4	boneMat;
	Skin_GetBoneByIndexNoBind(pSkin, pSkel, nodeIndex, boneMat);

	if(choice == BONE_COL_SHAPE_BOX)
	{
		vec3	min, max, center;

		Skin_GetBoxBoundSize(pSkin, nodeIndex, min, max);

		glm_vec3_center(min, max, center);

		glm_vec3_sub(max, min, localScale);

		glm_translate(boneMat, center);

		GD_IASetVertexBuffers(pSKE->mpGD, pSKE->mpCube->mpVB, pSKE->mpCube->mVertSize, 0);
		GD_IASetIndexBuffers(pSKE->mpGD, pSKE->mpCube->mpIB, DXGI_FORMAT_R16_UINT, 0);
	}
	else if(choice == BONE_COL_SHAPE_SPHERE)
	{
		vec4	size;

		Skin_GetSphereBoundSize(pSkin, nodeIndex, size);

		glm_translate(boneMat, size);

		glm_vec3_broadcast(size[3], localScale);

		GD_IASetVertexBuffers(pSKE->mpGD, pSKE->mpSphere->mpVB, pSKE->mpSphere->mVertSize, 0);
		GD_IASetIndexBuffers(pSKE->mpGD, pSKE->mpSphere->mpIB, DXGI_FORMAT_R16_UINT, 0);
	}
	else if(choice == BONE_COL_SHAPE_CAPSULE)
	{
		vec2	size;

		Skin_GetCapsuleBoundSize(pSkin, nodeIndex, size);

		//radius scales XZ, length Y, but the prim len is 4
		localScale[0]	=localScale[2]	=size[0];
		localScale[1]	=size[1] * 0.25f;

		//no translation for capsules I think?

		//bone space is right handed and z up
//		glm_mat4_mul(boneMat, s90Rot, boneMat);

		GD_IASetVertexBuffers(pSKE->mpGD, pSKE->mpCapsule->mpVB, pSKE->mpCapsule->mVertSize, 0);
		GD_IASetIndexBuffers(pSKE->mpGD, pSKE->mpCapsule->mpIB, DXGI_FORMAT_R16_UINT, 0);
	}

	GD_VSSetShader(pSKE->mpGD, pSKE->mpWNormWPos);
	GD_PSSetShader(pSKE->mpGD, pSKE->mpTriSolidSpec);
	GD_IASetInputLayout(pSKE->mpGD, pSKE->mpPrimLayout);

	//materialish stuff
	CBK_SetSolidColour(pSKE->mpCBK, colour);

	vec3	lightColour0	={	1,		1,		1		};
	vec3	lightColour1	={	0.8f,	0.8f,	0.8f	};
	vec3	lightColour2	={	0.6f,	0.6f,	0.6f	};
	vec3	specColour		={	1,		1,		1		};

	CBK_SetLocalScale(pSKE->mpCBK, localScale);

	CBK_SetTrilights3(pSKE->mpCBK, lightColour0, lightColour1, lightColour2, lightDir);
	CBK_SetSpecular(pSKE->mpCBK, specColour, 1.0f);

	CBK_SetWorldMat(pSKE->mpCBK, boneMat);
	CBK_UpdateObject(pSKE->mpCBK, pSKE->mpGD);

	if(choice == BONE_COL_SHAPE_BOX)
	{
		GD_DrawIndexed(pSKE->mpGD, pSKE->mpCube->mIndexCount, 0, 0);
	}
	else if(choice == BONE_COL_SHAPE_SPHERE)
	{
		GD_DrawIndexed(pSKE->mpGD, pSKE->mpSphere->mIndexCount, 0, 0);
	}
	else if(choice == BONE_COL_SHAPE_CAPSULE)
	{
		GD_DrawIndexed(pSKE->mpGD, pSKE->mpCapsule->mIndexCount, 0, 0);
	}
	else if(choice == BONE_COL_SHAPE_INVALID)
	{
		assert(0);
	}
}

//figure out what should go in the info box based
//on which nodes are selected
static void	sMakeInfoString(const SkellyEditor *pSKE)
{
	const GSNode	*pNode;

	int	numSelected	=0;
	for(const BoneDisplayData *pCur=pSKE->mpBDD;pCur != NULL;pCur=pCur->hh.next)
	{
		if(pCur->mbSelected)
		{
			numSelected++;
			pNode	=pCur->mpNode;
		}
	}

	if(numSelected <= 0)
	{
		return;
	}

	if(numSelected > 1)
	{
		sprintf(sInfoText, "Multiple bones selected.  Use C to collapse/expand bones, m to mirror selected, n to mark unused bones, or Del to delete.");
		return;
	}

	if(pSKE->mpChar == NULL)
	{
		sprintf(sInfoText, "Bone %s of index %d selected.  Load a character for collision info.",
			utstring_body(pNode->szName), pNode->mIndex);
		return;
	}

	const Skin	*pSkin	=Character_GetConstSkin(pSKE->mpChar);

	int	choice	=Skin_GetBoundChoice(pSkin, pNode->mIndex);

	if(pSKE->mbAdjustingMode)
	{
		if(choice == BONE_COL_SHAPE_BOX)
		{
			sprintf(sInfoText, "Adjusting bound of %s.  Keys y/u width, i/o length, [] depth, j to snap to joint, v/b to move, or \\ again to finish editing.",
				utstring_body(pNode->szName));
		}
		else if(choice == BONE_COL_SHAPE_SPHERE)
		{
			sprintf(sInfoText, "Adjusting bound of %s.  Keys g/h radius, j to snap to joint, v/b to move, or \\ again to finish editing.",
				utstring_body(pNode->szName));
		}
		else if(choice == BONE_COL_SHAPE_CAPSULE)
		{
			sprintf(sInfoText, "Adjusting bound of %s.  Keys i/o length, g/h radius, j to snap to joint, v/b to move, or \\ again to finish editing.",
				utstring_body(pNode->szName));
		}
		else
		{
			sprintf(sInfoText, "Invalid bound chosen for %s.  Press \\ again to finish editing.",
				utstring_body(pNode->szName));
		}
	}
	else
	{
		sprintf(sInfoText, "Bone %s of index %d using shape %s.  Press \\ to begin adjusting, or , to change shape.",
			utstring_body(pNode->szName), pNode->mIndex, sShapeNames[choice]);
	}
}


//dive thru the bone tree to make clay stuffs
static void sSkeletonLayout(const GSNode *pNode, SkellyEditor *pSKE, int colState)
{
	if(pNode == NULL)
	{
		return;
	}

	Clay_String	csNode;

	csNode.chars	=utstring_body(pNode->szName);
	csNode.length	=utstring_len(pNode->szName);

	//create a big encompassing box that contains
	//all child nodes too, will this work with no ID?
	Clay__OpenElement();

	Clay_ElementDeclaration	ced	={	.layout = { .childGap = 4,
		.padding = { 16, 0, 0, 0}, .layoutDirection = CLAY_TOP_TO_BOTTOM,
		.sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) }},
		.backgroundColor ={10, 221, 25, 45}	};

	Clay__ConfigureOpenElement(ced);

	//see if selected
	bool	bSelected	=sIsSelected(pSKE->mpBDD, pNode);

	//see if collapsed, if so recurse no further and add a +
	bool	bCollapsed	=sIsCollapsed(pSKE->mpBDD, pNode);

	//see if animating (opening or closing)
	bool	bAnimating	=sIsAnimating(pSKE->mpBDD, pNode);

	//see if bone has any children
	bool	bHasKids	=(pNode->mNumChildren > 0);

	//this is a 0 to BONE_VERTICAL_SIZE amount
	vec4	colAmount;
	Mover_GetPos(pSKE->mpBoneCollapse, colAmount);

	//width is always fit
	Clay_Sizing	cs;
	cs.width	=CLAY_SIZING_FIT(0);

	//height is either fit or semi squished if animating
	//TODO: maybe hide text while squishing?  It gets all goofy
	if(colState == NOT_COLLAPSING)
	{
		cs.height	=CLAY_SIZING_FIT(0);
	}
	else if(colState == GROWING)
	{
		cs.height	=CLAY_SIZING_FIXED(BONE_VERTICAL_SIZE - colAmount[0]);
	}
	else
	{
		cs.height	=CLAY_SIZING_FIXED(colAmount[0]);
	}

	//create an inner rect sized for the text
	Clay__OpenElement();

	//this one will have the bone name as an ID
	Clay__AttachId(Clay__HashString(csNode, 0, 0));

	Clay_ElementDeclaration	ced2	={	.layout = { .childGap = 4,
		.padding = { 8, 8, 2, 2 },	.sizing = cs},
		.cornerRadius = { 6 }, .backgroundColor = bSelected?
			COLOR_GOLD : (Clay_Hovered()? COLOR_ORANGE : COLOR_BLUE) };
	
	//is this where this goes?
	Clay_OnHover(sOnHoverBone, (intptr_t)pSKE);

	Clay__ConfigureOpenElement(ced2);
	
	//keep the passed in state
	int	childColState	=colState;
	if(bHasKids)
	{
		if(bCollapsed)
		{
			CLAY_TEXT(CLAY_STRING("+"), CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 0, 0, 255} }));
			if(bAnimating)
			{
				//if this node is collapsing, squish child nodes
				childColState	=COLLAPSING;
			}
		}
		else
		{
			CLAY_TEXT(CLAY_STRING("-"), CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 0, 0, 255} }));
			if(bAnimating)
			{
				//if this node is animating, grow child nodes
				childColState	=GROWING;
			}
		}
	}
	CLAY_TEXT(csNode, CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 0, 0, 255} })),

	//this closes the inner text nubbins
	Clay__CloseElement();

	//see if collapsed, if so recurse no further
	if(bCollapsed && !bAnimating)
	{
		//don't recurse (already closed nodes)
	}
	else if(!bCollapsed || pSKE->mbSEAnimating)
	{
		//children should parent off the big rect
		for(int i=0;i < pNode->mNumChildren;i++)
		{
			sSkeletonLayout(pNode->mpChildren[i], pSKE, childColState);
		}
	}

	//close big outer rect
	Clay__CloseElement();
}


//bone collision adjuster functions
static void	sRadiusAdjust(Skin *pSkin, int boneIdx, float amount)
{
	int	choice	=Skin_GetBoundChoice(pSkin, boneIdx);

	if(choice == BONE_COL_SHAPE_BOX)
	{
		printf("Radius increase for box?\n");
		return;
	}

	if(choice == BONE_COL_SHAPE_SPHERE)
	{
		vec4	size;
		Skin_GetSphereBoundSize(pSkin, boneIdx, size);

		size[3]	+=amount;

		Skin_SetSphereBoundSize(pSkin, boneIdx, size);
	}
	else if(choice == BONE_COL_SHAPE_CAPSULE)
	{
		vec2	size;
		Skin_GetCapsuleBoundSize(pSkin, boneIdx, size);

		size[0]	+=amount;

		Skin_SetCapsuleBoundSize(pSkin, boneIdx, size);
	}
}

static void	sLengthAdjust(Skin *pSkin, int boneIdx, float amount)
{
	int	choice	=Skin_GetBoundChoice(pSkin, boneIdx);

	if(choice == BONE_COL_SHAPE_BOX)
	{
		vec3	min, max;
		Skin_GetBoxBoundSize(pSkin, boneIdx, min, max);

		max[2]	+=amount;

		Skin_SetBoxBoundSize(pSkin, boneIdx, min, max);
	}
	else if(choice == BONE_COL_SHAPE_SPHERE)
	{
		printf("Length adjust for sphere!?\n");
	}
	else if(choice == BONE_COL_SHAPE_CAPSULE)
	{
		vec2	size;
		Skin_GetCapsuleBoundSize(pSkin, boneIdx, size);

		size[1]	+=amount;

		Skin_SetCapsuleBoundSize(pSkin, boneIdx, size);
	}
}


//input event handlers
static void CollapseBonesEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(pSKE->mbAdjustingMode)
	{
		return;
	}
	if(pSKE->mbSEAnimating)
	{
		return;
	}

	BoneDisplayData	*pBDD;

	//toggle collapse on selected and deselect
	for(pBDD=pSKE->mpBDD;pBDD != NULL;pBDD=pBDD->hh.next)
	{
		if(pBDD->mbSelected)
		{
			pBDD->mbSelected	=false;
			pBDD->mbCollapsed	=!pBDD->mbCollapsed;
			pBDD->mbAnimating	=true;
		}
	}

	Mover_SetUpMove(pSKE->mpBoneCollapse,
		(vec4){BONE_VERTICAL_SIZE,0,0,0}, (vec4){0,0,0,0},
		COLLAPSE_INTERVAL, 0.2f, 0.2f);

	pSKE->mbSEAnimating	=true;
}

static void MarkUnusedBonesEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void DeleteBonesEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void SkelPopOutEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	vec4	startPos	={0};
	vec4	endPos		={0};

	endPos[0]	=300;

	vec4	mvPos;
	Mover_GetPos(pSKE->mpSEM, mvPos);

	if(mvPos[0] > 0.0f)
	{
		if(pSKE->mbSEVisible)
		{
			//closing from partway open
			Mover_SetUpMove(pSKE->mpSEM, mvPos, startPos,
				COLLAPSE_INTERVAL / 2.0f, 0.2f, 0.2f);
		}
		else
		{
			//opening from partway closed
			Mover_SetUpMove(pSKE->mpSEM, mvPos, endPos,
				COLLAPSE_INTERVAL / 2.0f, 0.2f, 0.2f);
		}
	}
	else
	{
		if(!pSKE->mbSEVisible)
		{
			Mover_SetUpMove(pSKE->mpSEM, startPos, endPos,
				COLLAPSE_INTERVAL, 0.2f, 0.2f);
		}
	}

	pSKE->mbSEVisible	=!pSKE->mbSEVisible;
}

static void	AdjustBoneBoundEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	int	sel	=sGetSingleNodeSelected(pSKE->mpBDD);
	if(sel == -1)
	{
		return;
	}

	//toggle
	pSKE->mbAdjustingMode	=!pSKE->mbAdjustingMode;

	if(pSKE->mbAdjustingMode)
	{
		pSKE->mAdjustingIndex	=sel;
	}
	else
	{
		pSKE->mAdjustingIndex	=-1;
	}
}

static void	BoxWidthIncreaseEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void	BoxWidthDecreaseEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void	LengthIncreaseEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
	if(pSKE->mpChar == NULL)
	{
		return;
	}

	int	sel	=sGetSingleNodeSelected(pSKE->mpBDD);
	if(sel == -1)
	{
		return;
	}

	Skin	*pSkin	=Character_GetSkin(pSKE->mpChar);
	if(pSkin == NULL)
	{
		return;
	}

	int	choice	=Skin_GetBoundChoice(pSkin, sel);

	sLengthAdjust(pSkin, sel, LENGTH_INCREMENT);
}

static void	LengthDecreaseEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
	if(pSKE->mpChar == NULL)
	{
		return;
	}

	int	sel	=sGetSingleNodeSelected(pSKE->mpBDD);
	if(sel == -1)
	{
		return;
	}

	Skin	*pSkin	=Character_GetSkin(pSKE->mpChar);
	if(pSkin == NULL)
	{
		return;
	}

	int	choice	=Skin_GetBoundChoice(pSkin, sel);

	sLengthAdjust(pSkin, sel, -LENGTH_INCREMENT);
}

static void	BoxDepthIncreaseEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void	BoxDepthDecreaseEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void	RadiusIncreaseEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
	if(pSKE->mpChar == NULL)
	{
		return;
	}

	int	sel	=sGetSingleNodeSelected(pSKE->mpBDD);
	if(sel == -1)
	{
		return;
	}

	Skin	*pSkin	=Character_GetSkin(pSKE->mpChar);
	if(pSkin == NULL)
	{
		return;
	}

	int	choice	=Skin_GetBoundChoice(pSkin, sel);

	sRadiusAdjust(pSkin, sel, RADIUS_INCREMENT);
}

static void	RadiusDecreaseEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
	if(pSKE->mpChar == NULL)
	{
		return;
	}

	int	sel	=sGetSingleNodeSelected(pSKE->mpBDD);
	if(sel == -1)
	{
		return;
	}

	Skin	*pSkin	=Character_GetSkin(pSKE->mpChar);
	if(pSkin == NULL)
	{
		return;
	}

	int	choice	=Skin_GetBoundChoice(pSkin, sel);

	sRadiusAdjust(pSkin, sel, -RADIUS_INCREMENT);
}

static void	SnapToJointEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void	MirrorEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void	MoveForwardEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void	MoveBackwardEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(!pSKE->mbAdjustingMode)
	{
		return;
	}
}

static void	ChangeShapeEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

	if(pSKE->mbAdjustingMode)
	{
		return;
	}
	if(pSKE->mpChar == NULL)
	{
		return;
	}

	const GSNode	*pNode		=NULL;
	int		numSelected	=0;
	for(const BoneDisplayData *pCur=pSKE->mpBDD;pCur != NULL;pCur=pCur->hh.next)
	{
		if(pCur->mbSelected)
		{
			numSelected++;
			pNode	=pCur->mpNode;
		}
	}

	if(numSelected <= 0)
	{
		return;
	}

	if(numSelected > 1)
	{
		return;
	}

	Skin	*pSkin	=Character_GetSkin(pSKE->mpChar);
	if(pSkin == NULL)
	{
		return;
	}

	int	choice	=Skin_GetBoundChoice(pSkin, pNode->mIndex);

	choice++;

	if(choice > BONE_COL_SHAPE_INVALID)
	{
		choice	=0;
	}

	Skin_SetBoundChoice(pSkin, pNode->mIndex, choice);
}