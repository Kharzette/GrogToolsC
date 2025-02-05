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
#define	BONE_VERTICAL_SIZE	30
#define	COLLAPSE_INTERVAL	(0.25f)

//clay colours
#define COLOR_ORANGE		(Clay_Color) {225, 138, 50, 255}
#define COLOR_BLUE			(Clay_Color) {111, 173, 162, 255}
#define COLOR_GOLD			(Clay_Color) {255, 222, 162, 255}

//static data for making text
static char	sShapeNames[4][8]	={	{"Box"}, {"Sphere"}, {"Capsule"}, {"Invalid"}	};
static char	sInfoText[256];

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
	bool			mbSEVisible;	//user wants to see skelly editor
	bool			mbSEAnimating;	//stuff collapsing / growing

	//skelly popout movers
	Mover	*mpSEM;
	Mover	*mpBoneCollapse;
}	SkellyEditor;


//statics
static void sSetNodesAnimatingOff(BoneDisplayData *pBDD);
static void sSkeletonLayout(const GSNode *pNode, SkellyEditor *pSKE, int colState);
static bool sIsAnyNodeSelected(BoneDisplayData *pBDD);
static void	sMakeInfoString(const BoneDisplayData *pBDD, const Character *pChr);
static void sRenderNodeCollisionShape(const SkellyEditor *pSKE,
				const GSNode *pNode, const vec3 lightDir);

//input event handlers
static void CollapseBonesEH(void *pContext, const SDL_Event *pEvt);
static void MarkUnusedBonesEH(void *pContext, const SDL_Event *pEvt);
static void DeleteBonesEH(void *pContext, const SDL_Event *pEvt);
static void	SkelPopOutEH(void *pContext, const SDL_Event *pEvt);


SkellyEditor	*SKE_Create(StuffKeeper *pSK, GraphicsDevice *pGD,
			CBKeeper *pCBK, Input *pInp)
{
	SkellyEditor	*pRet	=malloc(sizeof(SkellyEditor));
	memset(pRet, 0, sizeof(SkellyEditor));

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
	pRet->mpCube	=PF_CreateCube(1.0f, false, pGD);
	pRet->mpSphere	=PF_CreateSphere((vec3){0,0,0}, 1.0f, pGD);
	pRet->mpCapsule	=PF_CreateCapsule(1.0f, 2.0f, pGD);

	//input handlers
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_c, CollapseBonesEH, pRet);	//collapse/uncollapse selected bones
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_m, MarkUnusedBonesEH, pRet);	//mark bones that aren't used in vert weights
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_DELETE, DeleteBonesEH, pRet);	//nuke selected bones
	INP_MakeBindingCTX(pInp, INP_BIND_TYPE_EVENT, SDLK_p, SkelPopOutEH, pRet);		//pop outsidebar

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
//	const Skin		*pSkin	=Character_GetSkin(pSKE->mpChar);
//	const Skeleton	*pSkel	=AnimLib_GetSkeleton(pSKE->mpALib);

	BoneDisplayData	*pCur;
//	GSNode			*pNode;

	int	numSelected	=0;
	for(pCur=pSKE->mpBDD;pCur != NULL;pCur=pCur->hh.next)
	{
		if(pCur->mbSelected)
		{
			sRenderNodeCollisionShape(pSKE, pCur->mpNode, lightDir);
			numSelected++;
		}
	}

	if(numSelected <= 0)
	{
		return;
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
			sMakeInfoString(pSKE->mpBDD, pSKE->mpChar);
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
	const GSNode *pNode, const vec3 lightDir)
{
	if(pSKE->mpChar == NULL)
	{
		return;
	}

	const Skin		*pSkin	=Character_GetSkin(pSKE->mpChar);
	const Skeleton	*pSkel	=AnimLib_GetSkeleton(pSKE->mpALib);

	int	choice	=Skin_GetBoundChoice(pSkin, pNode->mIndex);

	vec4	capSize;
	Skin_GetBoundSize(pSkin, pNode->mIndex, capSize);

	mat4	boneMat;
	Skin_GetBoneByIndexNoBind(pSkin, pSkel, pNode->mIndex, boneMat);

	GD_IASetVertexBuffers(pSKE->mpGD, pSKE->mpSphere->mpVB, pSKE->mpSphere->mVertSize, 0);
	GD_IASetIndexBuffers(pSKE->mpGD, pSKE->mpSphere->mpIB, DXGI_FORMAT_R16_UINT, 0);
	GD_VSSetShader(pSKE->mpGD, pSKE->mpWNormWPos);
	GD_PSSetShader(pSKE->mpGD, pSKE->mpTriSolidSpec);
	GD_IASetInputLayout(pSKE->mpGD, pSKE->mpPrimLayout);

	//materialish stuff
	CBK_SetSolidColour(pSKE->mpCBK, (vec4){1,1,1,1});

	vec3	lightColour0	={	1,		1,		1		};
	vec3	lightColour1	={	0.8f,	0.8f,	0.8f	};
	vec3	lightColour2	={	0.6f,	0.6f,	0.6f	};
	vec3	specColour		={	1,		1,		1		};
	vec3	localScale		={	capSize[3],	capSize[3],	capSize[3]	};

	CBK_SetLocalScale(pSKE->mpCBK, localScale);

	CBK_SetTrilights3(pSKE->mpCBK, lightColour0, lightColour1, lightColour2, lightDir);
	CBK_SetSpecular(pSKE->mpCBK, specColour, 1.0f);

	CBK_SetWorldMat(pSKE->mpCBK, boneMat);
	CBK_UpdateObject(pSKE->mpCBK, pSKE->mpGD);

	GD_DrawIndexed(pSKE->mpGD, pSKE->mpSphere->mIndexCount, 0, 0);
}

//figure out what should go in the info box based
//on which nodes are selected
static void	sMakeInfoString(const BoneDisplayData *pBDD, const Character *pChr)
{
	const GSNode	*pNode;

	int	numSelected	=0;
	for(const BoneDisplayData *pCur=pBDD;pCur != NULL;pCur=pCur->hh.next)
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
		sprintf(sInfoText, "Multiple bones selected.  Use C to collapse/expand bones, or Del to delete.");
		return;
	}

	if(pChr == NULL)
	{
		sprintf(sInfoText, "Bone %s of index %d selected.  Load a character for collision info.",
			utstring_body(pNode->szName), pNode->mIndex);
		return;
	}

	const Skin	*pSkin	=Character_GetSkin(pChr);

	int	choice	=Skin_GetBoundChoice(pSkin, pNode->mIndex);

	sprintf(sInfoText, "Bone %s of index %d using collision shape %s",
		utstring_body(pNode->szName), pNode->mIndex, sShapeNames[choice]);
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


//input event handlers
static void CollapseBonesEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);

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
}

static void DeleteBonesEH(void *pContext, const SDL_Event *pEvt)
{
	SkellyEditor	*pSKE	=(SkellyEditor *)pContext;
	assert(pSKE);
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