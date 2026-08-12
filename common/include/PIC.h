/**
 * @file PIC.h
 * @brief Portal Interaction Common - definitions shared by both CPUs.
 *
 * portalDS splits the game across the DS's two processors:
 *
 *  - the **ARM9** runs the game itself (rendering, level logic, input) and
 *    keeps a lightweight mirror of every physics object, see @ref PI9.h;
 *  - the **ARM7**, which would otherwise sit almost idle, runs the rigid body
 *    dynamics engine, see @ref PI7.h.
 *
 * The two halves talk over libnds' FIFO channels. This header defines the
 * things both sides must agree on: the command opcodes (@ref message_type),
 * how those opcodes are packed into a 32 bit FIFO word, and the handful of
 * portal maths helpers that must behave identically on both CPUs.
 *
 * @par Command encoding
 * Commands are sent on @c FIFO_USER_08 as a single header word followed by a
 * fixed number of argument words. The header word packs the opcode in the low
 * @ref PISIGNALDATA bits and an object id in the remaining bits:
 * @code
 *  31                    6 5        0
 * +-----------------------+----------+
 * |        object id      |  opcode  |
 * +-----------------------+----------+
 * @endcode
 * so the opcode is recovered with @c signal&PISIGNALMASK and the id with
 * @c signal>>PISIGNALDATA. Replies (object positions and orientations) travel
 * back the other way on @c FIFO_USER_01 .. @c FIFO_USER_07; see
 * @ref sendDataPI7 and @ref listenPI9.
 */

#ifndef PIC_H
#define PIC_H

/**
 * @name Portal geometry
 *
 * A portal is an ellipse-ish quad one world unit wide before scaling. Rather
 * than store its half-extents, both CPUs derive them by dividing a unit vector
 * by these constants, which keeps the two sides bit-for-bit identical.
 * @{
 */
#define PORTALFRACTIONX (12) /**< Divisor giving a portal's half width. */
#define PORTALFRACTIONY (6)  /**< Divisor giving a portal's half height. */

#define PORTALSIZEX (inttof32(1)/PORTALFRACTIONX) /**< Portal half width, f32. */
#define PORTALSIZEY (inttof32(1)/PORTALFRACTIONY) /**< Portal half height, f32. */
/** @} */

/**
 * @name FIFO command packing
 * @{
 */
#define PISIGNALDATA (6)                    /**< Bits of the FIFO header word used by the opcode. */
#define PISIGNALMASK ((1<<PISIGNALDATA)-1)  /**< Mask isolating the opcode in a FIFO header word. */
/** @} */

#define NUMPLATFORMS (8)     /**< Maximum number of moving platforms tracked by the physics engine. */
#define PLATFORMSIZE (256*3) /**< Platform half extent, in f32 units (a platform is a fixed size box). */

#define NUMAARS (300) /**< Maximum number of axis aligned rectangles (static collision surfaces). */

/**
 * @brief Opcodes understood by the ARM7 physics engine.
 *
 * The comment on each entry gives the number of argument words that follow the
 * header word, and their layout. @c id refers to the object id packed into the
 * header word itself; @c [a|b] means two 16 bit halves in one word.
 *
 * @note The numbering is not contiguous by accident of history - @c PI_RESETALL
 *       was added late and had to take the next free value (18) rather than
 *       slot in next to the other reset commands.
 *
 * @see listenPI7 for the decoder, and PI9.c for the senders.
 */
typedef enum
{
	PI_START=1,  	    /**< Resume simulation. ARG : 0 */
	PI_PAUSE=2,  	    /**< Suspend simulation, keeping all state. ARG : 0 */
	PI_STOP=3,   	    /**< Stop simulation. ARG : 0 */
	PI_RESET=4,  	    /**< Drop all dynamic boxes and restart. ARG : 0 */
	PI_RESETALL=18,	    /**< Full re-init: boxes, rectangles and platforms. ARG : 0 */
	PI_ADDBOX=5,  	    /**< Spawn a rigid body. ARG : 7 (id;[sizex|sizey][sizez|mass][posx][posy][posz][cosine][sine]) */
	PI_APPLYFORCE=6,    /**< Apply an impulse at a point. ARG : 5 (id;[posx|posy][posz][vx][vy][vz]) */
	PI_ADDAAR=7,  	    /**< Add a static collision rectangle. ARG : 7 (id;[sizex][sizey][sizez][normal][posx][posy][posz]) */
	PI_MAKEGRID=8,      /**< Rebuild the broadphase grid over the rectangles. ARG : 0 */
	PI_SETVELOCITY=9,   /**< Overwrite a body's linear velocity. ARG : 3 (id;[vx][vy][vz]) */
	PI_UPDATEPLAYER=10, /**< Tell the ARM7 where the player is. ARG : 3 ([vx][vy][vz]) */
	PI_UPDATEPORTAL=11, /**< Move a portal (also echoes the plane back). ARG : 7 (id;[px][py][pz][n][p0x][p0y][p0z]) */
	PI_ADDPLATFORM=12,  /**< Create a moving platform. ARG : 3 (id;[posx][posy][posz]) */
	PI_UPDATEPLATFORM=13,/**< Teleport a platform to a position. ARG : 3 (id;[posx][posy][posz]) */
	PI_TOGGLEPLATFORM=14,/**< Start or stop a platform. ARG : 1 (id;[active]) */
	PI_KILLBOX=15,       /**< Free a rigid body slot. ARG : 0 (id) */
	PI_RESETPORTALS=16,  /**< Mark both portals unused. ARG : 0 */
	PI_TOGGLEAAR=17,  	 /**< Enable/disable a static rectangle. ARG : 0 (id) */
	// 18 taken you fool
}message_type;

#ifdef ARM7
	/**
	 * @brief Rebuilds the four invisible rectangles that guide bodies through a portal.
	 *
	 * ARM7 only. Defined in AAR.c.
	 *
	 * @param p portal to regenerate the guide geometry for.
	 */
	void generateGuidAAR(portal_struct* p);

	/**
	 * @brief Completes a portal's local frame from its normal and first tangent.
	 *
	 * The second tangent is simply the cross product of the two, so only the
	 * normal and @c plane[0] ever need to be sent over the FIFO.
	 *
	 * This is the ARM7 flavour: it additionally refreshes the guide rectangles
	 * that stop rigid bodies from clipping the wall around the portal mouth.
	 *
	 * @param p portal whose frame should be completed; ignored if NULL.
	 */
	static inline void computePortalPlane(portal_struct* p)
	{
		if(!p)return;

		p->plane[1]=vectProduct(p->normal,p->plane[0]);

		//TEST
		generateGuidAAR(p);
	}
#else
	/**
	 * @brief Completes a portal's local frame from its normal and first tangent.
	 *
	 * ARM9 flavour: pure geometry, no collision side effects.
	 *
	 * @param p portal whose frame should be completed; ignored if NULL.
	 */
	static inline void computePortalPlane(portal_struct* p)
	{
		if(!p)return;

		p->plane[1]=vectProduct(p->normal,p->plane[0]);
	}
#endif

/**
 * @brief Transports a direction vector through a portal.
 *
 * Expresses @p v in @p p's local frame, then rebuilds it in the destination
 * portal's frame with the normal and first tangent flipped - that flip is what
 * makes you come *out* of the far portal rather than back into it.
 *
 * @param p portal being entered.
 * @param v vector expressed in world space.
 * @return @p v as seen on the far side, or a zero vector if @p p has no target.
 */
vect3D warpVector(portal_struct* p, vect3D v);

/**
 * @brief Transports a whole orientation through a portal.
 *
 * Applies @ref warpVector to each column of a row-major 3x3 matrix, in place.
 * Used to carry a rigid body's orientation across a teleport.
 *
 * @param p portal being entered.
 * @param m 3x3 matrix, modified in place; ignored if NULL.
 */
void warpMatrix(portal_struct* p, int32* m); //3x3


#endif
