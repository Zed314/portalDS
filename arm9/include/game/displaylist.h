/**
 * @file displaylist.h
 * @brief Recording geometry into a hardware display list.
 *
 * The DS geometry engine can execute a pre-recorded command buffer with a
 * single DMA transfer. That is far cheaper than writing the same commands to
 * its registers one at a time, and it is how the room and the models are drawn
 * fast enough to render the world three times a frame.
 *
 * These functions mirror the usual @c gl* drawing calls, but instead of poking
 * the hardware they append to a buffer:
 *
 * @code
 * u32* dl = glBeginListDL();
 *     glBeginDL(GL_QUADS);
 *         glTexCoordPACKED(...);
 *         glVertexPackedDL(...);
 *         ...
 *     glEndDL();
 * u32 size = glEndListDL();   // dl now holds a display list of `size` words
 * @endcode
 *
 * The result is handed to @c glCallList to draw.
 *
 * @warning There is one recording buffer, so lists cannot be nested or built
 *          concurrently. @ref glEndListDL must be reached before another
 *          @ref glBeginListDL.
 *
 * @see map.h (@ref generateRoomDisplayList) and md2.h
 *      (@ref generateModelDisplayLists) for the two users.
 */

#ifndef DISPLAYLIST9_H_
#define DISPLAYLIST9_H_

/** @brief Starts recording. @return the buffer commands will be written into. */
u32* glBeginListDL();

/** @brief Stops recording. @return the length of the recorded list, in words. */
u32 glEndListDL();

/** @brief Records a primitive begin. @param type GL_TRIANGLES, GL_QUADS, and so on. */
u32 glBeginDL(u32 type);

/** @brief Records a primitive end. */
void glEndDL();

/** @brief Records a packed normal. @see anorms.h */
u32 glNormalDL(uint32 normal);

/** @brief Records a vertex already packed into the hardware's VERTEX10 format. */
u32 glVertexPackedDL(u32 packed);

/** @brief Records a vertex from three 16 bit components. */
void glVertex3v16DL(v16 x, v16 y, v16 z);

/** @brief Records a packed texture coordinate pair. */
void glTexCoordPACKED(u32 uv);

/** @brief Records a vertex colour. */
void glColorDL(rgb color);

/** @brief Records a polygon attribute change: blending, lighting, polygon id. */
void glPolyFmtDL(u32 fmt);

/** @brief Records the texture and palette binding for a material. */
void applyMTLDL(mtlImg_struct* mtl);

#endif
