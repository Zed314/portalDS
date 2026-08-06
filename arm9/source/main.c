/**
 * @file main.c
 * @brief ARM9 entry point, the three state definitions and the outer loop.
 *
 * Startup order matters here:
 *  1. @c defaultExceptionHandler, so a crash shows a register dump rather than
 *     a white screen;
 *  2. @ref initFilesystem - if this fails there is nothing to load, so the
 *     game prints a message and waits for START rather than continuing;
 *  3. @ref loadSettings, which reads the card and so has to follow the
 *     filesystem, and which every state reads rather than asks for and so has
 *     to precede all of them;
 *  4. @c glInit, then optionally the address sanitizer;
 *  5. @ref changeState / @ref applyState to select the first state.
 *
 * After that main() runs the state machine forever: init, frame until the
 * state asks to end, kill, switch. See @ref state.h for how that works.
 *
 * @note The SELECT-held check before the final @c changeState is vestigial.
 *       Whichever state it picks is immediately overridden by the
 *       @c changeState(&menuState) that follows, so the game always starts at
 *       the menu - which is where the editor is actually reachable from.
 *
 * doSPALSH() draws the splash screens. It is currently not called; the
 * commented-out call sits just above the state selection.
 */

#include "common/general.h"


state_struct gameState={.init=&initGame, .frame=&gameFrame, .kill=&killGame, .vbl=&gameVBL, .id=0, .mc_id=0};
state_struct menuState={.init=&initMenu, .frame=&menuFrame, .kill=&killMenu, .vbl=&menuVBL, .id=1, .mc_id=0};
state_struct editorState={.init=&initEditor, .frame=&editorFrame, .kill=&killEditor, .vbl=&editorVBL, .id=2, .mc_id=0};


extern state_struct * current_state;

void doSPALSH()
{
    //vblank first, as in fadeIn(): this is the same mid-frame write.
    swiWaitForVBlank();
    setBrightness(3,-16);
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);

    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankC(VRAM_C_SUB_BG);

    int bg = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0,0);
    struct gl_texture_t* spalsh=(struct gl_texture_t *)ReadPCXFile("spalsh.pcx","");

    // The copies below are a fixed screenful, so the image has to be one -
    // a missing or wrongly sized splash used to be read past the end of.
    if(spalsh)
    {
        if(spalsh->width==256 && spalsh->height==192)
        {
            dmaCopy(spalsh->texels, bgGetGfxPtr(bg), 256*192);
            dmaCopy(spalsh->palette, BG_PALETTE, 256*2);
        }
        freePCX(spalsh);
    }

    int bg_sub = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0,0);
    struct gl_texture_t* spalsh_sub=(struct gl_texture_t *)ReadPCXFile("spalsh_bottom.pcx","");

    if(spalsh_sub)
    {
        if(spalsh_sub->width==256 && spalsh_sub->height==192)
        {
            dmaCopy(spalsh_sub->texels, bgGetGfxPtr(bg_sub), 256*192);
            dmaCopy(spalsh_sub->palette, BG_PALETTE_SUB, 256*2);
        }
        freePCX(spalsh_sub);
    }

    fadeIn();
    int i;
    for(i=0;i<60;i++)swiWaitForVBlank();
    fadeOut();
}

int main(int argc, char **argv)
{
    defaultExceptionHandler();
    int ret=initFilesystem(argc, argv);
    if (!ret)
    {
        consoleDemoInit();
        printf("Failed to initalize filesystem.\n");
        printf("Press START to exit.\n");
        while (1)
        {
            swiWaitForVBlank();
            scanKeys();

            if (keysDown() & KEY_START)
                return 1;
        }
    }
    //Needs the filesystem up, and has to be before any state is entered: the
    //states read settings rather than ask for them.
    loadSettings();

    glInit();
#if McuASAN_CONFIG_IS_ENABLED
    NOGBA("Init ASAN\n");
    McuASAN_Init();
#endif
    //initAudio();

    //doSPALSH();
    NOGBA("scan keys\n");
    //TEMP DEBUG
    scanKeys();
    scanKeys();
    scanKeys();
    scanKeys();

    if(keysHeld() & KEY_SELECT)
    {
        NOGBA("editorstate\n");
        changeState(&editorState);
    }
    else
    {
        NOGBA("gamestate\n");
        changeState(&gameState);
    }
    #ifdef FRAME_PROFILING
        //A profiling build wants gameplay numbers without a hand on the touch
        //screen, so boot straight into a chamber instead of stopping at the
        //menu.
        setMapFilePath("maps/default.map");
        changeState(&gameState);
    #else
        NOGBA("menustate\n");
        changeState(&menuState);
    #endif

    NOGBA("applystate\n");
    applyState();

    while(1)
    {
        currentState->init();
        while(currentState->used)
            currentState->frame();

        currentState->kill();
        applyState();
    }

    return 0;
}
