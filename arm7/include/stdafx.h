/**
 * @file stdafx.h
 * @brief Precompiled-header style umbrella include for the whole ARM7 build.
 *
 * Every ARM7 source file includes this and nothing else. The include order
 * matters and is not arbitrary: @ref math.h defines @ref vect3D, which every
 * later header needs, and @ref PI7.h must come after @ref AAR.h because
 * portal_struct embeds an array of @ref AAR_struct.
 *
 * The name is a Visual Studio convention that came along with the original
 * 2007-era code.
 */

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nds.h>

#include "math.h"     // fixed point vectors and 3x3 matrices
#include "OBB.h"      // rigid bodies
#include "plane.h"    // legacy infinite planes
#include "AAR.h"      // static collision rectangles and their broadphase grid
#include "PI7.h"      // FIFO protocol front end (pulls in common/include/PIC.h)
#include "platform.h" // moving platforms

//#define NOGBA(_fmt, _args...) do { char nogba_buffer[128]; snprintf(nogba_buffer, sizeof(nogba_buffer), _fmt, ##_args); nocashMessage(nogba_buffer); } while(0)

//#include "debug.h"


// TODO: reference additional headers your program requires here
