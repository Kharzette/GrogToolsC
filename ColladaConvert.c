#include    <nappgui.h>


//this gets passed into events and such
//will likely grow
typedef struct AppContext_t
{
	Window	*mpWnd;
	Layout	*mpLay;

}  AppContext;


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

static AppContext	*sAppCreate(void)
{
	AppContext	*pApp	=heap_new0(AppContext);
	
	gui_language("");
	pApp->mpWnd	=sCreateWindow();

	window_origin(pApp->mpWnd, v2df(500.f, 200.f));
	window_OnClose(pApp->mpWnd, listener(pApp, sOnClose, AppContext));
	window_show(pApp->mpWnd);

	Layout	*pLay	=layout_create(3, 3);
	layout_margin(pLay, 10);

	Button	*pB0	=button_push();
	Button	*pB1	=button_push();
	Button	*pB2	=button_push();
	Button	*pB3	=button_push();
	Button	*pB4	=button_push();
	Button	*pB5	=button_push();
	Button	*pB6	=button_push();
	Button	*pB7	=button_push();
	Button	*pB8	=button_push();

	button_text(pB0, "Blort0");
	button_text(pB1, "Blort1");
	button_text(pB2, "Blort2");
	button_text(pB3, "Blort3");
	button_text(pB4, "Blort4");
	button_text(pB5, "Blort5");
	button_text(pB6, "Blort6");
	button_text(pB7, "Blort7");
	button_text(pB8, "Blort8");

	layout_button(pLay, pB0, 0, 0);
	layout_button(pLay, pB1, 0, 1);
	layout_button(pLay, pB2, 0, 2);
	layout_button(pLay, pB3, 1, 0);
	layout_button(pLay, pB4, 1, 1);
	layout_button(pLay, pB5, 1, 2);
	layout_button(pLay, pB6, 2, 0);
	layout_button(pLay, pB7, 2, 1);
	layout_button(pLay, pB8, 2, 2);

	Panel	*pPanel	=panel_create();

	panel_layout(pPanel, pLay);

	window_panel(pApp->mpWnd, pPanel);

	window_size(pApp->mpWnd, s2df(800, 600));
	
	return	pApp;
}

static void	sAppDestroy(AppContext **ppApp)
{
	window_destroy(&(*ppApp)->mpWnd);

	heap_delete(ppApp, AppContext);
}


#include "osmain.h"
osmain(sAppCreate, sAppDestroy, "", AppContext);