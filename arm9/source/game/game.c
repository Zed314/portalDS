/**
 * @file game.c
 * @brief The game state: setup, the frame loop, and the render passes.
 *
 * Implements @ref game_ex.h and is the top of the game half of the program.
 *
 * @par initGame
 * Order matters and is not arbitrary: video modes and VRAM banks, then the 3D
 * engine, then textures and sound, then every entity pool, then the map - and
 * only then @ref transferRectangles, @ref makeGrid and @ref startPI, because
 * the ARM7 must not begin simulating until the collision world it will
 * simulate against exists.
 *
 * @par The frame
 * render1() and its companions do the work, and the ordering is forced by the
 * hardware: the view through each open portal must be rendered and captured
 * *before* the main view, because the main view uses those captures as
 * textures. So one displayed frame is up to three passes over the room, which
 * is why so much of the rest of the codebase is preoccupied with culling.
 *
 * postProcess() runs over the finished frame buffer; @c cpuEndSlice() is the
 * profiling hook whose results the commented-out @c iprintf calls report.
 */

#include "game/game_main.h"

bool currentBuffer;
int mainBG;
u16 mainScreen[256*192];

bool isNextRoom;

char mapFilePath[2048];
char nextMapFilePath[2048];

s16 levelInfoCounter;

bool testStepByStep=false;

PrintConsole bottomScreen;

// extern md2Model_struct storageCubeModel, companionCubeModel, cubeDispenserModel; //TEMP
cubeDispenser_struct* testDispenser;
bigButton_struct* testButton;
bigButton_struct* testButton2;
platform_struct* testPlatform;

u16 vblCNT, frmCNT, FPS;

void setMapFilePath(char* path)
{
	if(!path)return;

	strcpy(mapFilePath,path);
}

void setNextMapFilePath(char* path)
{
	if(!path)return;

	strcpy(nextMapFilePath,path);
}

void endGame(void)
{
	if(!isNextRoom)changeState(&menuState);
	else {
		strcpy(mapFilePath,nextMapFilePath);
		changeState(&gameState);
	}
}

void initGame(void)
{
	lcdMainOnTop();
	int oldv=getMemFree();
	NOGBA("mem free : %dko (%do)",getMemFree()/1024,getMemFree());
	NOGBA("initializing...");
	videoSetMode(MODE_5_3D | DISPLAY_BG3_ACTIVE);
	videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);

	glInit();

	vramSetPrimaryBanks(VRAM_A_TEXTURE,VRAM_B_TEXTURE,VRAM_C_LCD,VRAM_D_MAIN_BG_0x06000000);
	vramSetBankH(VRAM_H_SUB_BG);
	vramSetBankI(VRAM_I_SUB_BG_0x06208000);

	glEnable(GL_TEXTURE_2D);
	// glEnable(GL_ANTIALIAS);
	glDisable(GL_ANTIALIAS);
	glEnable(GL_BLEND);
	glEnable(GL_OUTLINE);

	glSetOutlineColor(0,RGB15(0,0,0)); //TEMP?
	glSetOutlineColor(1,RGB15(0,0,0)); //TEMP?
	glSetOutlineColor(7,RGB15(31,0,0)); //TEMP?
	glSetToonTableRange(0, 15, RGB15(8,8,8)); //TEMP?
	glSetToonTableRange(16, 31, RGB15(24,24,24)); //TEMP?

	glClearColor(31,31,0,31);
	glClearPolyID(63);
	glClearDepth(0x7FFF);

	glViewport(0,0,255,191);

	// initVramBanks(1);
	initVramBanks(2);
	initTextures();
	initSound();

	initCamera(NULL);

	initPlayer(NULL);

	initLights();
	// Not used
	//initParticles();

	initMaterials();

	loadMaterialSlices("slices.ini");
	loadMaterials("materials.ini");
	loadControlConfiguration("config.ini");

	initElevators();
	initWallDoors();
	initTurrets();
	initBigButtons();
	initTimedButtons();
	initEnergyBalls();
	initPlatforms();
	initCubes();
	initEmancipation();
	initDoors();
	initSludge();
	initPause();

	initText();

	NOGBA("lalala");

	getPlayer()->currentRoom=&gameRoom;

	currentBuffer=false;

	getVramStatus();
	fadeIn();

	mainBG=bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	bgSetPriority(mainBG, 0);
	REG_BG0CNT=BG_PRIORITY(3);

	#ifdef DEBUG_GAME
		consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 16, 0, false, true);
		consoleSelect(&bottomScreen);
	#endif

	// glSetToonTableRange(0, 14, RGB15(16,16,16));
	// glSetToonTableRange(15, 31, RGB15(26,26,26));

	initPortals();

	//PHYSICS
	initPI9();

	strcpy(&mapFilePath[strlen(mapFilePath)-3], "map");
	newReadMap(mapFilePath, NULL, 255);

	transferRectangles(&gameRoom);
	makeGrid();
	generateRoomGrid(&gameRoom);
	gameRoom.displayList=generateRoomDisplayList(&gameRoom, vect(0,0,0), vect(0,0,0), false);

	getVramStatus();

	startPI();

	NOGBA("START mem free : %dko (%do)",getMemFree()/1024,getMemFree());
	NOGBA("vs mem free : %dko (%do)",oldv/1024,oldv);

	levelInfoCounter=60;

	#ifdef FRAME_PROFILING
	{
		//marker 0xA: one word per level (re)start, so the harness can see a
		//death-and-restart loop for what it is
		static u16 initCount=0;
		profilerEmitDebug(0xA00000|++initCount);
	}
	#endif
}

bool testbool=false;
bool switchPortal=false;
//portal_struct *currentPortal,
portal_struct *previousPortal;

void postProcess(u16* scrP, u32* stackP);
bool orangeSeen, blueSeen;
extern u16** stackEnd;
u16* ppStack[192*16];

u32 prevTiming;

u32 cpuEndSlice()
{
	u32 temp=prevTiming;
	prevTiming=cpuGetTiming();
	return prevTiming-temp;
}

void drawCenteredString(char* str, int32 scale, u16 y)
{
	if(!str)return;
	int l=strlen(str);

	drawString(str, RGB15(31,31,31), scale, inttof32(128)-(l*scale)*4, inttof32(y));
}

static inline void render1(void)
{
	scanKeys();

	// cpuEndSlice();
	playerControls(NULL);
	updateControls();
	// iprintf("controls : %d  \n",cpuEndSlice());

		updatePlayer(NULL);
	// iprintf("player : %d  \n",cpuEndSlice());

		updatePortals();
		updateTurrets();
		updateBigButtons();
		updateTimedButtons();
		updateEnergyDevices();
		updateEnergyBalls();
		updatePlatforms();
		updateCubeDispensers();
		updateEmancipators();
		updateEmancipationGrids();
		updateDoors();
		updateWallDoors();
	// iprintf("updates : %d  \n",cpuEndSlice());
	profilerSectionEnd(PROF_UPDATES);

	// if(currentPortal)GFX_CLEAR_COLOR=currentPortal->color|(31<<16);
	// else GFX_CLEAR_COLOR=0;
	u16 color=getCurrentPortalColor(getPlayer()->object->position);
	// NOGBA("col %d",color);
	// GFX_CLEAR_COLOR=color|(31<<16);
	GFX_CLEAR_COLOR=RGB15(0,0,0)|(31<<16);

	#ifdef DEBUG_GAME
		if(fifoCheckValue32(FIFO_USER_08))iprintf("\x1b[0J");
		while(fifoCheckValue32(FIFO_USER_08)){int32 cnt=fifoGetValue32(FIFO_USER_08);iprintf("ALERT %ld      \n",cnt);NOGBA("ALERT %d      \n",cnt);}
	#else
		while(fifoCheckValue32(FIFO_USER_08)){int32 cnt=fifoGetValue32(FIFO_USER_08);NOGBA("ALERT %ld      \n",cnt);}
	#endif

	projectCamera(NULL);

	glPushMatrix();

		glScalef32(SCALEFACT,SCALEFACT,SCALEFACT);

		renderGun(NULL);

		transformCamera(NULL);

		cpuEndSlice();
			// drawRoomsGame(128, color);
			drawRoomsGame(0, color);
			// drawCell(getCurrentCell(getPlayer()->currentRoom,getPlayerCamera()->position));
		// iprintf("room : %d  \n",cpuEndSlice());
		// Not used
		//updateParticles();
		//drawParticles();
		// iprintf("particles : %d  \n",cpuEndSlice());

			drawOBBs();
		// iprintf("OBBs : %d  \n",cpuEndSlice());
			drawBigButtons();
			drawTimedButtons();
			drawEnergyDevices();
			drawEnergyBalls();
			drawPlatforms();
			drawCubeDispensers();
			drawTurretsStuff();
			drawEmancipators();
			drawEmancipationGrids();
			drawDoors();
			drawWallDoors(NULL);
			drawSludge(&gameRoom);
		// iprintf("stuff : %d  \n",cpuEndSlice());

		drawPortal(&portal1);
		drawPortal(&portal2);

	glPopMatrix(1);

	//HUD TEST
	if(levelInfoCounter>0 && (levelTitle[0] || levelAuthor[0]))
	{
		levelInfoCounter--;
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
			glLoadIdentity();
			glOrthof32(inttof32(0), inttof32(255), inttof32(191), inttof32(0), -inttof32(1), inttof32(1));

			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
				glLoadIdentity();

				if(levelTitle[0])drawCenteredString(levelTitle, inttof32(17)/10, (82));
				if(levelAuthor[0])drawCenteredString(levelAuthor, inttof32(1), (100));

			glPopMatrix(1);
			glMatrixMode(GL_PROJECTION);
		glPopMatrix(1);
	}

	glFlush(0);
	profilerSectionEnd(PROF_SUBMIT);
}

static inline void render2(void)
{
	if(!(orangeSeen||blueSeen)){previousPortal=NULL;return;}
	if((orangeSeen&&blueSeen)/*||(!orangeSeen&&!blueSeen)*/)
	{
		if(switchPortal)currentPortal=&portal1;
		else currentPortal=&portal2;
	}else if(orangeSeen)currentPortal=&portal1;
	else if(blueSeen)currentPortal=&portal2;

	previousPortal=currentPortal;

	switchPortal^=1;

	glClearColor(0,0,0,31);

	updatePortalCamera(currentPortal, NULL);
	projectCamera(&currentPortal->camera);

	glPushMatrix();

		glScalef32(SCALEFACT,SCALEFACT,SCALEFACT);

		renderGun(NULL);

		transformCamera(&currentPortal->camera);

		// drawRoomsGame(0);
		drawPortalRoom(currentPortal);

		drawPlayer(NULL);

		drawOBBs();
		drawBigButtons();
		drawTimedButtons();
		drawEnergyDevices();
		drawEnergyBalls();
		drawPlatforms();
		drawCubeDispensers();
		drawTurretsStuff();
		drawEmancipators();
		drawEmancipationGrids();
		drawDoors();
		drawWallDoors(currentPortal);
		drawSludge(&gameRoom);

	glPopMatrix(1);

	glFlush(0);
}

static inline void postProcess1(void)
{
	postProcess(mainScreen,ppStack);
}

static inline void postProcess2(void)
{
	u16* p1=mainScreen;
	u16* p2=portal1.viewPoint;
	u16* p3=portal2.viewPoint;
    
	blueSeen=orangeSeen=false;
	int oval=0;
	for(u16 ** stp=ppStack;stp<stackEnd;stp++)
	{
		int val=(int)((*stp)-p1);
		if(val>256*192*2 && p1 !=NULL)
        {
            blueSeen=true;
            val-=256*192*2;
            if(p1[oval-1]==65504)
                oval--;

            if(p1[val]==65504)
                val++;

            if(val-oval>0 && p3!=NULL)
                dmaCopy(&(p3[oval]), &(bgGetGfxPtr(mainBG)[oval]), (val-oval)*2);

        }
		else if(val>256*192 && p1 !=NULL)
        {
            orangeSeen=true;
            val-=256*192;
            if(p1[oval-1]==33791)
                oval--;
            if(p1[val]==33791)
                val++;
            if(val-oval>0 && p2 !=NULL)
                dmaCopy(&(p2[oval]), &(bgGetGfxPtr(mainBG)[oval]), (val-oval)*2);
        }
		else
        {
            if(val-oval>0 && p1 !=NULL)
                dmaCopy(&(p1[oval]), &(bgGetGfxPtr(mainBG)[oval]), (val-oval)*2);
        }
		oval=val;
	}
}

u32 debugVal; //TEMP

#ifdef FRAME_PROFILING
/**
 * Fires the two portals by itself once the chamber has settled, so a headless
 * profiling run exercises the whole portal pipeline. Rather than guessing at
 * view angles, it walks the room's portalable rectangles and points the
 * camera straight at each one's centre - through the shipping shootPlayerGun
 * path, so placement rules apply as in real play. A shot an obstruction or a
 * misfit refuses just moves on to the next candidate a second later. Each
 * attempt reports candidate index, shots placed and the used flags on the
 * profiler's debug channel.
 */
static void profilerAutoShoot(void)
{
	static u8 shotsPlaced=0;
	static u8 tries=0;
	static u16 tick=0; //own counter - frmCNT is the FPS meter and resets every second
	static int candidate=0;
	static vect3D aimCentre;
	static bool aimValid=false;
	tick++;

	player_struct* pl=getPlayer();
	if(!pl->currentRoom)return;

	//once the pair is down, keep the camera on the first portal every frame -
	//the regular camera update rebuilds the matrix, and an unseen portal
	//makes render2 return without doing any work
	if(shotsPlaced>=2)
	{
		if(aimValid)
		{
			vect3D lp=vectDifference(pl->object->position,convertVect(vect(pl->currentRoom->position.x,0,pl->currentRoom->position.y)));
			lp.x-=TILESIZE;lp.z-=TILESIZE;
			const vect3D fw=normalize(vectDifference(aimCentre,lp));
			const vect3D ur=(abs(fw.y)>3900)?vect(inttof32(1),0,0):vect(0,inttof32(1),0);
			//order matters: cross(f,up) keeps the basis right handed, and a
			//mirrored basis inverts winding so culling eats the whole scene
			const vect3D rt=normalize(vectProduct(fw,ur));
			const vect3D upv=vectProduct(rt,fw);
			int32* mm=getPlayerCamera()->transformationMatrix;
			mm[0]=rt.x;mm[3]=rt.y;mm[6]=rt.z;
			mm[1]=upv.x;mm[4]=upv.y;mm[7]=upv.z;
			mm[2]=-fw.x;mm[5]=-fw.y;mm[8]=-fw.z;
		}
		return;
	}
	if(tries>=64 || tick<120 || (tick%30))return;

	//the candidate-th portalable rectangle, wrapping at the end of the list
	rectangle_struct* target=NULL;
	int idx=0;
	listCell_struct* lc=pl->currentRoom->rectangles.first;
	while(lc)
	{
		if(lc->data.portalable){if(idx==candidate){target=&lc->data;break;}idx++;}
		lc=lc->next;
	}
	tries++;
	if(!target){candidate=0;return;}
	candidate++;

	//the rectangle's centre, in the same room space shootPlayerGun raycasts in
	vect3D centre=vect(target->position.x*TILESIZE*2+target->size.x*TILESIZE,
	                   target->position.y*HEIGHTUNIT+target->size.y*(HEIGHTUNIT/2),
	                   target->position.z*TILESIZE*2+target->size.z*TILESIZE);
	const vect3D roomOrig=convertVect(vect(pl->currentRoom->position.x,0,pl->currentRoom->position.y));
	vect3D l=vectDifference(pl->object->position,roomOrig);
	l.x-=TILESIZE;l.z-=TILESIZE;

	//A full pass of legitimate shots did not complete the pair - from this
	//spawn at most one candidate both has line of sight and fits. Profiling
	//wants portals rendering, not a marksmanship exam, so place both portals
	//side by side with movePortal - the same operation a successful shot ends
	//in - on the first portalable wall the ray can actually reach, and keep
	//looking at it. An occluded or back-facing wall would leave the portals
	//unseen, and render2 does no work for an unseen portal.
	if(tries>16)
	{
		vect3D l2=vectDifference(pl->object->position,roomOrig);
		l2.x-=TILESIZE;l2.z-=TILESIZE;
		rectangle_struct* los=NULL;
		vect3D c=vect(0,0,0);
		for(lc=pl->currentRoom->rectangles.first;lc;lc=lc->next)
		{
			if(!lc->data.portalable)continue;
			c=vect(lc->data.position.x*TILESIZE*2+lc->data.size.x*TILESIZE,
			       lc->data.position.y*HEIGHTUNIT+lc->data.size.y*(HEIGHTUNIT/2),
			       lc->data.position.z*TILESIZE*2+lc->data.size.z*TILESIZE);
			vect3D ip;int32 lk=0;
			rectangle_struct* hit=collideLineMapClosest(pl->currentRoom, NULL, l2,
				normalize(vectDifference(c,l2)), inttof32(300)-128, &ip, &lk);
			if(hit==&lc->data){los=&lc->data;break;}
		}
		if(!los)return;
		//a horizontal tangent for walls, x for floors - computePortalPlane
		//derives the other axis
		vect3D plane0;
		if(los->normal.x)plane0=vect(0,0,inttof32(1));
		else plane0=vect(inttof32(1),0,0);
		const vect3D basePos=addVect(addVect(roomOrig,c),vect(TILESIZE,0,TILESIZE));
		const vect3D offset=vectMult(plane0,TILESIZE);
		movePortal(portalForColor(true), vectDifference(basePos,offset), vectMultInt(los->normal,-1), plane0, true);
		movePortal(portalForColor(false), addVect(basePos,offset), vectMultInt(los->normal,-1), plane0, true);
		shotsPlaced=2;
		aimCentre=c;
		aimValid=true;
		centre=c;
	}

	//point the camera at the centre: columns of the matrix are the camera
	//basis, and getUnitVector reads the view direction out of column two
	const vect3D f=normalize(vectDifference(centre,l));
	const vect3D upref=(abs(f.y)>3900)?vect(inttof32(1),0,0):vect(0,inttof32(1),0);
	//cross(f,up) then cross(right,f): right handed, see the comment above
	const vect3D right=normalize(vectProduct(f,upref));
	const vect3D up=vectProduct(right,f);
	int32* m=getPlayerCamera()->transformationMatrix;
	m[0]=right.x;m[3]=right.y;m[6]=right.z;
	m[1]=up.x;   m[4]=up.y;   m[7]=up.z;
	m[2]=-f.x;   m[5]=-f.y;   m[8]=-f.z;

	u8 diag=0;
	if(shotsPlaced<2)
	{
		//diagnose the aim: does the ray reach the rectangle we pointed it at?
		vect3D ip;int32 lk=0;
		rectangle_struct* r=collideLineMapClosest(pl->currentRoom, NULL, l, getUnitVector(NULL), inttof32(300)-128, &ip, &lk);
		if(r)diag|=1;
		if(r && r->portalable)diag|=2;
		if(r==target)diag|=4;

		//mode 4: portal placement only, so a grabbable box cannot eat the shot
		if(shootPlayerGun(NULL, shotsPlaced==0, 4))shotsPlaced++;
		if(shotsPlaced>=2){aimCentre=centre;aimValid=true;}
	}
	profilerEmitDebug(((u32)candidate<<16)|((u32)diag<<8)|((u32)shotsPlaced<<4)|(portal1.used?1:0)|(portal2.used?2:0));
	//marker 0x9: where the player is and how alive - y then x in world units
	//offset by 128 (9 bits each), alive flag in bit 0
	{
		int y=(pl->object->position.y>>12)+128; if(y<0)y=0; if(y>511)y=511;
		int x=(pl->object->position.x>>12)+128; if(x<0)x=0; if(x>511)x=511;
		profilerEmitDebug(0x900000u|((u32)y<<10)|((u32)x<<1)|(u32)(pl->life>0));
	}
}
#endif

void gameFrame(void)
{
	switch(currentBuffer)
	{
		case false:
			#ifdef DEBUG_GAME
				iprintf("\x1b[0;0H");
				iprintf("%d FPS   \n", FPS);
				iprintf("%d (debug)   \n", debugVal);
				iprintf("%d (free ram)   \n", getMemFree()/1024);
				iprintf("%p (portal)   \n", portal1.displayList);
				iprintf("%p (portal)   \n", portal2.displayList);
			#endif
			#ifdef FRAME_PROFILING
				profilerAutoShoot();
			#endif
			cpuEndSlice();
			postProcess1();
			profilerSectionEnd(PROF_POSTPROC);
			// iprintf("postproc : %d  \n",cpuEndSlice());
			render1();

			// if(keysDown()&KEY_SELECT)testStepByStep^=1; //TEMP
			#ifdef DEBUG_GAME
				iprintf("full : %d (%d)  \n",cpuEndTiming(),testStepByStep);
			#endif
			profilerHalfEnd();
			swiWaitForVBlank();
			cpuStartTiming(0);
			profilerEpochStart(1); //the half about to run is B
			prevTiming=0;
			if(previousPortal)dmaCopy(VRAM_C, previousPortal->viewPoint, 256*192*2);
			setRegCapture(true, 0, 15, 2, 0, 3, 1, 0);
			profilerSectionEnd(PROF_COPY);
			frmCNT++;
			break;
		case true:
			// cpuStartTiming(0);
			postProcess2();
			profilerSectionEnd(PROF_POSTPROC);
			// iprintf("frm 2 : %d  \n",cpuGetTiming());
			render2();
			profilerSectionEnd(PROF_SUBMIT);
			listenPI9();
			updateOBBs();
			profilerSectionEnd(PROF_PHYSICS);
			// iprintf("frm 2 : %d  \n",cpuEndTiming());
			#ifdef DEBUG_GAME
				iprintf("fake frame : %d   \n",cpuEndTiming());
			#endif
			profilerHalfEnd();
			swiWaitForVBlank();
			cpuStartTiming(0);
			profilerEpochStart(0); //the half about to run is A
			prevTiming=0;
			dmaCopy(VRAM_C, mainScreen, 256*192*2);
			setRegCapture(true, 0, 15, 2, 0, 3, 1, 0);
			profilerSectionEnd(PROF_COPY);
			break;
	}

	// if(testStepByStep){int i=0;while(!(keysUp()&KEY_TOUCH)){scanKeys();listenPI9();swiWaitForVBlank();}NOGBA("WAITED");scanKeys();scanKeys();if(keysHeld()&KEY_SELECT)testStepByStep=false;}
	// else if(keysDown()&KEY_SELECT)testStepByStep=true;

	currentBuffer^=1;
}

void killGame(void)
{
	fadeOut();
	NOGBA("KILLING IT");
	freePlayer();
	freeEnergyBalls();
	freeBigButtons();
	freeCubes();
	freeDoors();
	freeElevators();
	freeEmancipation();
	freePlatforms();
	freeTimedButtons();
	freeTurrets();
	freeWallDoors();
	freeSludge();
	freeRoom(&gameRoom);
	freePortals();
	freeState(NULL);
	freeSound();
	freePause();

	resetAllPI();

	NOGBA("END mem free : %dko (%do)",getMemFree()/1024,getMemFree());
}

void gameVBL(void)
{
	vblCNT++;
	if(vblCNT>=60)
	{
		FPS=frmCNT;
		frmCNT=vblCNT=0;
	}
}
