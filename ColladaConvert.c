#include    <nappgui.h>
#include	<d3d11_1.h>
#include	"UtilityLib/GraphicsDevice.h"
#include	"UtilityLib/GameCamera.h"
#include	"UtilityLib/MiscStuff.h"
#include	"UtilityLib/ListStuff.h"
#include	"UtilityLib/StringStuff.h"
#include	"UtilityLib/DictionaryStuff.h"
#include	"MaterialLib/StuffKeeper.h"
#include	"MaterialLib/Material.h"
#include	"MaterialLib/MaterialLib.h"
#include	"MaterialLib/CBKeeper.h"
#include	"MaterialLib/PostProcess.h"
#include	"MeshLib/AnimLib.h"
#include	"MeshLib/Mesh.h"
#include	"MeshLib/Character.h"
#include	"MeshLib/CommonPrims.h"
#include	"InputLib/Input.h"


#define	RESX			1280
#define	RESY			720
#define	TIC_RATE		1.0f / 60.0f
#define	ROT_RATE		10.0f
#define	KEYTURN_RATE	0.01f
#define	MOVE_RATE		0.1f
#define	MOUSE_TO_ANG	0.001f
#define	POW_SLIDER_MAX	100

//this gets passed into events and such
//will likely grow
typedef struct AppContext_t
{
	//gui windows
	Window	*mpWnd;
	Window	*mpMatWnd;
	Window	*mpTexWnd;	//pops up a texture choice
	Window	*mpEditWnd;	//for editing text

	GraphicsDevice	*mpGD;
	GameCamera		*mpCam;
	StuffKeeper		*mpSK;
	CBKeeper		*mpCBK;
	PostProcess		*mpPP;
	Input			*mpInp;

	//loaded data
	AnimLib		*mpALib;
	Character	*mpChar;
//	StaticMesh	*mpStatic;
	MaterialLib	*mpMatLib;
	DictSZ		*mpMeshes;

	//prims
	LightRay	*mpLR;
	AxisXYZ		*mpAxis;

	//misc data
	vec3	mLightDir;
	vec3	mEyePos;
	float	mDeltaYaw, mDeltaPitch;
	bool	mbRunning, mbMouseLooking;
	int		mAnimIndex, mMatIndex;
	float	mAnimTime;

	//gui stuff
	ListBox		*mpMeshPartLB;
	ListBox		*mpMaterialLB;
	ListBox		*mpAnimLB;
	color_t		mChosen;		//returned from color dialog
	PopUp		*mpTexList;		//pops up to pick a texture
	Button		*mpMatStuff;	//new mat or rename mat
	Edit		*mpTextInput;	//for renaming things

	//material form controls
	Button		*mpTL0, *mpTL1, *mpTL2;
	Button		*mpSolid, *mpSpec;
	Slider		*mpSPow;
	ImageView	*mpSRV0;
	ImageView	*mpSRV1;
	PopUp		*mpShaderFile;	//stat/char/bsp etc
	PopUp		*mpVSPop;
	PopUp		*mpPSPop;
	Label		*mpPowVal;		//set by the slider

	//list of stuff loaded
	const StringList	*mpAnimList;
	const StringList	*mpMatList;
	const StringList	*mpMeshList;

}  AppContext;

//static forward decs
static void				SetupKeyBinds(Input *pInp);
static void				SetupRastVP(GraphicsDevice *pGD);
static const Image		*sMakeSmallVColourBox(vec3 colour);
static const Image		*sMakeSmallColourBox(color_t colour);
static int				sGetSelectedMeshPartIndex(AppContext *pApp);
static const char		*sGetSelectedMaterialName(AppContext *pApp);
static const Material	*sGetSelectedConstMaterial(AppContext *pApp);
static Material			*sGetSelectedMaterial(AppContext *pApp);
static void				sUpdateSelectedMaterial(AppContext *pApp);
static const Image		*sCreateTexImage(const UT_string *szTex);
static bool				SelectPopupItem(PopUp *pPop, const char *pSZ);

//input event handlers
static void	RandLightEH(void *pContext, const SDL_Event *pEvt);
static void	LeftMouseDownEH(void *pContext, const SDL_Event *pEvt);
static void	LeftMouseUpEH(void *pContext, const SDL_Event *pEvt);
static void	RightMouseDownEH(void *pContext, const SDL_Event *pEvt);
static void	RightMouseUpEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveForwardEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveBackEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveLeftEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveRightEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveUpEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveDownEH(void *pContext, const SDL_Event *pEvt);
static void	KeyTurnLeftEH(void *pContext, const SDL_Event *pEvt);
static void	KeyTurnRightEH(void *pContext, const SDL_Event *pEvt);
static void	KeyTurnUpEH(void *pContext, const SDL_Event *pEvt);
static void	KeyTurnDownEH(void *pContext, const SDL_Event *pEvt);
static void MouseMoveEH(void *pContext, const SDL_Event *pEvt);
static void EscEH(void *pContext, const SDL_Event *pEvt);

//button event handlers
static void sLoadCharacter(AppContext *pAC, Event *pEvt);
static void sLoadMaterialLib(AppContext *pAC, Event *pEvt);
static void sLoadAnimLib(AppContext *pAC, Event *pEvt);
static void sAssignMaterial(AppContext *pAC, Event *pEvt);
static void sShaderFileChanged(AppContext *pAC, Event *pEvt);
static void sShaderChanged(AppContext *pAC, Event *pEvt);
static void sSPowChanged(AppContext *pAC, Event *pEvt);
static void	sColourButtonClicked(AppContext *pAC, Event *pEvt);
static void sMatSelectionChanged(AppContext *pAC, Event *pEvt);
static void	sSRV0Clicked(AppContext *pAC, Event *pEvt);
static void	sSRV1Clicked(AppContext *pAC, Event *pEvt);
static void	sTexChosen(AppContext *pAC, Event *pEvt);
static void	sDoMatStuff(AppContext *pAC, Event *pEvt);	//contextual


static Window	*sCreateWindow(void)
{
	Window	*pWnd	=window_create(ekWINDOW_STD | ekWINDOW_ESC | ekWINDOW_RETURN | ekWINDOW_RESIZE);
	
	window_title(pWnd, "Collada Conversion Tool");
	
	return	pWnd;
}

static void sOnClose(AppContext *pApp, Event *e)
{
	const EvWinClose	*p	=event_params(e, EvWinClose);

	bool_t	*pClose	=event_result(e, bool_t);
	
	cassert_no_null(pApp);
	
	switch(p->origin)
	{
		case ekGUI_CLOSE_ESC:
			osapp_finish();
			break;

		case ekGUI_CLOSE_INTRO:
			*pClose	=FALSE;
			break;
		
		case ekGUI_CLOSE_BUTTON:
			osapp_finish();
			break;
		
		case ekGUI_CLOSE_DEACT:
			cassert_default();
	}
}

static void sFillShaderPopups(AppContext *pApp, const UT_string *szFile)
{
	//clear both
	popup_clear(pApp->mpVSPop);
	popup_clear(pApp->mpPSPop);

	const StringList	*szList	=StuffKeeper_GetVSEntryList(pApp->mpSK, szFile);

	const StringList	*pCur	=SZList_Iterate(szList);
	while(pCur != NULL)
	{
		popup_add_elem(pApp->mpVSPop, SZList_IteratorVal(pCur), NULL);
		pCur	=SZList_IteratorNext(pCur);
	}

	szList	=StuffKeeper_GetPSEntryList(pApp->mpSK, szFile);
	if(szList == NULL)
	{
		UT_string	*tri;
		utstring_new(tri);
		utstring_printf(tri, "Trilight");

		//pixel shaders use trilight file alot
		szList	=StuffKeeper_GetPSEntryList(pApp->mpSK, tri);
		utstring_done(tri);
	}

	pCur	=SZList_Iterate(szList);
	while(pCur != NULL)
	{
		popup_add_elem(pApp->mpPSPop, SZList_IteratorVal(pCur), NULL);
		pCur	=SZList_IteratorNext(pCur);
	}
}

static void	sCreateMatWindow(AppContext *pApp)
{
	//material window
	//edits what is currently selected in MaterialLB
	//don't really want a resizable, but have to set this
	//to set any size at all
	pApp->mpMatWnd	=window_create(ekWINDOW_TITLE | ekWINDOW_RESIZE);
	
	window_title(pApp->mpMatWnd, "Material");

	//eventually want this to tag alongside the main window
	window_origin(pApp->mpMatWnd, v2df(100.f, 200.f));

	pApp->mpTexWnd	=window_create(ekWINDOW_ESC | ekWINDOW_RETURN | ekWINDOW_RESIZE);

	//material should edit:
	//trilight values, solid colour, spec color and power
	//srv0 and 1, vert and pixel shader
	Layout	*pLay	=layout_create(5, 6);
	layout_margin(pLay, 10);

	Layout	*pTexLay	=layout_create(1, 1);

	//color buttons
	pApp->mpTL0		=button_push();
	pApp->mpTL1		=button_push();
	pApp->mpTL2		=button_push();
	pApp->mpSolid	=button_push();
	pApp->mpSpec	=button_push();

	const Image	*pBlock	=sMakeSmallVColourBox(GLM_VEC3_ONE);

	button_image(pApp->mpTL0, pBlock);
	button_image(pApp->mpTL1, pBlock);
	button_image(pApp->mpTL2, pBlock);
	button_image(pApp->mpSolid, pBlock);
	button_image(pApp->mpSpec, pBlock);

	//spec power
	pApp->mpSPow	=slider_create();
	Label	*pSPowL	=label_create();
	pApp->mpPowVal	=label_create();

	button_text(pApp->mpTL0, "Trilight 0");
	button_text(pApp->mpTL1, "Trilight 1");
	button_text(pApp->mpTL2, "Trilight 2");

	button_text(pApp->mpSolid, "Solid Colour");
	button_text(pApp->mpSpec, "Spec Colour");

	label_text(pSPowL, "Spec Power");
	label_text(pApp->mpPowVal, "0");

	//for size calculation
	label_size_text(pApp->mpPowVal, "100");

	//shaders
	pApp->mpShaderFile	=popup_create();
	pApp->mpVSPop		=popup_create();
	pApp->mpPSPop		=popup_create();
	pApp->mpTexList		=popup_create();

	//shaderfiles
	popup_add_elem(pApp->mpShaderFile, "Character", NULL);
	popup_add_elem(pApp->mpShaderFile, "Static", NULL);
	popup_add_elem(pApp->mpShaderFile, "BSP", NULL);

	//SRV images
	pApp->mpSRV0	=imageview_create();
	pApp->mpSRV1	=imageview_create();
	imageview_scale(pApp->mpSRV0, ekGUI_SCALE_ASPECTDW);
	imageview_scale(pApp->mpSRV1, ekGUI_SCALE_ASPECTDW);
	imageview_size(pApp->mpSRV0, s2di(64, 64));
	imageview_size(pApp->mpSRV1, s2di(64, 64));

	//shader combos at the top
	layout_popup(pLay, pApp->mpShaderFile, 0, 0);
	layout_popup(pLay, pApp->mpVSPop, 1, 0);
	layout_popup(pLay, pApp->mpPSPop, 2, 0);

	//trilight below on the left
	layout_button(pLay, pApp->mpTL0, 0, 1);
	layout_button(pLay, pApp->mpTL1, 0, 2);
	layout_button(pLay, pApp->mpTL2, 0, 3);

	//solid spec
	layout_button(pLay, pApp->mpSolid, 1, 1);
	layout_button(pLay, pApp->mpSpec, 1, 2);
	layout_label(pLay, pSPowL, 2, 1);
	layout_slider(pLay, pApp->mpSPow, 3, 1);
	layout_label(pLay, pApp->mpPowVal, 4, 1);	

	//right align the spec power label
	layout_halign(pLay, 2, 1, ekRIGHT);

	//images
	layout_imageview(pLay, pApp->mpSRV0, 3, 2);
	layout_imageview(pLay, pApp->mpSRV1, 3, 3);

	//texlist
	layout_popup(pTexLay, pApp->mpTexList, 0, 0);

	//events
	popup_OnSelect(pApp->mpShaderFile, listener(pApp, sShaderFileChanged, AppContext));
	popup_OnSelect(pApp->mpVSPop, listener(pApp, sShaderChanged, AppContext));
	popup_OnSelect(pApp->mpPSPop, listener(pApp, sShaderChanged, AppContext));
	slider_OnMoved(pApp->mpSPow, listener(pApp, sSPowChanged, AppContext));
	button_OnClick(pApp->mpTL0, listener(pApp, sColourButtonClicked, AppContext));
	button_OnClick(pApp->mpTL1, listener(pApp, sColourButtonClicked, AppContext));
	button_OnClick(pApp->mpTL2, listener(pApp, sColourButtonClicked, AppContext));
	button_OnClick(pApp->mpSolid, listener(pApp, sColourButtonClicked, AppContext));
	button_OnClick(pApp->mpSpec, listener(pApp, sColourButtonClicked, AppContext));
	imageview_OnClick(pApp->mpSRV0, listener(pApp, sSRV0Clicked, AppContext));
	imageview_OnClick(pApp->mpSRV1, listener(pApp, sSRV1Clicked, AppContext));
	popup_OnSelect(pApp->mpTexList, listener(pApp, sTexChosen, AppContext));

	Panel	*pPanel	=panel_create();

	panel_layout(pPanel, pLay);
	window_panel(pApp->mpMatWnd, pPanel);
	window_size(pApp->mpMatWnd, s2df(800, 300));

	Panel	*pTexPanel	=panel_create();

	panel_layout(pTexPanel, pTexLay);
	window_panel(pApp->mpTexWnd, pTexPanel);
	window_size(pApp->mpTexWnd, s2df(380, 50));
}

static AppContext	*sAppCreate(void)
{
	AppContext	*pApp	=heap_new0(AppContext);
	
	gui_language("");
	pApp->mpWnd		=sCreateWindow();
	pApp->mpEditWnd	=window_create(ekWINDOW_ESC | ekWINDOW_RETURN | ekWINDOW_RESIZE);

	window_origin(pApp->mpWnd, v2df(500.f, 200.f));
	window_OnClose(pApp->mpWnd, listener(pApp, sOnClose, AppContext));
	window_show(pApp->mpWnd);

	Layout	*pLay		=layout_create(3, 3);
	Layout	*pEditLay	=layout_create(1, 1);
	layout_margin(pLay, 10);

	Button	*pB0		=button_push();
	Button	*pB1		=button_push();
	Button	*pB2		=button_push();
	Button	*pB3		=button_push();
	pApp->mpMatStuff	=button_push();

	button_text(pB0, "Load Character");
	button_text(pB1, "Load MatLib");
	button_text(pB2, "Load AnimLib");
	button_text(pB3, "<- Assign Material <-");
	button_text(pApp->mpMatStuff, "New Material");

	pApp->mpMeshPartLB	=listbox_create();
	pApp->mpMaterialLB	=listbox_create();
	pApp->mpAnimLB		=listbox_create();

	pApp->mpTextInput	=edit_create();

	layout_button(pLay, pB0, 0, 0);
	layout_button(pLay, pB1, 1, 0);
	layout_button(pLay, pB2, 2, 0);
	layout_listbox(pLay, pApp->mpMeshPartLB, 0, 1);
	layout_button(pLay, pB3, 1, 1);
	layout_listbox(pLay, pApp->mpMaterialLB, 2, 1);
	layout_listbox(pLay, pApp->mpAnimLB, 0, 2);
	layout_button(pLay, pApp->mpMatStuff, 2, 2);

	layout_edit(pEditLay, pApp->mpTextInput, 0, 0);

	button_OnClick(pB0, listener(pApp, sLoadCharacter, AppContext));
	button_OnClick(pB1, listener(pApp, sLoadMaterialLib, AppContext));
	button_OnClick(pB2, listener(pApp, sLoadAnimLib, AppContext));
	button_OnClick(pB3, listener(pApp, sAssignMaterial, AppContext));
	listbox_OnSelect(pApp->mpMaterialLB, listener(pApp, sMatSelectionChanged, AppContext));
	button_OnClick(pApp->mpMatStuff, listener(pApp, sDoMatStuff, AppContext));

	Panel	*pPanel	=panel_create();

	panel_layout(pPanel, pLay);
	window_panel(pApp->mpWnd, pPanel);
	window_size(pApp->mpWnd, s2df(800, 600));

	Panel	*pEditPanel	=panel_create();

	panel_layout(pEditPanel, pEditLay);
	window_panel(pApp->mpEditWnd, pEditPanel);
	window_size(pApp->mpEditWnd, s2df(380, 50));

	sCreateMatWindow(pApp);

	//null loadable meshes
	pApp->mpChar	=NULL;

	//input and key / mouse bindings
	pApp->mpInp	=INP_CreateInput();
	SetupKeyBinds(pApp->mpInp);

	GD_Init(&pApp->mpGD, "Collada Tool", RESX, RESY, D3D_FEATURE_LEVEL_11_0);
	
	SetupRastVP(pApp->mpGD);

	pApp->mpSK	=StuffKeeper_Create(pApp->mpGD);
	if(pApp->mpSK == NULL)
	{
		printf("Couldn't create StuffKeeper!\n");
		GD_Destroy(&pApp->mpGD);
		heap_free((uint8_t **)&pApp, sizeof(AppContext), NULL);
		return	NULL;
	}

	//manually call event to fill boxes
	sShaderFileChanged(pApp, NULL);

	//test prims
	pApp->mpLR		=CP_CreateLightRay(5.0f, 0.25f, pApp->mpGD, pApp->mpSK);
	pApp->mpAxis	=CP_CreateAxis(5.0f, 0.1f, pApp->mpGD, pApp->mpSK);

	pApp->mpCBK	=CBK_Create(pApp->mpGD);
	pApp->mpPP	=PP_Create(pApp->mpGD, pApp->mpSK, pApp->mpCBK);

	//set sky gradient
	{
		vec3	skyHorizon	={	0.0f, 0.5f, 1.0f	};
		vec3	skyHigh		={	0.0f, 0.25f, 1.0f	};

		CBK_SetSky(pApp->mpCBK, skyHorizon, skyHigh);
		CBK_SetFogVars(pApp->mpCBK, 50.0f, 300.0f, true);
	}

	PP_MakePostTarget(pApp->mpPP, pApp->mpGD, "LinearColor", RESX, RESY, DXGI_FORMAT_R8G8B8A8_UNORM);
	PP_MakePostDepth(pApp->mpPP, pApp->mpGD, "LinearDepth", RESX, RESY, DXGI_FORMAT_D32_FLOAT);

	float	aspect	=(float)RESX / (float)RESY;

	pApp->mEyePos[1]	=3.6f;
	pApp->mEyePos[2]	=-4.5f;
	pApp->mEyePos[0]	=-3.0f;

	//game camera
	pApp->mpCam	=GameCam_Create(false, 0.1f, 2000.0f, GLM_PI_4f, aspect, 1.0f, 10.0f);
	//3D Projection
	mat4	camProj;
	GameCam_GetProjection(pApp->mpCam, camProj);
	CBK_SetProjection(pApp->mpCBK, camProj);

	//2d projection for text
	mat4	textProj;
	glm_ortho(0, RESX, RESY, 0, -1.0f, 1.0f, textProj);

	//set constant buffers to shaders, think I just have to do this once
	CBK_SetCommonCBToShaders(pApp->mpCBK, pApp->mpGD);

	pApp->mLightDir[0]		=0.3f;
	pApp->mLightDir[1]		=-0.7f;
	pApp->mLightDir[2]		=-0.5f;

	glm_vec3_normalize(pApp->mLightDir);

	pApp->mbRunning	=true;

	return	pApp;
}

static void	sAppDestroy(AppContext **ppApp)
{
	GD_Destroy(&((*ppApp)->mpGD));

	window_destroy(&(*ppApp)->mpWnd);

	heap_delete(ppApp, AppContext);
}

static void sRender(AppContext *pApp, const real64_t prTime, const real64_t cTime)
{
	vec4	lightRayCol	={	1.0f, 1.0f, 0.0f, 1.0f	};
	vec4	XAxisCol	={	1.0f, 0.0f, 0.0f, 1.0f	};
	vec4	YAxisCol	={	0.0f, 0.0f, 1.0f, 1.0f	};
	vec4	ZAxisCol	={	0.0f, 1.0f, 0.0f, 1.0f	};

	if(pApp->mpALib != NULL)
	{
		int	animCount	=listbox_count(pApp->mpAnimLB);
		int	selected	=-1;
		for(int i=0;i < animCount;i++)
		{
			if(listbox_selected(pApp->mpAnimLB, i))
			{
				selected	=i;
				break;
			}
		}

		if(selected >= 0)
		{
			int					index	=0;
			const StringList	*pCur	=SZList_Iterate(pApp->mpAnimList);
			while(pCur != NULL)
			{
				if(index == selected)
				{
					AnimLib_Animate(pApp->mpALib, SZList_IteratorVal(pCur), pApp->mAnimTime);
				}
				pCur	=SZList_IteratorNext(pCur);
				index++;
			}
		}
	}

	//set no blend, I think post processing turns it on maybe
	GD_OMSetBlendState(pApp->mpGD, StuffKeeper_GetBlendState(pApp->mpSK, "NoBlending"));
	GD_PSSetSampler(pApp->mpGD, StuffKeeper_GetSamplerState(pApp->mpSK, "PointWrap"), 0);

	//camera update
	GameCam_UpdateRotation(pApp->mpCam, pApp->mEyePos, pApp->mDeltaPitch,
							pApp->mDeltaYaw, 0.0f);

	//set CB view
	{
		mat4	viewMat;
		GameCam_GetViewMatrixFly(pApp->mpCam, viewMat, pApp->mEyePos);

		CBK_SetView(pApp->mpCBK, viewMat, pApp->mEyePos);
	}


	PP_SetTargets(pApp->mpPP, pApp->mpGD, "LinearColor", "LinearDepth");

	PP_ClearDepth(pApp->mpPP, pApp->mpGD, "LinearDepth");
	PP_ClearTarget(pApp->mpPP, pApp->mpGD, "LinearColor");

	//update frame CB
	CBK_UpdateFrame(pApp->mpCBK, pApp->mpGD);

	//draw light ray
	{
		vec3	rayLoc	={	0.0f, 5.0f, 0.0f	};
		CP_DrawLightRay(pApp->mpLR, pApp->mLightDir, lightRayCol, rayLoc, pApp->mpCBK, pApp->mpGD);
	}

	//draw xyz axis
	CP_DrawAxis(pApp->mpAxis, pApp->mLightDir, XAxisCol, YAxisCol, ZAxisCol, pApp->mpCBK, pApp->mpGD);

	GD_PSSetSampler(pApp->mpGD, StuffKeeper_GetSamplerState(pApp->mpSK, "PointClamp"), 0);

	//draw mesh
	if(pApp->mpMeshes != NULL && pApp->mpChar != NULL &&
		pApp->mpALib != NULL && pApp->mpMatLib != NULL)
	{
		Character_Draw(pApp->mpChar, pApp->mpMeshes, pApp->mpMatLib,
						pApp->mpALib, pApp->mpGD, pApp->mpCBK);
	}

	PP_ClearDepth(pApp->mpPP, pApp->mpGD, "BackDepth");
	PP_SetTargets(pApp->mpPP, pApp->mpGD, "BackColor", "BackDepth");

	PP_SetSRV(pApp->mpPP, pApp->mpGD, "LinearColor", 1);	//1 for colortex

	PP_DrawStage(pApp->mpPP, pApp->mpGD, pApp->mpCBK);

	GD_Present(pApp->mpGD);
}

static void sAppUpdate(AppContext *pApp, const real64_t prTime, const real64_t cTime)
{
	if(pApp == NULL)
	{
		//init not finished
		return;
	}
	
	if(!pApp->mbRunning)
	{
		osapp_finish();
	}

	pApp->mAnimTime		+=(cTime / 1000.0);

	pApp->mDeltaYaw		=0.0f;
	pApp->mDeltaPitch	=0.0f;

	INP_Update(pApp->mpInp, pApp);

	sRender(pApp, prTime, cTime);
}

//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wundefined-inline"
#include "osmain.h"
//#pragma GCC diagnostic pop

osmain_sync(TIC_RATE, sAppCreate, sAppDestroy, sAppUpdate, "", AppContext)

static void	SetupKeyBinds(Input *pInp)
{
	//event style bindings
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_l, RandLightEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_ESCAPE, EscEH);

	//held bindings
	//movement
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_w, KeyMoveForwardEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_a, KeyMoveLeftEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_s, KeyMoveBackEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_d, KeyMoveRightEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_c, KeyMoveUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_z, KeyMoveDownEH);

	//key turning
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_q, KeyTurnLeftEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_e, KeyTurnRightEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_r, KeyTurnUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_t, KeyTurnDownEH);

	//move data events
	INP_MakeBinding(pInp, INP_BIND_TYPE_MOVE, SDL_MOUSEMOTION, MouseMoveEH);

	//down/up events
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDL_BUTTON_RIGHT, RightMouseDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDL_BUTTON_RIGHT, RightMouseUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDL_BUTTON_LEFT, LeftMouseDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDL_BUTTON_LEFT, LeftMouseUpEH);
}

static void	SetupRastVP(GraphicsDevice *pGD)
{
	D3D11_RASTERIZER_DESC	rastDesc;
	rastDesc.AntialiasedLineEnable	=false;
	rastDesc.CullMode				=D3D11_CULL_BACK;
	rastDesc.FillMode				=D3D11_FILL_SOLID;
	rastDesc.FrontCounterClockwise	=true;
	rastDesc.MultisampleEnable		=false;
	rastDesc.DepthBias				=0;
	rastDesc.DepthBiasClamp			=0;
	rastDesc.DepthClipEnable		=true;
	rastDesc.ScissorEnable			=false;
	rastDesc.SlopeScaledDepthBias	=0;
	ID3D11RasterizerState	*pRast	=GD_CreateRasterizerState(pGD, &rastDesc);

	D3D11_VIEWPORT	vp;

	vp.Width	=RESX;
	vp.Height	=RESY;
	vp.MaxDepth	=1.0f;
	vp.MinDepth	=0.0f;
	vp.TopLeftX	=0;
	vp.TopLeftY	=0;

	GD_RSSetViewPort(pGD, &vp);
	GD_RSSetState(pGD, pRast);
	GD_IASetPrimitiveTopology(pGD, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

//event handlers (eh)
static void	RandLightEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	Misc_RandomDirection(pTS->mLightDir);

	//copy changes to the material selected
	sUpdateSelectedMaterial(pTS);
}

static void	LeftMouseDownEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);
}

static void	LeftMouseUpEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);
}

static void	RightMouseDownEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	SDL_SetRelativeMouseMode(SDL_TRUE);

	pTS->mbMouseLooking	=true;
}

static void	RightMouseUpEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	SDL_SetRelativeMouseMode(SDL_FALSE);

	pTS->mbMouseLooking	=false;
}

static void	MouseMoveEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	if(pTS->mbMouseLooking)
	{
		pTS->mDeltaYaw		+=(pEvt->motion.xrel * MOUSE_TO_ANG);
		pTS->mDeltaPitch	+=(pEvt->motion.yrel * MOUSE_TO_ANG);
	}
}

static void	KeyMoveForwardEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	vec3	forward;
	GameCam_GetForwardVec(pTS->mpCam, forward);
	glm_vec3_scale(forward, MOVE_RATE, forward);

	glm_vec3_add(pTS->mEyePos, forward, pTS->mEyePos);
}

static void	KeyMoveBackEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	vec3	forward;
	GameCam_GetForwardVec(pTS->mpCam, forward);
	glm_vec3_scale(forward, MOVE_RATE, forward);

	glm_vec3_sub(pTS->mEyePos, forward, pTS->mEyePos);
}

static void	KeyMoveLeftEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	vec3	right;
	GameCam_GetRightVec(pTS->mpCam, right);
	glm_vec3_scale(right, MOVE_RATE, right);

	glm_vec3_sub(pTS->mEyePos, right, pTS->mEyePos);
}

static void	KeyMoveRightEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	vec3	right;
	GameCam_GetRightVec(pTS->mpCam, right);
	glm_vec3_scale(right, MOVE_RATE, right);

	glm_vec3_add(pTS->mEyePos, right, pTS->mEyePos);
}

static void	KeyMoveUpEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	vec3	up;
	GameCam_GetUpVec(pTS->mpCam, up);
	glm_vec3_scale(up, MOVE_RATE, up);

	glm_vec3_add(pTS->mEyePos, up, pTS->mEyePos);
}

static void	KeyMoveDownEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	vec3	up;
	GameCam_GetUpVec(pTS->mpCam, up);
	glm_vec3_scale(up, MOVE_RATE, up);

	glm_vec3_sub(pTS->mEyePos, up, pTS->mEyePos);
}

static void	KeyTurnLeftEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	pTS->mDeltaYaw	+=KEYTURN_RATE;
}

static void	KeyTurnRightEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	pTS->mDeltaYaw	-=KEYTURN_RATE;
}

static void	KeyTurnUpEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	pTS->mDeltaPitch	+=KEYTURN_RATE;
}

static void	KeyTurnDownEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	pTS->mDeltaPitch	-=KEYTURN_RATE;
}

static void	EscEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	pTS->mbRunning	=false;
}


static void sLoadCharacter(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"Character", "character"	};

	const char	*pFileName	=comwin_open_file(pAC->mpWnd, fTypes, 2, NULL);

	printf("FileName: %s\n", pFileName);

	pAC->mpChar	=Character_Read(pFileName);

	printf("Character loaded...\n");

	pAC->mpMeshList	=Character_GetPartList(pAC->mpChar);

	DictSZ_New(&pAC->mpMeshes);

	UT_string	*szPath	=SZ_StripFileName(pFileName);

	UT_string	*szFullPath;
	utstring_new(szFullPath);

	const StringList	*pCur	=SZList_Iterate(pAC->mpMeshList);
	while(pCur != NULL)
	{
		utstring_printf(szFullPath, "%s/%s.mesh", utstring_body(szPath), SZList_IteratorVal(pCur));

		Mesh	*pMesh	=Mesh_Read(pAC->mpGD, pAC->mpSK, utstring_body(szFullPath));

		DictSZ_Add(&pAC->mpMeshes, SZList_IteratorValUT(pCur), pMesh);

		listbox_add_elem(pAC->mpMeshPartLB, SZList_IteratorVal(pCur), NULL);

		pCur	=SZList_IteratorNext(pCur);
	}
}

static void sLoadMaterialLib(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"MatLib", "Matlib", "matlib"	};

	const char	*pFileName	=comwin_open_file(pAC->mpWnd, fTypes, 3, NULL);

	printf("FileName: %s\n", pFileName);

	if(pFileName == NULL)
	{
		return;
	}

	pAC->mpMatLib	=MatLib_Read(pFileName, pAC->mpSK);

	pAC->mpMatList	=MatLib_GetMatList(pAC->mpMatLib);

	printf("Material lib loaded...\n");

	const StringList	*pCur	=SZList_Iterate(pAC->mpMatList);

	while(pCur != NULL)
	{
		printf("\t%s\n", SZList_IteratorVal(pCur));

		listbox_add_elem(pAC->mpMaterialLB, SZList_IteratorVal(pCur), NULL);

		pCur	=SZList_IteratorNext(pCur);
	}
}

static void sLoadAnimLib(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"AnimLib", "Animlib", "animlib"	};

	const char	*pFileName	=comwin_open_file(pAC->mpWnd, fTypes, 3, NULL);

	printf("FileName: %s\n", pFileName);

	pAC->mpALib	=AnimLib_Read(pFileName);

	pAC->mpAnimList	=AnimLib_GetAnimList(pAC->mpALib);

	printf("Anim lib loaded...\n");

	const StringList	*pCur	=SZList_Iterate(pAC->mpAnimList);

	while(pCur != NULL)
	{
		printf("\t%s\n", SZList_IteratorVal(pCur));

		listbox_add_elem(pAC->mpAnimLB, SZList_IteratorVal(pCur), NULL);

		pCur	=SZList_IteratorNext(pCur);
	}
}

static void sAssignMaterial(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*szMatSel	=sGetSelectedMaterialName(pAC);
	int	meshSelected		=sGetSelectedMeshPartIndex(pAC);

	Character_AssignMaterial(pAC->mpChar, meshSelected, szMatSel);

	printf("Assigned material %s to mesh part %d\n", szMatSel, meshSelected);
}

static int sGetSelectedIndex(const ListBox *pLB)
{
	//change the text in the listbox too
	int	count	=listbox_count(pLB);
	int	seld	=-1;
	for(int i=0;i < count;i++)
	{
		if(listbox_selected(pLB, i))
		{
			seld	=i;
			break;
		}
	}
	return	seld;
}

static void sDoMatStuff(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	//if a material is selected, this will rename it
	//if no material is selected, create a new one
	const char	*szMatSel	=sGetSelectedMaterialName(pAC);

	if(szMatSel == NULL)
	{
		//if there's no material lib yet, create one
		if(pAC->mpMatLib == NULL)
		{
			pAC->mpMatLib	=MatLib_Create(pAC->mpSK);
		}

		Material	*pMat	=MAT_Create(pAC->mpGD);

		char	defMatName[32]	="NewMat";
		int		dupeCount		=0;

		Material	*pOtherMat	=MatLib_GetMaterial(pAC->mpMatLib, defMatName);
		while(pOtherMat != NULL)
		{
			sprintf(defMatName, "NewMat%03d", dupeCount);
			dupeCount++;

			pOtherMat	=MatLib_GetMaterial(pAC->mpMatLib, defMatName);
		}

		MatLib_Add(pAC->mpMatLib, defMatName, pMat);

		printf("New material %s created.\n", defMatName);

		listbox_add_elem(pAC->mpMaterialLB, defMatName, NULL);
	}
	else
	{
		//rename
		//get the screen location for the selected material
		V2Df	pos		=window_get_origin(pAC->mpWnd);
		S2Df	size	=window_get_client_size(pAC->mpWnd);

		V2Df	goodPos	=pos;

		goodPos.x	+=size.width * 0.75f;
		goodPos.y	+=size.height / 4;

		//I thought about trying to bump Y a bit
		//according to the index selected, but that
		//doesn't work if there's a scrollbar involved
		window_origin(pAC->mpEditWnd, goodPos);
		
		uint32_t	ret	=window_modal(pAC->mpEditWnd, pAC->mpWnd);
		if(ret != ekGUI_CLOSE_ESC)
		{
			MatLib_ReName(pAC->mpMatLib, szMatSel, edit_get_text(pAC->mpTextInput));

			//change the text in the listbox too
			int	seld	=sGetSelectedIndex(pAC->mpMaterialLB);
			if(seld != -1)
			{
				listbox_set_elem(pAC->mpMaterialLB, seld, edit_get_text(pAC->mpTextInput), NULL);
			}
		}
	}
}

static void sShaderFileChanged(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	uint32_t	seld	=popup_get_selected(pAC->mpShaderFile);

	UT_string	*pSZFile;
	utstring_new(pSZFile);

	utstring_printf(pSZFile, "%s", popup_get_text(pAC->mpShaderFile, seld));

	sFillShaderPopups(pAC, pSZFile);

	utstring_done(pSZFile);
}

static void sTexChosen(AppContext *pAC, Event *pEvt)
{
	Material	*pMat	=sGetSelectedMaterial(pAC);
	if(pMat == NULL)
	{
		unref(pEvt);
		window_stop_modal(pAC->mpTexWnd, ekGUI_CLOSE_ESC);
		return;
	}

	const EvButton	*pB	=event_params(pEvt, EvButton);

	//tag determines which SRV was clicked
	uint32_t	tag	=guicontrol_get_tag((GuiControl *)pAC->mpTexList);

	if(tag == 0)
	{
		MAT_SetSRV0(pMat, pB->text, pAC->mpSK);
	}
	else
	{
		MAT_SetSRV1(pMat, pB->text, pAC->mpSK);
	}

	window_stop_modal(pAC->mpTexWnd, 69);
}

static void sShaderChanged(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	//copy changes to the material selected
	sUpdateSelectedMaterial(pAC);
}

static void sSPowChanged(AppContext *pAC, Event *pEvt)
{
	const EvSlider	*pSlide	=event_params(pEvt, EvSlider);

	float	newPow	=pSlide->pos * POW_SLIDER_MAX;

	char	val[6];

	sprintf(val, "%d", (int)newPow);

	label_text(pAC->mpPowVal, val);
	
	//copy changes to the material selected
	sUpdateSelectedMaterial(pAC);
}

static void sColourChosen(AppContext *pAC, Event *pEvt)
{
	color_t	*pCol	=event_params(pEvt, color_t);

	pAC->mChosen	=*pCol;
}

static void	sUpdateSelectedMaterial(AppContext *pApp)
{
	Material	*pMat	=sGetSelectedMaterial(pApp);
	if(pMat == NULL)
	{
		return;
	}

	vec3	t0, t1, t2;

	Misc_RGBAToVec3(button_get_tag(pApp->mpTL0), t0);
	Misc_RGBAToVec3(button_get_tag(pApp->mpTL1), t1);
	Misc_RGBAToVec3(button_get_tag(pApp->mpTL2), t2);

	MAT_SetLights(pMat, t0, t1, t2, pApp->mLightDir);

	vec4	solid;
	vec3	spec;
	float	specPower;
	Misc_RGBAToVec4(button_get_tag(pApp->mpSolid), solid);
	Misc_RGBAToVec3(button_get_tag(pApp->mpSpec), spec);

	specPower	=slider_get_value(pApp->mpSPow) * POW_SLIDER_MAX;

	MAT_SetSolidColour(pMat, solid);
	MAT_SetSpecular(pMat, spec, specPower);

	//shaders
	uint32_t	seld	=popup_get_selected(pApp->mpVSPop);

	UT_string	*pSZShader;
	utstring_new(pSZShader);

	utstring_printf(pSZShader, "%s", popup_get_text(pApp->mpVSPop, seld));

	MAT_SetVShader(pMat, utstring_body(pSZShader), pApp->mpSK);

	utstring_clear(pSZShader);

	seld	=popup_get_selected(pApp->mpPSPop);

	utstring_printf(pSZShader, "%s", popup_get_text(pApp->mpPSPop, seld));

	MAT_SetPShader(pMat, utstring_body(pSZShader), pApp->mpSK);

	utstring_done(pSZShader);
}

//pLapp pLapp pLapp, get texture, get texture, get texture
static void SpawnTexWindow(AppContext *pLapp, int idx)
{
	Material	*pMat	=sGetSelectedMaterial(pLapp);
	if(pMat == NULL)
	{
		return;
	}

	StringList	*pTexs	=StuffKeeper_GetTextureList(pLapp->mpSK);
	if(pTexs == NULL)
	{
		return;
	}

	popup_clear(pLapp->mpTexList);

	const StringList	*pT	=SZList_Iterate(pTexs);
	while(pT != NULL)
	{
		printf("%s\n", SZList_IteratorVal(pT));

		popup_add_elem(pLapp->mpTexList, SZList_IteratorVal(pT), NULL);

		pT	=SZList_IteratorNext(pT);
	}

	SZList_Clear(&pTexs);

	const ID3D11ShaderResourceView	*pSRV	=(idx == 0)? MAT_GetSRV0(pMat) : MAT_GetSRV1(pMat);
	if(pSRV != NULL)
	{
		const UT_string	*pSRVName	=StuffKeeper_GetSRVName(pLapp->mpSK, pSRV);
		SelectPopupItem(pLapp->mpTexList, utstring_body(pSRVName));
	}

	//get the screen location for the SRV button
	V2Df	pos		=window_get_origin(pLapp->mpMatWnd);
	S2Df	size	=window_get_client_size(pLapp->mpMatWnd);

	V2Df	goodPos	=pos;

	goodPos.x	+=size.width;
	goodPos.y	+=size.height / 2;

	if(idx != 0)
	{
		goodPos.y	+=100;
	}

	window_origin(pLapp->mpTexWnd, goodPos);
}

static void UpdateSRVImages(AppContext *pApp)
{
	Material	*pMat	=sGetSelectedMaterial(pApp);
	if(pMat == NULL)
	{
		return;
	}

	//srvs
	const ID3D11ShaderResourceView	*pSRV0	=MAT_GetSRV0(pMat);
	const ID3D11ShaderResourceView	*pSRV1	=MAT_GetSRV1(pMat);

	const UT_string	*pSRV0Name	=StuffKeeper_GetSRVName(pApp->mpSK, pSRV0);
	const UT_string	*pSRV1Name	=StuffKeeper_GetSRVName(pApp->mpSK, pSRV1);

	const Image	*pS0	=sCreateTexImage(pSRV0Name);
	const Image	*pS1	=sCreateTexImage(pSRV1Name);

	imageview_image(pApp->mpSRV0, pS0);
	imageview_image(pApp->mpSRV1, pS1);
}

static void sSRVClicked(AppContext *pApp, int idx)
{
	//tag with SRV index
	guicontrol_tag((GuiControl *)pApp->mpTexList, idx);

	SpawnTexWindow(pApp, idx);

	uint32_t	ret	=window_modal(pApp->mpTexWnd, pApp->mpMatWnd);
	if(ret != ekGUI_CLOSE_ESC)
	{
		UpdateSRVImages(pApp);
	}
}

static void sSRV0Clicked(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);
	sSRVClicked(pAC, 0);
}

static void sSRV1Clicked(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);
	sSRVClicked(pAC, 1);
}

static void sColourButtonClicked(AppContext *pAC, Event *pEvt)
{
	__attribute_maybe_unused__
	const EvButton	*pBtn	=event_params(pEvt, EvButton);

	comwin_color(pAC->mpWnd, "Choose Colour", 100, 50, ekRIGHT, ekTOP, kCOLOR_WHITE, NULL, 0, listener(pAC, sColourChosen, AppContext));

	GuiControl	*pCur	=window_get_focus(pAC->mpMatWnd);

	Button	*pButn	=guicontrol_button(pCur);
	if(pButn == NULL)
	{
		return;
	}

	//store rgba color in tag
	button_tag(pButn, pAC->mChosen);

	//update little color box on the button
	button_image(pButn, sMakeSmallColourBox(pAC->mChosen));

	//copy changes to the material selected
	sUpdateSelectedMaterial(pAC);
}

static bool	SelectPopupItem(PopUp *pPop, const char *pSZ)
{
	int	cnt	=popup_count(pPop);

	for(int i=0;i < cnt;i++)
	{
		const char	*pItemTxt	=popup_get_text(pPop, i);

		int	result	=strcmp(pItemTxt, pSZ);
		if(result == 0)
		{
			popup_selected(pPop, i);
			return	true;
		}
	}
	return	false;
}

static void	sFillMaterialFormValues(AppContext *pApp, const char *szMaterial)
{
	if(pApp == NULL || szMaterial == NULL || pApp->mpMatLib == NULL)
	{
		printf("Not ready for material form fill\n");
		return;
	}

	const Material	*pMat	=MatLib_GetConstMaterial(pApp->mpMatLib, szMaterial);
	if(pMat == NULL)
	{
		return;
	}

	vec3	t0, t1, t2;
	MAT_GetTrilight(pMat, t0, t1, t2);
	button_image(pApp->mpTL0, sMakeSmallVColourBox(t0));
	button_image(pApp->mpTL1, sMakeSmallVColourBox(t1));
	button_image(pApp->mpTL2, sMakeSmallVColourBox(t2));

	vec4	sc;
	MAT_GetSolidColour(pMat, sc);
	button_image(pApp->mpSolid, sMakeSmallVColourBox(sc));

	vec4	spec;
	MAT_GetSpecular(pMat, spec);
	button_image(pApp->mpSpec, sMakeSmallVColourBox(spec));
	slider_value(pApp->mpSPow, (spec[3]) / (real32_t)POW_SLIDER_MAX);

	//set tags to colors
	button_tag(pApp->mpTL0, Misc_SSE_Vec3ToRGBA(t0));
	button_tag(pApp->mpTL1, Misc_SSE_Vec3ToRGBA(t1));
	button_tag(pApp->mpTL2, Misc_SSE_Vec3ToRGBA(t2));
	button_tag(pApp->mpSolid, Misc_SSE_Vec4ToRGBA(sc));
	button_tag(pApp->mpSpec, Misc_SSE_Vec3ToRGBA(spec));

	char	val[6];
	sprintf(val, "%d", (int)spec[3]);
	label_text(pApp->mpPowVal, val);

	//srvs
	UpdateSRVImages(pApp);

	const ID3D11VertexShader	*pVS	=MAT_GetVShader(pMat);
	const ID3D11PixelShader		*pPS	=MAT_GetPShader(pMat);

	if(pVS != NULL)
	{
		const UT_string	*pVSName	=StuffKeeper_GetVSName(pApp->mpSK, pVS);
		SelectPopupItem(pApp->mpVSPop, utstring_body(pVSName));
	}

	if(pPS != NULL)
	{
		const UT_string	*pPSName	=StuffKeeper_GetPSName(pApp->mpSK, pPS);
		SelectPopupItem(pApp->mpPSPop, utstring_body(pPSName));
	}
}

static void sMatSelectionChanged(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*szMatSel	=sGetSelectedMaterialName(pAC);
	if(szMatSel == NULL)
	{
		button_text(pAC->mpMatStuff, "Create Material");
		window_hide(pAC->mpMatWnd);
		return;
	}

	sFillMaterialFormValues(pAC, szMatSel);

	window_show(pAC->mpMatWnd);

	button_text(pAC->mpMatStuff, "Rename Material");
}

static const Image	*sMakeSmallVColourBox(vec3 colour)
{
	return	sMakeSmallColourBox(Misc_SSE_Vec3ToRGBA(colour));
}

static const Image	*sMakeSmallColourBox(color_t colour)
{
	//create a block image
	uint32_t	block[32 * 32];
	for(int i=0;i < (32 * 32);i++)
	{
		block[i]	=colour;
	}

	pixformat_t	fmt	=ekRGBA32;
	Image	*pBlock	=image_from_pixels(32, 32, fmt, (byte_t *)block, NULL, 0);

	return	pBlock;
}

static const Image	*sCreateTexImage(const UT_string *szTex)
{
	if(szTex == NULL)
	{
		return	NULL;
	}

	UT_string	*szPath;
	utstring_new(szPath);

	utstring_printf(szPath, "Textures/%s.png", utstring_body(szTex));

	uint32_t	w, h;
	int			rowPitch;
	BYTE	**pRows	=SK_LoadTextureBytes(utstring_body(szPath), &rowPitch, &w, &h);

	//put into a contiguous byte array
	uint32_t	*pPix	=malloc(w * h * sizeof(uint32_t));

	for(int y=0;y < h;y++)
	{
		int	ofs	=y * w;
		memcpy(&pPix[ofs], pRows[y], rowPitch);
	}

	pixformat_t	fmt	=ekRGBA32;
	Image	*pBlock	=image_from_pixels(w, h, fmt, (byte_t *)pPix, NULL, 0);

	free(pPix);

	return	pBlock;
}

static int	sGetSelectedMeshPartIndex(AppContext *pApp)
{
	return	sGetSelectedIndex(pApp->mpMeshPartLB);
}

static const char	*sGetSelectedMaterialName(AppContext *pApp)
{
	int	matSelected	=sGetSelectedIndex(pApp->mpMaterialLB);
	if(matSelected == -1)
	{
		return	NULL;
	}

	return	listbox_text(pApp->mpMaterialLB, matSelected);
}

static Material	*sGetSelectedMaterial(AppContext *pApp)
{
	const char	*szMatSel	=sGetSelectedMaterialName(pApp);

	return	MatLib_GetMaterial(pApp->mpMatLib, szMatSel);
}

__attribute_maybe_unused__
static const Material	*sGetSelectedConstMaterial(AppContext *pApp)
{
	const char	*szMatSel	=sGetSelectedMaterialName(pApp);

	return	MatLib_GetConstMaterial(pApp->mpMatLib, szMatSel);
}