/**
 * @file menupage.c
 * @brief The menu's pages, and where the game and editor are launched.
 *
 * Implements @ref menupage.h. Each page is a function that clears the buttons,
 * builds the new set, and starts a camera transition to that page's viewpoint.
 *
 * This is the branch point of the whole program: the button callbacks here are
 * what call @ref setMapFilePath followed by @c changeState(&gameState), or
 * @ref setEditorMapFilePath followed by @c changeState(&editorState). Every
 * other part of the menu is presentation.
 *
 * The level lists are read off the filesystem and rendered onto the in-scene
 * terminal as a @ref screenList_struct - see @ref menuscene.h.
 */

#include "menu/menu_main.h"

#include <dirent.h>



/**  Structures that represents a menu button. */
typedef struct
{
	/** Displayed title of button */
	const char* string;
	/** Function to be executed when button is pressed */
	buttonTargetFunction targetFunction;
}menuButton_struct;


//extern u8 logoAlpha;


static void setupMenuPage(menuButton_struct* mp, u8 n);

static void mainMenuCreditsButtonFunction(sguiButton_struct* b);
static void creditsMenuBackButtonFunction(sguiButton_struct* b);

static void mainMenuOptionsButtonFunction(sguiButton_struct* b);
static void optionsMenuUpButtonFunction(sguiButton_struct* b);
static void optionsMenuDownButtonFunction(sguiButton_struct* b);
static void optionsMenuLessButtonFunction(sguiButton_struct* b);
static void optionsMenuMoreButtonFunction(sguiButton_struct* b);
static void optionsMenuBackButtonFunction(sguiButton_struct* b);

static void startMenuPlayButtonFunction(sguiButton_struct* b);
static void mainMenuCreateButtonFunction(sguiButton_struct* b);
static void mainMenuPlayButtonFunction(sguiButton_struct* b);
static void playMenuCampaignButtonFunction(sguiButton_struct* b);
static void playMenuLoadLevelButtonFunction(sguiButton_struct* b);
static void playMenuBackButtonFunction(sguiButton_struct* b);
static void createMenuBackButtonFunction(sguiButton_struct* b);
static void createMenuNewLevelButtonFunction(sguiButton_struct* b);
static void createMenuLoadLevelButtonFunction(sguiButton_struct* b);
static void newLevelMenuOKButtonFunction(sguiButton_struct* b);
static void newLevelMenuBackButtonFunction(sguiButton_struct* b);
static void selectLevelMenuUpButtonFunction(sguiButton_struct* b);
static void selectLevelMenuDownButtonFunction(sguiButton_struct* b);
static void selectLevelMenuOKButtonFunction(sguiButton_struct* b);
static void selectLevelMenuBackButtonFunction(sguiButton_struct* b);
static void loadLevelMenuUpButtonFunction(sguiButton_struct* b);
static void loadLevelMenuDownButtonFunction(sguiButton_struct* b);
static void loadLevelMenuOKButtonFunction(sguiButton_struct* b);
static void loadLevelMenuBackButtonFunction(sguiButton_struct* b);


static void freeFileList(char** list, int length);
static int listFiles(char* path, char** list);

static bool creditsShown=false;
static bool optionsShown=false;

static char **testList=NULL;
static int testListCnt, testListCnt1;
static screenList_struct testScreenList;

static menuButton_struct startMenuPage[]={(menuButton_struct){"START", (buttonTargetFunction)startMenuPlayButtonFunction}};
static u8 startMenuPageLength=arrayLength(startMenuPage);
//setupMenuPage() stacks these from the bottom of the screen upwards, so index
//zero is the lowest button. Credits therefore goes first to sit under Options.
static menuButton_struct mainMenuPage[]={(menuButton_struct){"Credits", (buttonTargetFunction)mainMenuCreditsButtonFunction}, (menuButton_struct){"Options", (buttonTargetFunction)mainMenuOptionsButtonFunction}, (menuButton_struct){"Create", (buttonTargetFunction)mainMenuCreateButtonFunction}, (menuButton_struct){"Play", (buttonTargetFunction)mainMenuPlayButtonFunction}};
static u8 mainMenuPageLength=arrayLength(mainMenuPage);
static menuButton_struct creditsMenuPage[]={(menuButton_struct){"Back", (buttonTargetFunction)creditsMenuBackButtonFunction}};
static u8 creditsMenuPageLength=arrayLength(creditsMenuPage);
//Up and Down move the cursor over the settings, Less and More change the one it
//is on - the same shape as the level lists' Up/Down/OK, which is the only
//multiple-choice idiom this GUI has.
static menuButton_struct optionsMenuPage[]={(menuButton_struct){"Back", (buttonTargetFunction)optionsMenuBackButtonFunction}, (menuButton_struct){"More", (buttonTargetFunction)optionsMenuMoreButtonFunction}, (menuButton_struct){"Less", (buttonTargetFunction)optionsMenuLessButtonFunction}, (menuButton_struct){"Down", (buttonTargetFunction)optionsMenuDownButtonFunction}, (menuButton_struct){"Up", (buttonTargetFunction)optionsMenuUpButtonFunction}};
static u8 optionsMenuPageLength=arrayLength(optionsMenuPage);
static menuButton_struct playMenuPage[]={(menuButton_struct){"Back", (buttonTargetFunction)playMenuBackButtonFunction}, (menuButton_struct){"Select Level", playMenuLoadLevelButtonFunction}, (menuButton_struct){"Campaign", playMenuCampaignButtonFunction}};
static u8 playMenuPageLength=arrayLength(playMenuPage);
static menuButton_struct createMenuPage[]={(menuButton_struct){"Back", (buttonTargetFunction)createMenuBackButtonFunction}, (menuButton_struct){"Load Level", createMenuLoadLevelButtonFunction}, (menuButton_struct){"New level", (buttonTargetFunction)createMenuNewLevelButtonFunction}};
static u8 createMenuPageLength=arrayLength(createMenuPage);
static menuButton_struct newLevelMenuPage[]={(menuButton_struct){"Back", (buttonTargetFunction)newLevelMenuBackButtonFunction}, (menuButton_struct){"OK", (buttonTargetFunction)newLevelMenuOKButtonFunction}};
static u8 newLevelMenuPageLength=arrayLength(newLevelMenuPage);
static menuButton_struct selectLevelMenuPage[]={(menuButton_struct){"Back", (buttonTargetFunction)selectLevelMenuBackButtonFunction}, (menuButton_struct){"OK", (buttonTargetFunction)selectLevelMenuOKButtonFunction}, (menuButton_struct){"Down", (buttonTargetFunction)selectLevelMenuDownButtonFunction}, (menuButton_struct){"Up", (buttonTargetFunction)selectLevelMenuUpButtonFunction}};
static u8 selectLevelMenuPageLength=arrayLength(selectLevelMenuPage);
static menuButton_struct loadLevelMenuPage[]={(menuButton_struct){"Back", (buttonTargetFunction)loadLevelMenuBackButtonFunction}, (menuButton_struct){"OK", (buttonTargetFunction)loadLevelMenuOKButtonFunction}, (menuButton_struct){"Down", (buttonTargetFunction)loadLevelMenuDownButtonFunction}, (menuButton_struct){"Up", (buttonTargetFunction)loadLevelMenuUpButtonFunction}};
static u8 loadLevelMenuPageLength=arrayLength(loadLevelMenuPage);

void initMenuButtons(void)
{
	initSimpleGui();
}

void setupHomeMenuPage(void)
{
	setupMenuPage(startMenuPage, startMenuPageLength);
}

static void setupMenuPage(menuButton_struct* mp, u8 n)
{
	if(!mp || !n)return;

	//Any page change leaves the credits and the options, so the flags are
	//cleared here rather than in every callback that could navigate away from
	//them.
	creditsShown=false;
	optionsShown=false;

	cleanUpSimpleButtons();

	int i;
	for(i=0;i<n;i++)
	{
		createSimpleButton(vect(0,192-(i+1)*16,0), mp[i].string, mp[i].targetFunction);
	}
}



static void startMenuPlayButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[4],&cameraStates[0],48);
	setupMenuPage(mainMenuPage, mainMenuPageLength);
	/* vanish logo */
	logoAlpha=0;
}



static void mainMenuPlayButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[0],&cameraStates[1],48);
	setupMenuPage(playMenuPage, playMenuPageLength);
}

/*
 * The credits screen.
 *
 * The lines are drawn on whichever screen is not showing the buttons, so the
 * text gets a whole screen and the Back button stays reachable underneath.
 * That screen otherwise shows the logo, which drawMenuCredits() stands in for
 * while the page is up - see menuFrame().
 *
 * At this size a character is eight pixels wide, so a line has room for about
 * thirty of them. Keeping to that is the only constraint.
 */
typedef struct
{
	const char* text;
	bool centered; /**< false left aligns it, which is what keeps columns in line. */
}creditsLine_struct;

static const creditsLine_struct creditsLines[]={
	{"p o r t a l D S", true},
	{"", true},
	{"a homebrew Portal for the DS", true},
	{"", true},
	{"code            smealum", false},
	{"graphics        Lobo", false},
	{"", true},
	{"based on Portal, by Valve", true},
	{"", true},
	{"md2 and pcx     David HENRY", false},
	{"ini parser      N. Devillard", false},
	{"rle codec       GRIT", false},
	{"toolchain       BlocksDS", false},
};
#define CREDITSLINES arrayLength(creditsLines)

#define CREDITSTOP (28)      /**< Pixels from the top of the screen to the first line. */
#define CREDITSSPACING (12)  /**< Pixels between one line and the next. */
#define CREDITSMARGIN (16)   /**< Left edge of the two column rows; centres the widest of them. */

/**
 * @brief Draws one line of the credits.
 *
 * Centring every line independently would put each two column row at its own
 * x, so neither the labels nor the values would line up with the row above.
 * The rows are left aligned at a shared margin instead, and only the headings
 * are centred - which is the same arithmetic game.c uses for level titles: a
 * character is scale*8 pixels wide, so half a line is scale*4 of them.
 */
static void drawCreditsLine(const creditsLine_struct* line, int32 scale, int y)
{
	const int l=strlen(line->text);
	if(!l)return;

	const int32 x=line->centered?(inttof32(128)-(l*scale)*4):inttof32(CREDITSMARGIN);

	drawString((char*)line->text, RGB15(31,31,31), scale, x, inttof32(y));
}

void drawMenuCredits(void)
{
	if(!creditsShown)return;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
		glLoadIdentity();
		glOrthof32(inttof32(0), inttof32(255), inttof32(191), inttof32(0), -inttof32(1), inttof32(1));

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
			glLoadIdentity();

			unsigned i;
			for(i=0;i<CREDITSLINES;i++)
			{
				//the first line is the title, drawn larger
				const int32 scale=i?inttof32(1):(inttof32(3)/2);
				drawCreditsLine(&creditsLines[i], scale, CREDITSTOP+i*CREDITSSPACING);
			}

		glPopMatrix(1);
		glMatrixMode(GL_PROJECTION);
	glPopMatrix(1);
}

static void mainMenuCreditsButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	setupMenuPage(creditsMenuPage, creditsMenuPageLength);
	creditsShown=true; //after setupMenuPage, which clears it
}

static void creditsMenuBackButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	setupMenuPage(mainMenuPage, mainMenuPageLength);
}

/*
 * The options screen.
 *
 * Laid out like the credits and for the same reason: the settings are drawn on
 * whichever screen is not showing the buttons, so the list gets a whole screen
 * and Up, Down, Less, More and Back all stay reachable underneath.
 *
 * One row per setting, the selected one in yellow. Labels are left aligned and
 * values right aligned against the opposite margin, so the values line up in a
 * column of their own whatever the labels do - the same reasoning as
 * drawCreditsLine(), by the same arithmetic.
 *
 * The settings themselves live in settings.h; this page only moves them within
 * the ranges declared there and writes the result out on the way back.
 */
typedef enum
{
	OPTION_SENSITIVITY,
	OPTION_INVERTLOOK,
	OPTION_VOLUME,
	OPTION_BRIGHTNESS,
	OPTION_NUMBER
}menuOption_type;

static const char* optionNames[OPTION_NUMBER]={"Sensitivity", "Invert look", "Volume", "Brightness"};

static u8 optionsCursor=0;

#define OPTIONSTITLE (20)   /**< Pixels from the top of the screen to the title. */
#define OPTIONSTOP (56)     /**< Pixels from the top of the screen to the first row. */
#define OPTIONSSPACING (16) /**< Pixels between one row and the next. */
#define OPTIONSMARGIN (24)  /**< Distance of the labels and the values from their edges. */

/** @brief Writes one setting's value as the player reads it. */
static void optionValueString(menuOption_type o, char* out, int n)
{
	switch(o)
	{
		case OPTION_SENSITIVITY: snprintf(out, n, "%d%%", settings.lookSensitivity); break;
		case OPTION_INVERTLOOK: snprintf(out, n, "%s", settings.invertLookY?"on":"off"); break;
		case OPTION_VOLUME: snprintf(out, n, "%d%%", settings.sfxVolume); break;
		case OPTION_BRIGHTNESS: snprintf(out, n, "%d", settings.brightness); break;
		default: out[0]='\0'; break;
	}
}

/** Formatted value strings for the rows, refreshed only when a value changes -
 *  snprintf is by far the most expensive thing on this page, so it must not
 *  run per row per frame. */
static char optionValueCache[OPTION_NUMBER][16];
static u8 optionValueLen[OPTION_NUMBER];
static bool optionValuesDirty=true;

static void refreshOptionValues(void)
{
	int i;
	for(i=0;i<OPTION_NUMBER;i++)
	{
		optionValueString(i, optionValueCache[i], sizeof(optionValueCache[i]));
		optionValueLen[i]=strlen(optionValueCache[i]);
	}
	optionValuesDirty=false;
}

/**
 * @brief Moves one setting by one step.
 * @param o setting to change.
 * @param direction -1 for Less, 1 for More.
 *
 * Every case clamps rather than wraps: a player holding More to reach the top
 * of a range should stop there, not come back round at the bottom.
 */
static void stepOption(menuOption_type o, int direction)
{
	switch(o)
	{
		case OPTION_SENSITIVITY:
		{
			const int v=settings.lookSensitivity+direction*SETTINGS_SENSITIVITY_STEP;
			settings.lookSensitivity=max(SETTINGS_SENSITIVITY_MIN, min(SETTINGS_SENSITIVITY_MAX, v));
			break;
		}
		case OPTION_INVERTLOOK:
			//Two states, so both buttons do the same thing to it.
			settings.invertLookY=!settings.invertLookY;
			break;
		case OPTION_VOLUME:
		{
			const int v=settings.sfxVolume+direction*SETTINGS_VOLUME_STEP;
			settings.sfxVolume=max(SETTINGS_VOLUME_MIN, min(SETTINGS_VOLUME_MAX, v));
			break;
		}
		case OPTION_BRIGHTNESS:
		{
			const int v=settings.brightness+direction*SETTINGS_BRIGHTNESS_STEP;
			settings.brightness=max(SETTINGS_BRIGHTNESS_MIN, min(SETTINGS_BRIGHTNESS_MAX, v));
			//Applied at the next vblank rather than here - see settings.h.
			requestBrightnessUpdate();
			break;
		}
		default: break;
	}
	optionValuesDirty=true;
}

/** @brief Draws one row: label at the left margin, value at the right one. */
static void drawOptionRow(menuOption_type o, int y)
{
	//Yellow marks the row Less and More act on. There is no cursor sprite to
	//draw, and with four rows on a screen colour is enough to find it by.
	const u16 color=(o==optionsCursor)?RGB15(31,31,0):RGB15(31,31,31);

	drawString((char*)optionNames[o], color, inttof32(1), inttof32(OPTIONSMARGIN), inttof32(y));
	drawString(optionValueCache[o], color, inttof32(1),
		inttof32(256-OPTIONSMARGIN-(int)optionValueLen[o]*8), inttof32(y));
}

void drawMenuOptions(void)
{
	if(!optionsShown)return;

	if(optionValuesDirty)refreshOptionValues();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
		glLoadIdentity();
		glOrthof32(inttof32(0), inttof32(255), inttof32(191), inttof32(0), -inttof32(1), inttof32(1));

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
			glLoadIdentity();

			//Centred, by the same half-a-line-is-scale*4-per-character
			//arithmetic drawCreditsLine() uses.
			const int32 titleScale=inttof32(3)/2;
			drawString("OPTIONS", RGB15(31,31,31), titleScale,
				inttof32(128)-(7*titleScale)*4, inttof32(OPTIONSTITLE));

			int i;
			for(i=0;i<OPTION_NUMBER;i++)drawOptionRow(i, OPTIONSTOP+i*OPTIONSSPACING);

			//Worth saying while the page is up rather than after Back has been
			//pressed and the page is gone: without a card there is nowhere for
			//any of this to be written.
			if(!settingsCanBeSaved())
			{
				drawString("no card - changes are not saved", RGB15(31,16,16), inttof32(1),
					inttof32(0), inttof32(OPTIONSTOP+OPTION_NUMBER*OPTIONSSPACING+16));
			}

		glPopMatrix(1);
		glMatrixMode(GL_PROJECTION);
	glPopMatrix(1);
}

static void mainMenuOptionsButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	setupMenuPage(optionsMenuPage, optionsMenuPageLength);
	optionsShown=true; //after setupMenuPage, which clears it
	optionsCursor=0;
	optionValuesDirty=true; //the settings may have changed since last shown
}

static void optionsMenuUpButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	if(optionsCursor)optionsCursor--;
}

static void optionsMenuDownButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	if(optionsCursor+1<OPTION_NUMBER)optionsCursor++;
}

static void optionsMenuLessButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	stepOption(optionsCursor, -1);
}

static void optionsMenuMoreButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	stepOption(optionsCursor, 1);
}

static void optionsMenuBackButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	//Leaving the page is what saves. Writing on every tap of Less or More would
	//be a card write per press, and this is the only way off the page.
	saveSettings();

	setupMenuPage(mainMenuPage, mainMenuPageLength);
}

static void mainMenuCreateButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[0],&cameraStates[2],64);
	setupMenuPage(createMenuPage, createMenuPageLength);
}

static void playMenuCampaignButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	setMapFilePath("maps/test01.map");
	changeState(&gameState);
}

static void playMenuLoadLevelButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[1],&cameraStates[3],64);
	setupMenuPage(selectLevelMenuPage, selectLevelMenuPageLength);

	testListCnt=0;
	testListCnt1=0;

	testListCnt1=testListCnt+=listFiles("./maps", NULL);
	#ifndef FATONLY
		char str[255];
		sprintf(str,"%s/%s/maps",basePath,ROOT);
		testListCnt+=listFiles(str, NULL);
	#endif

	testList=malloc(sizeof(char*)*testListCnt);

	listFiles("./maps", testList);
	#ifndef FATONLY
		listFiles(str, &testList[testListCnt1]);
	#endif

	initScreenList(&testScreenList, "Select level", testList, testListCnt);
	updateScreenList(&testScreenList);
}

static void playMenuBackButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[1],&cameraStates[0],48);
	setupMenuPage(mainMenuPage, mainMenuPageLength);
}


static void createMenuBackButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[2],&cameraStates[0],64);
	setupMenuPage(mainMenuPage, mainMenuPageLength);
}

static void createMenuNewLevelButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[2],&cameraStates[3],64);
	setupMenuPage(newLevelMenuPage, newLevelMenuPageLength);

	resetSceneScreen();
	sprintf(menuScreenText[0],"Level name :");
	sprintf(menuScreenText[1],"  ");

	setupKeyboard(&menuScreenText[1][2], 10, 16, 16);
}

static void createMenuLoadLevelButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[2],&cameraStates[3],64);
	setupMenuPage(loadLevelMenuPage, loadLevelMenuPageLength);

	testListCnt=0;
	testListCnt1=0;

	char str[255];
	sprintf(str,"%s/%s/maps",basePath,ROOT);
	testListCnt1=testListCnt+=listFiles(str, NULL);

	testList=malloc(sizeof(char*)*testListCnt);

	listFiles(str, testList);

	initScreenList(&testScreenList, "Load level", testList, testListCnt);
	updateScreenList(&testScreenList);
}

static void newLevelMenuOKButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	static char str[2048];
	sprintf(str,"%s/%s/maps/%s.map",basePath,ROOT,&menuScreenText[1][2]);

	setEditorMapFilePath(str);
	changeState(&editorState);
}

static void newLevelMenuBackButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[3],&cameraStates[2],64);
	setupMenuPage(createMenuPage, createMenuPageLength);
}


static void selectLevelMenuUpButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	screenListMove(&testScreenList, -1);
	updateScreenList(&testScreenList);
}

static void selectLevelMenuDownButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	screenListMove(&testScreenList, 1);
	updateScreenList(&testScreenList);
}

static void selectLevelMenuOKButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	static char str[2048];
	if(testScreenList.cursor<testListCnt1)sprintf(str,"./maps/%s",testScreenList.list[testScreenList.cursor]);
	else sprintf(str,"%s/%s/maps/%s",basePath,ROOT,testScreenList.list[testScreenList.cursor]);

	setMapFilePath(str);
	changeState(&gameState);
}

static void selectLevelMenuBackButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[3],&cameraStates[1],64);
	setupMenuPage(playMenuPage, playMenuPageLength);
	freeFileList(testList, testListCnt);
	testList=NULL;
}


static void loadLevelMenuUpButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	screenListMove(&testScreenList, -1);
	updateScreenList(&testScreenList);
}

static void loadLevelMenuDownButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	screenListMove(&testScreenList, 1);
	updateScreenList(&testScreenList);
}

static void loadLevelMenuOKButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	static char str[2048];
	sprintf(str,"%s/%s/maps/%s",basePath,ROOT,testScreenList.list[testScreenList.cursor]);

	setEditorMapFilePath(str);
	changeState(&editorState);
}

static void loadLevelMenuBackButtonFunction(__attribute__((unused)) sguiButton_struct* b)
{
	testTransition=startCameraTransition(&cameraStates[3],&cameraStates[2],64);
	setupMenuPage(createMenuPage, createMenuPageLength);
	freeFileList(testList, testListCnt);
	testList=NULL;
}



static void freeFileList(char** list, int length)
{
	if(!list)return;

	int i;
	for(i=0;i<length;i++)
	{
		if(list[i]){free(list[i]);list[i]=NULL;}
	}
	free(list);
}

static int listFiles(char* path, char** list)
{
	if(!path)return 0;

	char currentPath[255];
	getcwd(currentPath,255);

	chdir(path);

	struct dirent *ent;
	struct stat st;
	DIR* dir=opendir(".");

	int cnt=0;
	while((ent=readdir(dir)))
	{
		stat(ent->d_name,&st);
		if(!S_ISDIR(st.st_mode) && strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
		{
			//dirty .map filter
			int l=strlen(ent->d_name);
			if(l>4 && ent->d_name[l-1]=='p' && ent->d_name[l-2]=='a' && ent->d_name[l-3]=='m' && ent->d_name[l-4]=='.')
			{
				if(list)
				{
					list[cnt]=malloc(strlen(ent->d_name)+1);
					strcpy(list[cnt],ent->d_name);
				}
				cnt++;
			}
		}
	}
	closedir(dir);

	chdir(currentPath);

	return cnt;
}
