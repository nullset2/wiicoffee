#include <gccore.h>
#include <gctypes.h>
#include <ogcsys.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "libwiigui/gui.h"
#include "menu.h"
#include "demo.h"
#include "input.h"
#include "filelist.h"
#include "filebrowser.h"
#include "httpcalls.h"
#include <json/json.h>

#define THREAD_SLEEP 100

static GuiImageData * pointer[4];
static GuiImage * bgImg = NULL;
static GuiSound * bgMusic = NULL;
static GuiWindow * mainWindow = NULL;
static lwp_t guithread = LWP_THREAD_NULL;
static bool guiHalt = true;

/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
static void
ResumeGui()
{
	guiHalt = false;
	LWP_ResumeThread (guithread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
static void
HaltGui()
{
	guiHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(guithread))
		usleep(THREAD_SLEEP);
}

/****************************************************************************
 * WindowPrompt
 *
 * Displays a prompt window to user, with information, an error message, or
 * presenting a user with a choice
 ***************************************************************************/
int
WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){0, 0, 0, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,40);
	GuiText msgTxt(msg, 22, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetWrap(true, 400);

	GuiText btn1Txt(btn1Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(20, -25);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -25);
	}

	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetSoundOver(&btnSoundOver);
	btn1.SetTrigger(&trigA);
	btn1.SetState(STATE_SELECTED);
	btn1.SetEffectGrow();

	GuiText btn2Txt(btn2Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-20, -25);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetSoundOver(&btnSoundOver);
	btn2.SetTrigger(&trigA);
	btn2.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);

	if(btn2Label)
		promptWindow.Append(&btn2);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	while(choice == -1)
	{
		usleep(THREAD_SLEEP);

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 0;
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(THREAD_SLEEP);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return choice;
}

/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/

static void *
UpdateGUI (void *arg)
{
	int i;

	while(1)
	{
		if(guiHalt)
		{
			LWP_SuspendThread(guithread);
		}
		else
		{
			UpdatePads();
			mainWindow->Draw();

			#ifdef HW_RVL
			for(i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad->ir.valid)
					Menu_DrawImg(userInput[i].wpad->ir.x-48, userInput[i].wpad->ir.y-48,
						96, 96, pointer[i]->GetImage(), userInput[i].wpad->ir.angle, 1, 1, 255);
				DoRumble(i);
			}
			#endif

			Menu_Render();

			for(i=0; i < 4; i++)
				mainWindow->Update(&userInput[i]);

			if(ExitRequested)
			{
				for(i = 0; i <= 255; i += 15)
				{
					mainWindow->Draw();
					Menu_DrawRectangle(0,0,screenwidth,screenheight,(GXColor){0, 0, 0, i},1);
					Menu_Render();
				}
				ExitApp();
			}
		}
	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 *
 * Startup GUI threads
 ***************************************************************************/
void
InitGUIThreads()
{
	LWP_CreateThread (&guithread, UpdateGUI, NULL, NULL, 0, 70);
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, u16 maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText okBtnTxt("OK", 22, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (GXColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetTrigger(&trigA);
	cancelBtn.SetEffectGrow();

	keyboard.Append(&okBtn);
	keyboard.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&keyboard);
	mainWindow->ChangeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	HaltGui();
	mainWindow->Remove(&keyboard);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
}

GXColor HexToGXColor(const std::string& hex) {
	GXColor color = {255, 255, 255, 255}; // default white
	if(hex.length() == 7 && hex[0] == '#') {
		color.r = strtol(hex.substr(1,2).c_str(), nullptr, 16);
		color.g = strtol(hex.substr(3,2).c_str(), nullptr, 16);
		color.b = strtol(hex.substr(5,2).c_str(), nullptr, 16);
	}
	return color;
}

u8* CreateColorImageBuffer(GXColor color, int width, int height) {
	u8* buffer = new u8[width * height * 4]; // 4 bytes per pixel
	for (int i = 0; i < width * height; ++i) {
		buffer[i * 4 + 0] = color.r;
		buffer[i * 4 + 1] = color.g;
		buffer[i * 4 + 2] = color.b;
		buffer[i * 4 + 3] = color.a;
	}
	return buffer;
}

static int MenuProducts() {
	int menu = MENU_NONE;

	GuiText titleTxt("Shop", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50, 50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	std::vector<Product> products = parseProductsJson(http_get("/products"));
	std::vector<GuiButton*> buttons;

	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);

	int i = 20;

	for (auto product : products) {
		GuiText* btnTxt = new GuiText(product.name.c_str(), 15, (GXColor){0, 0, 0, 255});
		btnTxt->SetWrap(true, btnLargeOutline.GetWidth() - 30);
		GuiImage* btnImg = new GuiImage(&btnLargeOutline);
		GuiImage* btnImgOver = new GuiImage(&btnLargeOutlineOver);
		GuiButton* btn = new GuiButton(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
		if(product.tags.find("color") != product.tags.end()) {
			GXColor productColor = HexToGXColor(product.tags["color"]);
			u8* colorBuffer = CreateColorImageBuffer(productColor, btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
			GuiImageData* colorBgData = new GuiImageData(colorBuffer, btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
			GuiImage* colorBgImage = new GuiImage(colorBgData);
			btn->SetImage(colorBgImage);
		}
		btn->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
		btn->SetPosition(i, 120);
		btn->SetLabel(btnTxt);
		btn->SetImageOver(btnImgOver);
		btn->SetSoundOver(&btnSoundOver);
		btn->SetTrigger(&trigA);
		btn->SetEffectGrow();
		buttons.push_back(btn);
		w.Append(btn);
		i = i+20;
	}

	GuiText* backText = new GuiText("Back", 18, (GXColor){0, 0, 0, 255});
	backText->SetWrap(true, btnLargeOutline.GetWidth() - 30);
	GuiImage* backImg = new GuiImage(&btnLargeOutline);
	GuiImage* backImgOver = new GuiImage(&btnLargeOutlineOver);
	GuiButton* backBtn = new GuiButton(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	backBtn->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn->SetPosition(50, -50);
	backBtn->SetLabel(backText);
	backBtn->SetImage(backImg);
	backBtn->SetImageOver(backImgOver);
	backBtn->SetSoundOver(&btnSoundOver);
	backBtn->SetTrigger(&trigA);
	backBtn->SetEffectGrow();
	w.Append(backBtn);

	HaltGui();
	mainWindow->Append(&w);
	ResumeGui();

	while (menu == MENU_NONE) {
		usleep(THREAD_SLEEP);

		if (backBtn->GetState() == STATE_CLICKED) {
			backBtn->ResetState();
			menu = MENU_NONE;
		}
	}

	HaltGui();
	mainWindow->Remove(&w);

	delete backText;
	delete backImg;
	delete backImgOver;
	delete backBtn;

	for (auto btn : buttons) {
		delete btn;
	}

	return menu;
}


/****************************************************************************
 * MenuSettings
 ***************************************************************************/
static int MenuSettings()
{
	int menu = MENU_NONE;

	GuiText titleTxt("Terminal.shop", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText productsBtnTxt("Shop", 18, (GXColor){0, 0, 0, 255});
	productsBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage productsBtnImg(&btnLargeOutline);
	GuiImage productsBtnImgOver(&btnLargeOutlineOver);
	GuiButton productsBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	productsBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	productsBtn.SetPosition(50, 120);
	productsBtn.SetLabel(&productsBtnTxt);
	productsBtn.SetImage(&productsBtnImg);
	productsBtn.SetImageOver(&productsBtnImgOver);
	productsBtn.SetSoundOver(&btnSoundOver);
	productsBtn.SetTrigger(&trigA);
	productsBtn.SetEffectGrow();

	GuiText ordersBtnTxt("Order History", 18, (GXColor){0, 0, 0, 255});
	ordersBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage ordersBtnImg(&btnLargeOutline);
	GuiImage ordersBtnImgOver(&btnLargeOutlineOver);
	GuiButton ordersBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	ordersBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	ordersBtn.SetPosition(-50, 120);
	ordersBtn.SetLabel(&ordersBtnTxt);
	ordersBtn.SetImage(&ordersBtnImg);
	ordersBtn.SetImageOver(&ordersBtnImgOver);
	ordersBtn.SetSoundOver(&btnSoundOver);
	ordersBtn.SetTrigger(&trigA);
	ordersBtn.SetEffectGrow();

	GuiText subscriptionsBtnTxt("Subscriptions", 18, (GXColor){0, 0, 0, 255});
	subscriptionsBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage subscriptionsBtnImg(&btnLargeOutline);
	GuiImage subscriptionsBtnImgOver(&btnLargeOutlineOver);
	GuiButton subscriptionsBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	subscriptionsBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	subscriptionsBtn.SetPosition(-190, 250);
	subscriptionsBtn.SetLabel(&subscriptionsBtnTxt);
	subscriptionsBtn.SetImage(&subscriptionsBtnImg);
	subscriptionsBtn.SetImageOver(&subscriptionsBtnImgOver);
	subscriptionsBtn.SetSoundOver(&btnSoundOver);
	subscriptionsBtn.SetTrigger(&trigA);
	subscriptionsBtn.SetEffectGrow();

	GuiText cartBtnTxt("Cart", 18, (GXColor){0, 0, 0, 255});
	cartBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage cartBtnImg(&btnLargeOutline);
	GuiImage cartBtnImgOver(&btnLargeOutlineOver);
	GuiButton cartBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	cartBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	cartBtn.SetPosition(190, 250);
	cartBtn.SetLabel(&cartBtnTxt);
	cartBtn.SetImage(&cartBtnImg);
	cartBtn.SetImageOver(&cartBtnImgOver);
	cartBtn.SetSoundOver(&btnSoundOver);
	cartBtn.SetTrigger(&trigA);
	cartBtn.SetEffectGrow();

	GuiText exitBtnTxt("Exit", 22, (GXColor){0, 0, 0, 255});
	GuiImage exitBtnImg(&btnOutline);
	GuiImage exitBtnImgOver(&btnOutlineOver);
	GuiButton exitBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	exitBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	exitBtn.SetPosition(100, -35);
	exitBtn.SetLabel(&exitBtnTxt);
	exitBtn.SetImage(&exitBtnImg);
	exitBtn.SetImageOver(&exitBtnImgOver);
	exitBtn.SetSoundOver(&btnSoundOver);
	exitBtn.SetTrigger(&trigA);
	exitBtn.SetTrigger(&trigHome);
	exitBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&productsBtn);
	w.Append(&ordersBtn);
	w.Append(&subscriptionsBtn);
	w.Append(&cartBtn);
	w.Append(&exitBtn);

	mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if (productsBtn.GetState() == STATE_CLICKED)
		{
			productsBtn.ResetState();
			menu = MENU_PRODUCTS;
		}
		else if (exitBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;
		}
	}

	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * MainMenu
 ***************************************************************************/
void MainMenu(int menu)
{
	int currentMenu = menu;

	#ifdef HW_RVL
	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);
	#endif

	mainWindow = new GuiWindow(screenwidth, screenheight);

	bgImg = new GuiImage(screenwidth, screenheight, (GXColor){0, 0, 0, 255});
	mainWindow->Append(bgImg);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	ResumeGui();

	bgMusic = new GuiSound(bg_music_ogg, bg_music_ogg_size, SOUND_OGG);
	bgMusic->SetVolume(50);
	bgMusic->Play(); // startup music

	while(currentMenu != MENU_EXIT)
	{
		switch (currentMenu)
		{
			case MENU_NONE:
				currentMenu = MenuSettings();
				break;
			case MENU_PRODUCTS:
				currentMenu = MenuProducts();
				break;
			default: // unrecognized menu
				currentMenu = MenuSettings();
				break;
		}
	}

	ResumeGui();
	ExitRequested = 1;
	while(1) usleep(THREAD_SLEEP);

	HaltGui();

	bgMusic->Stop();
	delete bgMusic;
	delete bgImg;
	delete mainWindow;

	delete pointer[0];
	delete pointer[1];
	delete pointer[2];
	delete pointer[3];

	mainWindow = NULL;
}
