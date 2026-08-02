/**
 * @file sfx.h
 * @brief Sound effects.
 *
 * A thin layer over libnds' sound API: each effect is a raw sample buffered
 * entirely in RAM and fired with @ref playSFX. There is no mixing policy, no
 * priority and no positional audio - the hardware allocates a channel and that
 * is the end of it.
 *
 * Samples are held in memory rather than streamed because the ARM7 owns the
 * sound hardware and streaming would mean sharing a FIFO that is already busy
 * carrying physics results.
 */

#ifndef SFX_H
#define SFX_H

#define NUMSFX (32) /**< Maximum number of loaded effects. */

/** @brief A sound effect held in RAM. */
typedef struct
{
	u8* data;          /**< Raw sample data. */
	u32 size;          /**< Sample size in bytes. */
	SoundFormat format;/**< Sample format: 8 bit, 16 bit or ADPCM. */
	bool used;         /**< False when this slot is free. */
}SFX_struct;

/** @brief Brings up the sound hardware. Call before loading any effect. */
void initSound(void);

/** @brief Shuts down sound and frees every loaded effect. */
void freeSound(void);

/** @brief Clears an effect slot. */
void initSFX(SFX_struct* s);

/**
 * @brief Loads a sample into an existing slot.
 * @param s        slot to fill.
 * @param filename sample file to read.
 * @param format   sample format.
 */
void loadSFX(SFX_struct* s, char* filename, SoundFormat format);

/**
 * @brief Allocates a slot and loads a sample into it.
 * @return the new effect, or NULL if the pool is full or the file is missing.
 */
SFX_struct* createSFX(char* filename, SoundFormat format);

/** @brief Plays an effect once on the next free hardware channel. */
void playSFX(SFX_struct* s);

#endif
