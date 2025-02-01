#include    <nappgui.h>
#include	<d3d11_1.h>
#include	"UtilityLib/GraphicsDevice.h"
#include	"UtilityLib/GameCamera.h"
#include	"UtilityLib/MiscStuff.h"
#include	"UtilityLib/ListStuff.h"
#include	"UtilityLib/StringStuff.h"
#include	"UtilityLib/DictionaryStuff.h"
#include	"UtilityLib/UserSettings.h"
#include	"UtilityLib/Mover.h"
#include	"MaterialLib/StuffKeeper.h"
#include	"MaterialLib/Material.h"
#include	"MaterialLib/MaterialLib.h"
#include	"MaterialLib/CBKeeper.h"
#include	"MaterialLib/PostProcess.h"
#include	"MaterialLib/UIStuff.h"
#include	"MeshLib/AnimLib.h"
#include	"MeshLib/Mesh.h"
#include	"MeshLib/Character.h"
#include	"MeshLib/Skeleton.h"
#include	"MeshLib/GSNode.h"
#include	"MeshLib/Static.h"
#include	"MeshLib/CommonPrims.h"
#include	"InputLib/Input.h"


#define	RESX			1280
#define	RESY			720
#define	TIC_RATE		(1.0f / 60.0f)
#define	ROT_RATE		10.0f
#define	KEYTURN_RATE	0.01f
#define	MOVE_RATE		0.1f
#define	MOUSE_TO_ANG	0.001f
#define	POW_SLIDER_MAX	100
#define	MAX_UI_VERTS	(8192 * 2)

//clay defines
#define COLOR_ORANGE		(Clay_Color) {225, 138, 50, 255}
#define COLOR_BLUE			(Clay_Color) {111, 173, 162, 255}
#define COLOR_GOLD			(Clay_Color) {255, 222, 162, 255}
#define	BONE_VERT_SIZE		30
#define	COLLAPSE_INTERVAL	(0.5f)

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

//this gets passed into events and such
//will likely grow
typedef struct AppContext_t
{
	//user settings
	UserSettings	*mpUS;

	//gui windows
	Window	*mpWnd;
	Window	*mpMatWnd;
	Window	*mpTexWnd;	//pops up a texture choice
	Window	*mpEditWnd;	//for editing text

	//window pos assgoblinry
	bool	mbPosDiffTaken;
	vec2	mPosDiff;

	//D3D stuff
	ID3D11RasterizerState	*mp3DRast;

	//grogstuff
	GraphicsDevice	*mpGD;
	GameCamera		*mpCam;
	StuffKeeper		*mpSK;
	CBKeeper		*mpCBK;
	PostProcess		*mpPP;
	Input			*mpInp;
	UIStuff			*mpUI;		//clay ui

	//loaded data
	AnimLib		*mpALib;
	Character	*mpChar;
	Static		*mpStatic;
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
	bool	mbLeftMouseDown;

	//clay pointer stuff
	Clay_Vector2	mScrollDelta;
	Clay_Vector2	mMousePos;

	//skelly editor data
	BoneDisplayData	*mpBDD;
	bool			mbSEVisible;	//user wants to see skelly editor
	bool			mbSEAnimating;	//stuff collapsing / growing

	//skelly popout movers
	Mover	*mpSEM;
	Mover	*mpBoneCollapse;

	//projection matrices
	mat4	mCamProj, mTextProj;

	//nappgui stuff
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

	//list of anims loaded
	StringList	*mpAnimList;

}  AppContext;

//function pointer types
typedef void	(*ReNameFunc)(void *, const char *, const char *);

//static forward decs
static void				SetupKeyBinds(Input *pInp);
static void				SetupRastVP(AppContext *pApp);
static const Image		*sMakeSmallVColourBox(vec3 colour);
static const Image		*sMakeSmallColourBox(color_t colour);
static int				sGetSelectedMeshPartIndex(AppContext *pApp);
static const char		*sGetSelectedMaterialName(AppContext *pApp);
static const Material	*sGetSelectedConstMaterial(AppContext *pApp);
static Material			*sGetSelectedMaterial(AppContext *pApp);
static void				sUpdateSelectedMaterial(AppContext *pApp);
static const Image		*sCreateTexImage(const UT_string *szTex);
static bool				sSelectPopupItem(PopUp *pPop, const char *pSZ);
static int 				sGetSelectedIndex(const ListBox *pLB);
static void				sDeleteListBoxItem(ListBox *pLB, int idx);
static void				sSetDefaultCel(AppContext *pApp);
static uint32_t			sSpawnReName(AppContext *pApp, V2Df pos, const char *szOld,
							ListBox *pLB, void *pItem, ReNameFunc reName);
//clay stuff
static const Clay_RenderCommandArray sCreateLayout(AppContext *pApp);
static void sHandleClayErrors(Clay_ErrorData errorData);
static void sSetNodesAnimatingOff(BoneDisplayData *pBDD);

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
static void MouseWheelEH(void *pContext, const SDL_Event *pEvt);
static void EscEH(void *pContext, const SDL_Event *pEvt);
static void CollapseBonesEH(void *pContext, const SDL_Event *pEvt);
static void MarkUnusedBonesEH(void *pContext, const SDL_Event *pEvt);
static void DeleteBonesEH(void *pContext, const SDL_Event *pEvt);
static void	SkelPopOutEH(void *pContext, const SDL_Event *pEvt);

//button event handlers
static void sLoadCharacter(AppContext *pAC, Event *pEvt);
static void sSaveCharacter(AppContext *pAC, Event *pEvt);
static void sLoadStatic(AppContext *pAC, Event *pEvt);
static void sSaveStatic(AppContext *pAC, Event *pEvt);
static void sLoadMaterialLib(AppContext *pAC, Event *pEvt);
static void sSaveMaterialLib(AppContext *pAC, Event *pEvt);
static void sLoadAnimLib(AppContext *pAC, Event *pEvt);
static void sSaveAnimLib(AppContext *pAC, Event *pEvt);
static void sAssignMaterial(AppContext *pAC, Event *pEvt);
static void sShaderFileChanged(AppContext *pAC, Event *pEvt);
static void sShaderChanged(AppContext *pAC, Event *pEvt);
static void sSPowChanged(AppContext *pAC, Event *pEvt);
static void	sColourButtonClicked(AppContext *pAC, Event *pEvt);
static void sMatSelectionChanged(AppContext *pAC, Event *pEvt);
static void sMeshSelectionChanged(AppContext *pAC, Event *pEvt);
static void	sSRV0Clicked(AppContext *pAC, Event *pEvt);
static void	sSRV1Clicked(AppContext *pAC, Event *pEvt);
static void	sTexChosen(AppContext *pAC, Event *pEvt);
static void	sDoMatStuff(AppContext *pAC, Event *pEvt);		//contextual
static void	sOnHotKeyReName(AppContext *pAC, Event *pEvt);	//F2
static void	sOnHotKeyDelete(AppContext *pAC, Event *pEvt);	//F2


static void	sSaveWindowPositions(AppContext *pApp)
{
	V2Df	pos	=window_get_origin(pApp->mpWnd);
	UserSettings_AddPosition(pApp->mpUS, "MainWindow", pos.x, pos.y);

	pos	=window_get_origin(pApp->mpMatWnd);
	UserSettings_AddPosition(pApp->mpUS, "MaterialWindow", pos.x, pos.y);

	vec2	goblinPos;
	goblinPos[0]	=GD_GetPosX(pApp->mpGD);
	goblinPos[1]	=GD_GetPosY(pApp->mpGD);

	glm_vec2_sub(goblinPos, pApp->mPosDiff, goblinPos);

	UserSettings_AddPosition(pApp->mpUS, "3DWindow", goblinPos[0], goblinPos[1]);

	UserSettings_Save(pApp->mpUS);
}

static Window	*sCreateWindow(void)
{
	Window	*pWnd	=window_create(ekWINDOW_STD | ekWINDOW_ESC | ekWINDOW_RETURN | ekWINDOW_RESIZE);
	
	window_title(pWnd, "Collada Conversion Tool");

	printf("sCreateWindow\n");
	
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

	vec2	mwPos;
	UserSettings_GetPosition(pApp->mpUS, "MaterialWindow", mwPos);

	//eventually want this to tag alongside the main window
	window_origin(pApp->mpMatWnd, v2df(mwPos[0], mwPos[1]));

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

	//SRV Labels
	Label	*pSL0	=label_create();
	Label	*pSL1	=label_create();
	label_text(pSL0, "SRV0->");
	label_text(pSL1, "SRV1->");

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
	layout_label(pLay, pSL0, 2, 2);
	layout_label(pLay, pSL1, 2, 3);

	//right aligned labels
	layout_halign(pLay, 2, 1, ekRIGHT);
	layout_halign(pLay, 2, 2, ekRIGHT);
	layout_halign(pLay, 2, 3, ekRIGHT);

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
	printf("sAppCreate\n");
	AppContext	*pApp	=heap_new0(AppContext);

	pApp->mpUS	=UserSettings_Create();
	UserSettings_Load(pApp->mpUS);
	
	gui_language("");
	pApp->mpWnd		=sCreateWindow();
	pApp->mpEditWnd	=window_create(ekWINDOW_ESC | ekWINDOW_RETURN | ekWINDOW_RESIZE);

	vec2	mwPos;
	UserSettings_GetPosition(pApp->mpUS, "MainWindow", mwPos);

	window_origin(pApp->mpWnd, v2df(mwPos[0], mwPos[1]));
	window_OnClose(pApp->mpWnd, listener(pApp, sOnClose, AppContext));
	window_show(pApp->mpWnd);

	//set up F2 and del.  Del is SUPR!?
	window_hotkey(pApp->mpWnd, ekKEY_F2, 0, listener(pApp, sOnHotKeyReName, AppContext));
	window_hotkey(pApp->mpWnd, ekKEY_SUPR, 0, listener(pApp, sOnHotKeyDelete, AppContext));

	Layout	*pLay		=layout_create(3, 3);
	Layout	*pEditLay	=layout_create(1, 1);
	layout_margin(pLay, 10);

	//sublayouts for save / load buttons
	Layout	*pSavLoadMeshLay	=layout_create(2, 2);
	Layout	*pSavLoadMatLay		=layout_create(1, 2);
	Layout	*pSavLoadAnimLay	=layout_create(1, 2);

	Button	*pLChar		=button_push();
	Button	*pSChar		=button_push();
	Button	*pLMat		=button_push();
	Button	*pSMat		=button_push();
	Button	*pLAnim		=button_push();
	Button	*pSAnim		=button_push();
	Button	*pLStat		=button_push();
	Button	*pSStat		=button_push();
	Button	*pAssMat	=button_push();
	pApp->mpMatStuff	=button_push();

	button_text(pLChar, "Load Character");
	button_text(pSChar, "Save Character");
	button_text(pLStat, "Load Static");
	button_text(pSStat, "Save Static");
	button_text(pLMat, "Load MatLib");
	button_text(pSMat, "Save MatLib");
	button_text(pLAnim, "Load AnimLib");
	button_text(pSAnim, "Save AnimLib");
	button_text(pAssMat, "<- Assign Material <-");
	button_text(pApp->mpMatStuff, "New Material");

	pApp->mpMeshPartLB	=listbox_create();
	pApp->mpMaterialLB	=listbox_create();
	pApp->mpAnimLB		=listbox_create();

	pApp->mpTextInput	=edit_create();
	edit_autoselect(pApp->mpTextInput, true);

	layout_layout(pLay, pSavLoadMeshLay, 0, 0);
	layout_layout(pLay, pSavLoadMatLay, 1, 0);
	layout_layout(pLay, pSavLoadAnimLay, 2, 0);

	//put the buttons within sublayouts
	layout_button(pSavLoadMeshLay, pLChar, 0, 0);
	layout_button(pSavLoadMeshLay, pSChar, 0, 1);
	layout_button(pSavLoadMeshLay, pLStat, 1, 0);
	layout_button(pSavLoadMeshLay, pSStat, 1, 1);

	layout_button(pSavLoadMatLay, pLMat, 0, 0);
	layout_button(pSavLoadMatLay, pSMat, 0, 1);

	layout_button(pSavLoadAnimLay, pLAnim, 0, 0);
	layout_button(pSavLoadAnimLay, pSAnim, 0, 1);

	//center these buttons
	layout_halign(pSavLoadMeshLay, 0, 0, ekCENTER);
	layout_halign(pSavLoadMeshLay, 0, 1, ekCENTER);
	layout_halign(pSavLoadMeshLay, 1, 0, ekCENTER);
	layout_halign(pSavLoadMeshLay, 1, 1, ekCENTER);
	layout_halign(pSavLoadMatLay, 0, 0, ekCENTER);
	layout_halign(pSavLoadMatLay, 0, 1, ekCENTER);
	layout_halign(pSavLoadAnimLay, 0, 0, ekCENTER);
	layout_halign(pSavLoadAnimLay, 0, 1, ekCENTER);

	layout_listbox(pLay, pApp->mpMeshPartLB, 0, 1);
	layout_button(pLay, pAssMat, 1, 1);
	layout_listbox(pLay, pApp->mpMaterialLB, 2, 1);
	layout_listbox(pLay, pApp->mpAnimLB, 0, 2);
	layout_button(pLay, pApp->mpMatStuff, 2, 2);

	layout_edit(pEditLay, pApp->mpTextInput, 0, 0);

	button_OnClick(pLChar, listener(pApp, sLoadCharacter, AppContext));
	button_OnClick(pSChar, listener(pApp, sSaveCharacter, AppContext));
	button_OnClick(pLStat, listener(pApp, sLoadStatic, AppContext));
	button_OnClick(pSStat, listener(pApp, sSaveStatic, AppContext));
	button_OnClick(pLMat, listener(pApp, sLoadMaterialLib, AppContext));
	button_OnClick(pSMat, listener(pApp, sSaveMaterialLib, AppContext));
	button_OnClick(pLAnim, listener(pApp, sLoadAnimLib, AppContext));
	button_OnClick(pSAnim, listener(pApp, sSaveAnimLib, AppContext));
	button_OnClick(pAssMat, listener(pApp, sAssignMaterial, AppContext));
	listbox_OnSelect(pApp->mpMaterialLB, listener(pApp, sMatSelectionChanged, AppContext));
	listbox_OnSelect(pApp->mpMeshPartLB, listener(pApp, sMeshSelectionChanged, AppContext));
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
	pApp->mpStatic	=NULL;

	//movers
	pApp->mpSEM				=Mover_Create();
	pApp->mpBoneCollapse	=Mover_Create();

	//input and key / mouse bindings
	pApp->mpInp	=INP_CreateInput();
	SetupKeyBinds(pApp->mpInp);

	//SDL is a pain in the ass
	pApp->mbPosDiffTaken	=false;

	//Get position gives the client topleft
	//Create takes a position that is the topleft of the full window
	//so if you save and restore with w title bar, it bumps itself
	//down every launch.
	vec2	SDLWinPos;
	UserSettings_GetPosition(pApp->mpUS, "3DWindow", SDLWinPos);

	GD_Init(&pApp->mpGD, "Collada Tool",
		SDLWinPos[0], SDLWinPos[1], RESX, RESY, false,
		D3D_FEATURE_LEVEL_11_0);

	//turn on border
	GD_SetWindowBordered(pApp->mpGD, true);

	SetupRastVP(pApp);

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

	//default cel shading
	sSetDefaultCel(pApp);
	
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
	GameCam_GetProjection(pApp->mpCam, pApp->mCamProj);
	CBK_SetProjection(pApp->mpCBK, pApp->mCamProj);

	//2d projection for text
	glm_ortho(0, RESX, RESY, 0, -1.0f, 1.0f, pApp->mTextProj);

	//set constant buffers to shaders, think I just have to do this once
	CBK_SetCommonCBToShaders(pApp->mpCBK, pApp->mpGD);

	pApp->mLightDir[0]		=0.3f;
	pApp->mLightDir[1]		=-0.7f;
	pApp->mLightDir[2]		=-0.5f;

	glm_vec3_normalize(pApp->mLightDir);

	pApp->mpUI	=UI_Create(pApp->mpGD, pApp->mpSK, MAX_UI_VERTS);

	UI_AddFont(pApp->mpUI, "MeiryoUI26", 0);

	//clay init
    uint64_t totalMemorySize = Clay_MinMemorySize();
    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(clayMemory, (Clay_Dimensions) { (float)RESX, (float)RESY }, (Clay_ErrorHandler) { sHandleClayErrors });
    Clay_SetMeasureTextFunction(UI_MeasureText, (uintptr_t)pApp->mpUI);

	Clay_SetDebugModeEnabled(true);

	pApp->mbRunning	=true;

	return	pApp;
}

static void	sAppDestroy(AppContext **ppApp)
{
	AppContext	*pAC	=*ppApp;

	printf("sAppDestroy\n");
	sSaveWindowPositions(pAC);

	GD_Destroy(&pAC->mpGD);

	window_destroy(&pAC->mpWnd);

	//nuke bone display data
	BoneDisplayData	*pCur, *pTmp;
	HASH_ITER(hh, pAC->mpBDD, pCur, pTmp)
	{
		HASH_DEL(pAC->mpBDD, pCur);
		free(pCur);
	}

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
		int	selected	=sGetSelectedIndex(pApp->mpAnimLB);
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

	//camera update
	GameCam_UpdateRotation(pApp->mpCam, pApp->mEyePos, pApp->mDeltaPitch,
							pApp->mDeltaYaw, 0.0f);

	//Set proj and rast and depth for 3D
	CBK_SetProjection(pApp->mpCBK, pApp->mCamProj);
	GD_RSSetState(pApp->mpGD, pApp->mp3DRast);
	GD_OMSetDepthStencilState(pApp->mpGD, StuffKeeper_GetDepthStencilState(pApp->mpSK, "EnableDepth"));

	//set no blend, I think post processing turns it on maybe
	GD_OMSetBlendState(pApp->mpGD, StuffKeeper_GetBlendState(pApp->mpSK, "NoBlending"));
	GD_PSSetSampler(pApp->mpGD, StuffKeeper_GetSamplerState(pApp->mpSK, "PointWrap"), 0);

	//set CB view
	{
		mat4	viewMat;
		GameCam_GetViewMatrixFly(pApp->mpCam, viewMat, pApp->mEyePos);

		CBK_SetView(pApp->mpCBK, viewMat, pApp->mEyePos);
	}

	PP_SetTargets(pApp->mpPP, pApp->mpGD, "BackColor", "BackDepth");
	PP_ClearDepth(pApp->mpPP, pApp->mpGD, "BackDepth");
	PP_ClearTarget(pApp->mpPP, pApp->mpGD, "BackColor");

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
	if(pApp->mpChar != NULL)
	{
		if(pApp->mpMeshes != NULL && pApp->mpALib != NULL && pApp->mpMatLib != NULL)
		{
			Character_Draw(pApp->mpChar, pApp->mpMeshes, pApp->mpMatLib,
							pApp->mpALib, pApp->mpGD, pApp->mpCBK);
		}
	}
	else if(pApp->mpStatic != NULL)
	{
		if(pApp->mpMeshes != NULL && pApp->mpMatLib != NULL)
		{
			Static_Draw(pApp->mpStatic, pApp->mpMeshes, pApp->mpMatLib,
							pApp->mpGD, pApp->mpCBK);
		}
	}

	//set proj for 2D
	CBK_SetProjection(pApp->mpCBK, pApp->mTextProj);
	CBK_UpdateFrame(pApp->mpCBK, pApp->mpGD);

	Clay_UpdateScrollContainers(true, pApp->mScrollDelta, cTime);

	pApp->mScrollDelta.x	=pApp->mScrollDelta.y	=0.0f;

    Clay_RenderCommandArray renderCommands = sCreateLayout(pApp);

	UI_BeginDraw(pApp->mpUI);

	UI_ClayRender(pApp->mpUI, renderCommands);

	UI_EndDraw(pApp->mpUI);

	GD_Present(pApp->mpGD);

	//have to do this window position difference after
	//present has run 
	if(!pApp->mbPosDiffTaken)
	{
		vec2	SDLWinPos;
		UserSettings_GetPosition(pApp->mpUS, "3DWindow", SDLWinPos);

		//store the difference
		vec2	goblinPos;
		goblinPos[0]	=GD_GetPosX(pApp->mpGD);
		goblinPos[1]	=GD_GetPosY(pApp->mpGD);

		glm_vec2_sub(goblinPos, SDLWinPos, pApp->mPosDiff);

		pApp->mbPosDiffTaken	=true;
	}
}

static void sAppUpdate(AppContext *pApp, const real64_t prTime, const real64_t cTime)
{
	if(pApp == NULL)
	{
		//init not finished
		printf("Init not finished\n");
		return;
	}
	
	if(!pApp->mbRunning)
	{
		osapp_finish();
	}

	Mover_Update(pApp->mpSEM, TIC_RATE);
	Mover_Update(pApp->mpBoneCollapse, TIC_RATE);

	if(Mover_IsDone(pApp->mpBoneCollapse))
	{
		pApp->mbSEAnimating	=false;
		sSetNodesAnimatingOff(pApp->mpBDD);
	}

	pApp->mAnimTime		=cTime;
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
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_l, RandLightEH);		//randomize light dir
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_c, CollapseBonesEH);	//collapse/uncollapse selected bones
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_m, MarkUnusedBonesEH);	//mark bones that aren't used in vert weights
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_DELETE, DeleteBonesEH);	//nuke selected bones
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_p, SkelPopOutEH);	//nuke selected bones
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_ESCAPE, EscEH);

	//held bindings
	//movement
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_w, KeyMoveForwardEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_a, KeyMoveLeftEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_s, KeyMoveBackEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_d, KeyMoveRightEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_SPACE, KeyMoveUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_z, KeyMoveDownEH);

	//key turning
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_q, KeyTurnLeftEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_e, KeyTurnRightEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_r, KeyTurnUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_t, KeyTurnDownEH);

	//move data events
	INP_MakeBinding(pInp, INP_BIND_TYPE_MOVE, SDL_MOUSEMOTION, MouseMoveEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_MOVE, SDL_MOUSEWHEEL, MouseWheelEH);

	//down/up events
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDL_BUTTON_RIGHT, RightMouseDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDL_BUTTON_RIGHT, RightMouseUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDL_BUTTON_LEFT, LeftMouseDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDL_BUTTON_LEFT, LeftMouseUpEH);
}

static void	SetupRastVP(AppContext *pApp)
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

	pApp->mp3DRast	=GD_CreateRasterizerState(pApp->mpGD, &rastDesc);

	D3D11_VIEWPORT	vp;

	vp.Width	=RESX;
	vp.Height	=RESY;
	vp.MaxDepth	=1.0f;
	vp.MinDepth	=0.0f;
	vp.TopLeftX	=0;
	vp.TopLeftY	=0;

	GD_RSSetViewPort(pApp->mpGD, &vp);
	GD_RSSetState(pApp->mpGD, pApp->mp3DRast);
	GD_IASetPrimitiveTopology(pApp->mpGD, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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

	pTS->mbLeftMouseDown	=true;

	if(!pTS->mbMouseLooking)
	{
		Clay_SetPointerState(pTS->mMousePos, true);
	}
}

static void	LeftMouseUpEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	pTS->mbLeftMouseDown	=false;

	if(!pTS->mbMouseLooking)
	{
		Clay_SetPointerState(pTS->mMousePos, false);
	}
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
	else
	{
		pTS->mMousePos.x	=pEvt->motion.x;
		pTS->mMousePos.y	=pEvt->motion.y;

		Clay_SetPointerState(pTS->mMousePos, pTS->mbLeftMouseDown);
	}
}

static void	MouseWheelEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	pTS->mScrollDelta.x	=pEvt->wheel.x;
	pTS->mScrollDelta.y	=pEvt->wheel.y;
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

	pTS->mDeltaYaw	-=KEYTURN_RATE;
}

static void	KeyTurnRightEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	pTS->mDeltaYaw	+=KEYTURN_RATE;
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

static void CollapseBonesEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	if(pTS->mbSEAnimating)
	{
		return;
	}

	BoneDisplayData	*pBDD;

	//toggle collapse on selected and deselect
	for(pBDD=pTS->mpBDD;pBDD != NULL;pBDD=pBDD->hh.next)
	{
		if(pBDD->mbSelected)
		{
			pBDD->mbSelected	=false;
			pBDD->mbCollapsed	=!pBDD->mbCollapsed;
			pBDD->mbAnimating	=true;
		}
	}

	Mover_SetUpMove(pTS->mpBoneCollapse,
		(vec4){BONE_VERT_SIZE,0,0,0}, (vec4){0,0,0,0},
		COLLAPSE_INTERVAL, 0.2f, 0.2f);

	pTS->mbSEAnimating	=true;
}

static void MarkUnusedBonesEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);
}

static void DeleteBonesEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);
}

static void SkelPopOutEH(void *pContext, const SDL_Event *pEvt)
{
	AppContext	*pTS	=(AppContext *)pContext;

	assert(pTS);

	vec4	startPos	={0};
	vec4	endPos		={0};

	endPos[0]	=300;

	vec4	mvPos;
	Mover_GetPos(pTS->mpSEM, mvPos);

	if(mvPos[0] > 0.0f)
	{
		if(pTS->mbSEVisible)
		{
			//closing from partway open
			Mover_SetUpMove(pTS->mpSEM, mvPos, startPos, 0.25f, 0.2f, 0.2f);
		}
		else
		{
			//opening from partway closed
			Mover_SetUpMove(pTS->mpSEM, mvPos, endPos, 0.25f, 0.2f, 0.2f);
		}
	}
	else
	{
		if(!pTS->mbSEVisible)
		{
			Mover_SetUpMove(pTS->mpSEM, startPos, endPos, 0.5f, 0.2f, 0.2f);
		}
	}

	pTS->mbSEVisible	=!pTS->mbSEVisible;
}


static void sLoadCharacter(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"Character", "character"	};

	const char	*pFileName	=comwin_open_file(pAC->mpWnd, fTypes, 2, NULL);
	if(pFileName == NULL)
	{
		printf("Empty filename for Load.\n");
		return;
	}

	printf("Character load fileName: %s\n", pFileName);

	if(pAC->mpChar != NULL)
	{
		Character_Destroy(pAC->mpChar);
	}

	pAC->mpChar	=Character_Read(pFileName);

	printf("Character loaded...\n");

	StringList	*pParts	=Character_GetPartList(pAC->mpChar);

	DictSZ_New(&pAC->mpMeshes);

	UT_string	*szPath	=SZ_StripFileName(pFileName);

	UT_string	*szFullPath;
	utstring_new(szFullPath);

	const StringList	*pCur	=SZList_Iterate(pParts);
	while(pCur != NULL)
	{
		utstring_printf(szFullPath, "%s/%s.mesh", utstring_body(szPath), SZList_IteratorVal(pCur));

		Mesh	*pMesh	=Mesh_Read(pAC->mpGD, pAC->mpSK, utstring_body(szFullPath), true);

		DictSZ_Add(&pAC->mpMeshes, SZList_IteratorValUT(pCur), pMesh);

		listbox_add_elem(pAC->mpMeshPartLB, SZList_IteratorVal(pCur), NULL);

		utstring_clear(szFullPath);

		pCur	=SZList_IteratorNext(pCur);
	}

	SZList_Clear(&pParts);
}

static void	sSaveMeshParts(const UT_string *pKey, const void *pValue, void *pContext)
{
	Mesh	*pMesh	=(Mesh *)pValue;
	if(pMesh == NULL)
	{
		printf("Bad mesh for %s\n", utstring_body(pKey));
		return;
	}

	printf("Saving mesh %s\n", utstring_body(pKey));

	UT_string	*szPath	=(UT_string *)pContext;

	UT_string	*szFullPath;
	utstring_new(szFullPath);

	utstring_printf(szFullPath, "%s/%s.mesh",
		utstring_body(szPath), utstring_body(pKey));

	Mesh_Write(pMesh, utstring_body(szFullPath));

	utstring_done(szFullPath);
}

static void sSaveCharacter(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"Character", "character"	};

	const char	*pFileName	=comwin_save_file(pAC->mpWnd, fTypes, 2, NULL);
	if(pFileName == NULL)
	{
		printf("Empty filename for Save.\n");
		return;
	}

	UT_string	*szFileName;
	utstring_new(szFileName);

	UT_string	*szExt	=SZ_GetExtension(pFileName);
	if(szExt == NULL)
	{
		utstring_printf(szFileName, "%s.Character", pFileName);
	}
	else
	{
		utstring_printf(szFileName, "%s", pFileName);
		utstring_done(szExt);
	}

	printf("Character save fileName: %s\n", utstring_body(szFileName));

	Character_Write(pAC->mpChar, utstring_body(szFileName));

	printf("Character saved...\n");

	UT_string	*szPath	=SZ_StripFileName(pFileName);

	//save mesh parts
	DictSZ_ForEach(pAC->mpMeshes, sSaveMeshParts, szPath);

	utstring_done(szPath);
}

static void sLoadStatic(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"Static", "static"	};

	const char	*pFileName	=comwin_open_file(pAC->mpWnd, fTypes, 2, NULL);
	if(pFileName == NULL)
	{
		printf("Empty filename for Load.\n");
		return;
	}

	printf("Static load fileName: %s\n", pFileName);

	if(pAC->mpStatic != NULL)
	{
		Static_Destroy(pAC->mpStatic);
	}

	pAC->mpStatic	=Static_Read(pFileName);

	printf("Static loaded...\n");

	StringList	*pParts	=Static_GetPartList(pAC->mpStatic);

	DictSZ_New(&pAC->mpMeshes);

	UT_string	*szPath	=SZ_StripFileName(pFileName);

	UT_string	*szFullPath;
	utstring_new(szFullPath);

	const StringList	*pCur	=SZList_Iterate(pParts);
	while(pCur != NULL)
	{
		utstring_printf(szFullPath, "%s/%s.mesh", utstring_body(szPath), SZList_IteratorVal(pCur));

		Mesh	*pMesh	=Mesh_Read(pAC->mpGD, pAC->mpSK, utstring_body(szFullPath), true);

		DictSZ_Add(&pAC->mpMeshes, SZList_IteratorValUT(pCur), pMesh);

		listbox_add_elem(pAC->mpMeshPartLB, SZList_IteratorVal(pCur), NULL);

		utstring_clear(szFullPath);

		pCur	=SZList_IteratorNext(pCur);
	}

	SZList_Clear(&pParts);
}

static void sSaveStatic(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"Static", "static"	};

	const char	*pFileName	=comwin_save_file(pAC->mpWnd, fTypes, 2, NULL);
	if(pFileName == NULL)
	{
		printf("Empty filename for Save.\n");
		return;
	}

	UT_string	*szFileName;
	utstring_new(szFileName);

	UT_string	*szExt	=SZ_GetExtension(pFileName);
	if(szExt == NULL)
	{
		utstring_printf(szFileName, "%s.Static", pFileName);
	}
	else
	{
		utstring_printf(szFileName, "%s", pFileName);
		utstring_done(szExt);
	}

	printf("Static save fileName: %s\n", utstring_body(szFileName));

	Static_Write(pAC->mpStatic, utstring_body(szFileName));

	printf("Static saved...\n");

	UT_string	*szPath	=SZ_StripFileName(pFileName);

	//save mesh parts
	DictSZ_ForEach(pAC->mpMeshes, sSaveMeshParts, szPath);

	utstring_done(szPath);
}

static void sLoadMaterialLib(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"MatLib", "Matlib", "matlib"	};

	const char	*pFileName	=comwin_open_file(pAC->mpWnd, fTypes, 3, NULL);
	if(pFileName == NULL)
	{
		printf("Empty filename for Load.\n");
		return;
	}

	printf("MaterialLib load fileName: %s\n", pFileName);

	pAC->mpMatLib	=MatLib_Read(pFileName, pAC->mpSK);

	StringList	*pMats	=MatLib_GetMatList(pAC->mpMatLib);

	printf("Material lib loaded...\n");

	const StringList	*pCur	=SZList_Iterate(pMats);

	while(pCur != NULL)
	{
		printf("\t%s\n", SZList_IteratorVal(pCur));

		listbox_add_elem(pAC->mpMaterialLB, SZList_IteratorVal(pCur), NULL);

		pCur	=SZList_IteratorNext(pCur);
	}

	SZList_Clear(&pMats);
}

static void sSaveMaterialLib(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"MatLib", "Matlib", "matlib"	};

	const char	*pFileName	=comwin_save_file(pAC->mpWnd, fTypes, 3, NULL);
	if(pFileName == NULL)
	{
		printf("Empty filename for Save.\n");
		return;
	}

	UT_string	*szFileName;
	utstring_new(szFileName);

	UT_string	*szExt	=SZ_GetExtension(pFileName);
	if(szExt == NULL)
	{
		utstring_printf(szFileName, "%s.MatLib", pFileName);
	}
	else
	{
		utstring_printf(szFileName, "%s", pFileName);
		utstring_done(szExt);
	}

	printf("MaterialLib save fileName: %s\n", utstring_body(szFileName));

	MatLib_Write(pAC->mpMatLib, utstring_body(szFileName));

	printf("Material lib saved...\n");
}

static void sLoadAnimLib(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"AnimLib", "Animlib", "animlib"	};

	const char	*pFileName	=comwin_open_file(pAC->mpWnd, fTypes, 3, NULL);
	if(pFileName == NULL)
	{
		printf("Empty filename for Load.\n");
		return;
	}

	printf("AnimLib load fileName: %s\n", pFileName);

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

static void sSaveAnimLib(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*fTypes[]	={	"AnimLib", "Animlib", "animlib"	};

	const char	*pFileName	=comwin_save_file(pAC->mpWnd, fTypes, 3, NULL);
	if(pFileName == NULL)
	{
		printf("Empty filename for Save.\n");
		return;
	}

	UT_string	*szFileName;
	utstring_new(szFileName);

	UT_string	*szExt	=SZ_GetExtension(pFileName);
	if(szExt == NULL)
	{
		utstring_printf(szFileName, "%s.AnimLib", pFileName);
	}
	else
	{
		utstring_printf(szFileName, "%s", pFileName);
		utstring_done(szExt);
	}

	printf("AnimLib save fileName: %s\n", utstring_body(szFileName));

	AnimLib_Write(pAC->mpALib, utstring_body(szFileName));

	printf("Animation library saved...\n");
}

static void sAssignMaterial(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	const char	*szMatSel	=sGetSelectedMaterialName(pAC);
	int	meshSelected		=sGetSelectedMeshPartIndex(pAC);

	if(pAC->mpChar != NULL)
	{
		Character_AssignMaterial(pAC->mpChar, meshSelected, szMatSel);
	}
	else
	{
		Static_AssignMaterial(pAC->mpStatic, meshSelected, szMatSel);
	}

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
		
		//uint32_t	ret	=
		sSpawnReName(pAC, goodPos, szMatSel,
			pAC->mpMaterialLB, pAC->mpMatLib, (ReNameFunc)MatLib_ReName);
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
		sSelectPopupItem(pLapp->mpTexList, utstring_body(pSRVName));
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

	V2Df	matWindowPos	=window_get_origin(pAC->mpMatWnd);

	comwin_color(pAC->mpWnd, "Choose Colour", matWindowPos.x, matWindowPos.y, ekRIGHT, ekTOP, kCOLOR_WHITE, NULL, 0, listener(pAC, sColourChosen, AppContext));

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

static bool	sSelectPopupItem(PopUp *pPop, const char *pSZ)
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

static bool	sSelectListBoxItem(ListBox *pLB, const char *pSZ)
{
	int	cnt	=listbox_count(pLB);

	for(int i=0;i < cnt;i++)
	{
		const char	*pItemTxt	=listbox_text(pLB, i);

		int	result	=strcmp(pItemTxt, pSZ);
		if(result == 0)
		{
			listbox_select(pLB, i, true);
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
		sSelectPopupItem(pApp->mpVSPop, utstring_body(pVSName));
	}

	if(pPS != NULL)
	{
		const UT_string	*pPSName	=StuffKeeper_GetPSName(pApp->mpSK, pPS);
		sSelectPopupItem(pApp->mpPSPop, utstring_body(pPSName));
	}
}

static void sOnHotKeyReName(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	GuiControl	*pItem	=window_get_focus(pAC->mpWnd);

	printf("Rename: tag %d\n", guicontrol_get_tag(pItem));

	//rename only really does stuff on listboxen
	ListBox	*pLB	=guicontrol_listbox(pItem);
	if(pLB == NULL)
	{
		return;
	}
	int	seld	=sGetSelectedIndex(pLB);

	printf("Rename ListBox Item: %d\n", seld);

	if(seld == -1)
	{
		return;
	}
	const char	*szOld	=listbox_text(pLB, seld);

	V2Df	pos		=window_get_origin(pAC->mpWnd);
	S2Df	size	=window_get_client_size(pAC->mpWnd);

	V2Df	goodPos	=pos;

	if(pLB == pAC->mpAnimLB)
	{
		goodPos.y	+=size.height * 0.75f;
		uint32_t	result	=sSpawnReName(pAC, goodPos,
			szOld, pLB, pAC->mpALib, (ReNameFunc)AnimLib_ReName);
		printf("AnimBox %d\n", result);
	}
	else if(pLB == pAC->mpMaterialLB)
	{
		goodPos.x	+=size.width * 0.75f;
		goodPos.y	+=size.height / 4;
		uint32_t	result	=sSpawnReName(pAC, goodPos,
			szOld, pLB, pAC->mpMatLib, (ReNameFunc)MatLib_ReName);
		printf("MatBox %d\n", result);
	}
	else if(pLB == pAC->mpMeshPartLB)
	{
		goodPos.y	+=size.height / 4;

		//also rename in meshes
		Mesh	*pMesh	=NULL;
		if(DictSZ_ContainsKeyccp(pAC->mpMeshes, szOld))
		{
			pMesh	=DictSZ_GetValueccp(pAC->mpMeshes, szOld);
			DictSZ_Removeccp(&pAC->mpMeshes, listbox_text(pLB, seld));
		}

		uint32_t	result;

		if(pAC->mpChar != NULL)
		{
			result	=sSpawnReName(pAC, goodPos,
				listbox_text(pLB, seld), pLB, pAC->mpChar, (ReNameFunc)Character_ReNamePart);
		}
		else if(pAC->mpStatic != NULL)
		{
			result	=sSpawnReName(pAC, goodPos,
				listbox_text(pLB, seld), pLB, pAC->mpStatic, (ReNameFunc)Static_ReNamePart);
		}

		if(pMesh != NULL)
		{
			DictSZ_Addccp(&pAC->mpMeshes, edit_get_text(pAC->mpTextInput), pMesh);
			Mesh_SetName(pMesh, edit_get_text(pAC->mpTextInput));
		}
		printf("MeshBox %d\n", result);
	}
}

static void sOnHotKeyDelete(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	GuiControl	*pItem	=window_get_focus(pAC->mpWnd);

	printf("delete: tag %d\n", guicontrol_get_tag(pItem));

	//delete only really does stuff on listboxen
	ListBox	*pLB	=guicontrol_listbox(pItem);
	if(pLB == NULL)
	{
		return;
	}
	int	seld	=sGetSelectedIndex(pLB);

	printf("Delete ListBox Item: %d\n", seld);

	if(seld == -1)
	{
		return;
	}
	const char	*szItem	=listbox_text(pLB, seld);

	if(pLB == pAC->mpAnimLB)
	{
		AnimLib_Delete(pAC->mpALib, szItem);
		SZList_Remove(&pAC->mpAnimList, szItem);
	}
	else if(pLB == pAC->mpMaterialLB)
	{
		Material	*pMat	=MatLib_GetMaterial(pAC->mpMatLib, szItem);
		MatLib_Remove(pAC->mpMatLib, szItem);
		free(pMat);
	}
	else if(pLB == pAC->mpMeshPartLB)
	{
		Character_DeletePart(pAC->mpChar, szItem);

		//also nuke in meshes
		Mesh	*pMesh	=NULL;
		if(DictSZ_ContainsKeyccp(pAC->mpMeshes, szItem))
		{
			pMesh	=DictSZ_GetValueccp(pAC->mpMeshes, szItem);
			DictSZ_Removeccp(&pAC->mpMeshes, listbox_text(pLB, seld));
			Mesh_Destroy(pMesh);
		}
	}

	sDeleteListBoxItem(pLB, seld);
}

static void sDeleteListBoxItem(ListBox *pLB, int idx)
{
	int	count	=listbox_count(pLB);

	//copy contents
	UT_string	*pContents[count];
	for(int i=0;i < count;i++)
	{
		utstring_new(pContents[i]);
		utstring_printf(pContents[i], "%s", listbox_text(pLB, i));
	}

	//clear lb
	listbox_clear(pLB);

	for(int i=0;i < count;i++)
	{
		if(i == idx)
		{
			continue;
		}
		listbox_add_elem(pLB, utstring_body(pContents[i]), NULL);
	}

	for(int i=0;i < count;i++)
	{
		utstring_done(pContents[i]);
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

static void sMeshSelectionChanged(AppContext *pAC, Event *pEvt)
{
	unref(pEvt);

	int	seld	=sGetSelectedMeshPartIndex(pAC);	
	if(seld == -1)
	{
		return;
	}

	const char	*szMesh	=listbox_text(pAC->mpMeshPartLB, seld);

	if(pAC->mpChar != NULL)
	{
		const char	*szMat	=Character_GetMaterialForPart(pAC->mpChar, szMesh);
		if(sSelectListBoxItem(pAC->mpMaterialLB, szMat))
		{
			sMatSelectionChanged(pAC, NULL);
		}
	}
	else if(pAC->mpStatic != NULL)
	{
		const char	*szMat	=Static_GetMaterialForPart(pAC->mpStatic, szMesh);
		if(sSelectListBoxItem(pAC->mpMaterialLB, szMat))
		{
			sMatSelectionChanged(pAC, NULL);
		}
	}
	else
	{
		return;
	}
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

	if(szMatSel == NULL)
	{
		return	NULL;
	}

	return	MatLib_GetMaterial(pApp->mpMatLib, szMatSel);
}

__attribute_maybe_unused__
static const Material	*sGetSelectedConstMaterial(AppContext *pApp)
{
	const char	*szMatSel	=sGetSelectedMaterialName(pApp);

	return	MatLib_GetConstMaterial(pApp->mpMatLib, szMatSel);
}

static uint32_t	sSpawnReName(
	AppContext *pApp, V2Df pos, const char *szOld,
	ListBox *pLB, void *pItem, ReNameFunc reName)
{
	window_origin(pApp->mpEditWnd, pos);

	//set the text to the material's current name
	edit_text(pApp->mpTextInput, szOld);
	
	uint32_t	ret	=window_modal(pApp->mpEditWnd, pApp->mpWnd);
	if(ret != ekGUI_CLOSE_ESC)
	{
		//rename the actual item
		reName(pItem, szOld, edit_get_text(pApp->mpTextInput));

		//change the text in the listbox too
		int	seld	=sGetSelectedIndex(pLB);
		if(seld != -1)
		{
			listbox_set_elem(pLB, seld, edit_get_text(pApp->mpTextInput), NULL);
		}
	}
	return	ret;
}

static void	sSetDefaultCel(AppContext *pApp)
{
	float	mins[4]	={	0.0f, 0.3f, 0.6f, 1.0f	};
	float	maxs[4]	={	0.3f, 0.6f, 1.0f, 5.0f	};
	float	snap[4]	={	0.3f, 0.5f, 0.9f, 1.4f	};

	CBK_SetCelSteps(pApp->mpCBK, mins, maxs, snap, 4);

	CBK_UpdateCel(pApp->mpCBK, pApp->mpGD);
}


static void sOnHoverBone(Clay_ElementId eID, Clay_PointerData pnt, intptr_t userData)
{
	//clicked?
	if(pnt.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME)
	{
		printf("Click! %s\n", eID.stringId.chars);

		AppContext	*pApp	=(AppContext *)userData;
		if(pApp == NULL)
		{
			return;
		}
		if(pApp->mpALib == NULL)
		{
			return;
		}

		const Skeleton	*pSkel	=AnimLib_GetSkeleton(pApp->mpALib);
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
		HASH_FIND_PTR(pApp->mpBDD, &pBone, pBDD);
		if(pBDD == NULL)
		{
			pBDD	=malloc(sizeof(BoneDisplayData));
			memset(pBDD, 0, sizeof(BoneDisplayData));

			pBDD->mpNode	=pBone;

			HASH_ADD_PTR(pApp->mpBDD, mpNode, pBDD);
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

#define	NOT_COLLAPSING	0
#define	GROWING			1
#define	COLLAPSING		2

//dive thru the bone tree to make clay stuffs
static void sSkeletonLayout(const GSNode *pNode, AppContext *pAC, int colState)
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
	CLAY_LAYOUT({ .childGap = 4, .padding = { 16, 0, 0, 0},
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
					.sizing = { .width = CLAY_SIZING_FIT(0),
						.height = CLAY_SIZING_FIT(0) }}),
		CLAY_RECTANGLE({ .color = {10, 221, 25, 45} });
	Clay__ElementPostConfiguration();

	//see if selected
	bool	bSelected	=sIsSelected(pAC->mpBDD, pNode);

	//see if collapsed, if so recurse no further and add a +
	bool	bCollapsed	=sIsCollapsed(pAC->mpBDD, pNode);

	//see if animating (opening or closing)
	bool	bAnimating	=sIsAnimating(pAC->mpBDD, pNode);

	//see if bone has any children
	bool	bHasKids	=(pNode->mNumChildren > 0);

	vec4	colAmount;
	Mover_GetPos(pAC->mpBoneCollapse, colAmount);

	Clay_Sizing	cs	={0};

	cs.width	=CLAY_SIZING_FIT(0);

	if(colState == NOT_COLLAPSING)
	{
		cs.height	=CLAY_SIZING_FIT(0);
	}
	else if(colState == GROWING)
	{
		cs.height	=CLAY_SIZING_FIXED(BONE_VERT_SIZE - colAmount[0]);
	}
	else
	{
		cs.height	=CLAY_SIZING_FIXED(colAmount[0]);
	}

	//create an inner rect sized for the text
	Clay__OpenElement();
	Clay__AttachId(Clay__HashString(csNode, 0, 0));
	CLAY_LAYOUT({ .childGap = 4, .padding = { 8, 8, 2, 2 },	.sizing = cs}),
		Clay_OnHover(sOnHoverBone, (intptr_t)pAC),		
		CLAY_RECTANGLE({ .cornerRadius = {6},
			//selected, hovered, or normal?
			.color = bSelected? COLOR_GOLD : (Clay_Hovered()? COLOR_ORANGE : COLOR_BLUE) });
	
	int	childColState	=colState;
	if(bHasKids)
	{
		if(bCollapsed)
		{
			CLAY_TEXT(CLAY_STRING("+"), CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 0, 0, 255} }));
			if(bAnimating)
			{
				childColState	=COLLAPSING;
			}
		}
		else
		{
			CLAY_TEXT(CLAY_STRING("-"), CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 0, 0, 255} }));
			if(bAnimating)
			{
				childColState	=GROWING;
			}
		}
	}
	CLAY_TEXT(csNode, CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 0, 0, 255} })),

	Clay__ElementPostConfiguration();

//	printf("CloseElement: %s\n", csNode.chars);

	//this closes the inner text nubbins
	Clay__CloseElement();

	//see if collapsed, if so recurse no further
	if(bCollapsed && !bAnimating)
	{
		//don't recurse (already closed nodes)
	}
	else if(!bCollapsed || pAC->mbSEAnimating)
	{
		//children should parent off the big rect
		for(int i=0;i < pNode->mNumChildren;i++)
		{
			sSkeletonLayout(pNode->mpChildren[i], pAC, childColState);
		}
	}

	//close big outer rect
	Clay__CloseElement();
}


//for in render window UI
static Clay_RenderCommandArray sCreateLayout(AppContext *pApp)
{
	Clay_BeginLayout();

	
/*	CLAY(CLAY_ID("SkelPopoutButton"),
		CLAY_LAYOUT({ .sizing = { .width = CLAY_SIZING_FIXED(10), .height = CLAY_SIZING_FIXED(30) }, .padding = { 4, 4, 4, 4 }}),
		CLAY_FLOATING({ .zIndex = 1, .attachment = { CLAY_ATTACH_POINT_CENTER_TOP, CLAY_ATTACH_POINT_CENTER_TOP }, .offset = {0, 0} }),
		CLAY_BORDER_OUTSIDE({ .color = {80, 80, 80, 255}, .width = 2 }),
		CLAY_RECTANGLE({ .color = {140,80, 200, 200 }}))
		{
			CLAY_TEXT(CLAY_STRING("I'm an inline floating container."), CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255,255,255,255} }));
		}*/
	vec4	mvPos, bcPos;
	Mover_GetPos(pApp->mpSEM, mvPos);
	Mover_GetPos(pApp->mpBoneCollapse, bcPos);

	if(mvPos[0] > 0.0f)
	{
		Clay__OpenElement();

		CLAY_ID("SideBar");

		CLAY_LAYOUT({ .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP},
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
						.sizing = { .width = CLAY_SIZING_FIXED(mvPos[0]),
						.height = CLAY_SIZING_GROW(0) },
						.padding = {16, 16, 16, 16 },
						.childGap = 16 });
		CLAY_RECTANGLE({ .color = {150, 150, 155, 55} });
		CLAY_SCROLL({ .horizontal = true, .vertical = true });

		Clay__ElementPostConfiguration();

		if(pApp->mpALib != NULL)
		{
			const Skeleton	*pSkel	=AnimLib_GetSkeleton(pApp->mpALib);
			if(pSkel != NULL)
			{
				for(int i=0;i < pSkel->mNumRoots;i++)
				{
					sSkeletonLayout(pSkel->mpRoots[i], pApp, NOT_COLLAPSING);
				}
			}
		}

		Clay__CloseElement();
	}

	return Clay_EndLayout();
}

static bool reinitializeClay = false;

static void sHandleClayErrors(Clay_ErrorData errorData) {
    printf("%s", errorData.errorText.chars);
    if (errorData.errorType == CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED) {
        reinitializeClay = true;
        Clay_SetMaxElementCount(Clay_GetMaxElementCount() * 2);
    } else if (errorData.errorType == CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED) {
        reinitializeClay = true;
        Clay_SetMaxMeasureTextCacheWordCount(Clay_GetMaxMeasureTextCacheWordCount() * 2);
    }
}
