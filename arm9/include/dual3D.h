/**
 * @file dual3D.h
 * @brief Rendering hardware-accelerated 3D to both screens at once.
 *
 * The DS has exactly one 3D engine and it can only draw to one screen. The
 * standard trick, implemented here, is to render alternate frames for
 * alternate screens and use the display capture unit to freeze each result
 * into a VRAM bank, which is then shown as a bitmap background on the screen
 * it belongs to.
 *
 * The cost is that each screen only updates at 30Hz, and that two VRAM banks
 * are permanently spoken for. The benefit is that the game can show the world
 * on the top screen and the portal gun view (or the editor's viewport) on the
 * bottom.
 *
 * @ref updateD3D is called once per frame and does the swap: it captures what
 * was just rendered, flips @ref d3dScreen, and reconfigures the backgrounds
 * and capture registers for the other screen.
 */

#ifndef __D3D9__
#define __D3D9__

/** @brief Which screen the 3D engine is currently rendering for: false = main, true = sub. */
extern bool d3dScreen;

/**
 * @brief Sets up the video modes, VRAM banks and 3D engine for dual screen 3D.
 *
 * Must be called before any 3D drawing, and again after anything that
 * reassigns VRAM banks.
 */
void initD3D();

/**
 * @brief Ends a frame and flips which screen the 3D engine draws to.
 *
 * Call once per frame after all drawing. Captures the finished frame into its
 * VRAM bank, then swaps the backgrounds and capture configuration so the next
 * frame renders for the other screen.
 */
void updateD3D();

/**
 * @brief Programs the display capture register directly.
 *
 * A thin wrapper over @c REG_DISPCAPCNT for the cases libnds does not cover.
 *
 * @param enable    start a capture this frame.
 * @param srcBlend  blend weight of source A.
 * @param destBlend blend weight of source B.
 * @param bank      destination VRAM bank, 0-3 for A-D.
 * @param offset    write offset within the bank.
 * @param size      capture size selector (see GBATEK).
 * @param source    which sources to capture: 3D, the 2D layers, or a blend.
 * @param srcOffset read offset for source B.
 */
void setRegCapture(bool enable, uint8 srcBlend, uint8 destBlend, uint8 bank, uint8 offset, uint8 size, uint8 source, uint8 srcOffset);
#endif
