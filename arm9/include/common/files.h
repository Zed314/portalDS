/**
 * @file files.h
 * @brief File system setup and whole-file loading.
 *
 * The game reads its assets from two places and the difference matters:
 *
 *  - **NitroFS** (@c nitro:/), the read-only filesystem baked into the .nds
 *    file itself. All the shipped levels, models and textures live here, under
 *    the @ref ROOT directory. This always works, on any cartridge or emulator.
 *  - **FAT**, the SD card or flashcart, reached through DLDI. This is the only
 *    place the game can *write*, so saved games, editor levels and screenshots
 *    go here, under an @c asds directory created at startup.
 *
 * @ref initFilesystem brings both up, creating the writable directories if it
 * can, and quietly continues in read-only mode if it cannot - which is why
 * running from an unpatched ROM gives you the game but not the editor's save.
 */

#ifndef __FILES9_H__
#define __FILES9_H__

#define ROOT "asds" /**< Name of the game's directory, in NitroFS and on the card. */

extern char* basePath; /**< The FAT working directory the game was launched from. */

//extern bool saveAvailable;
//extern u8 fsMode;
extern int lastSize; /**< Size in bytes of the file most recently read by @ref bufferizeFile. */

/**
 * @brief Changes the current directory.
 *
 * Declared here because the paths in use switch between the @c nitro:/ and
 * FAT roots throughout loading.
 */
int chdir (const char *path);

/**
 * @brief Tests whether a file can be opened.
 * @param filename file name.
 * @param dir      directory to look in; may be empty for the current one.
 */
bool fileExists(char* filename, char* dir);

/**
 * @brief Mounts NitroFS and, if possible, the FAT card.
 *
 * Creates @c asds, @c asds/maps and @c asds/screens on the card so the editor
 * has somewhere to write. Failure to create them is not fatal - saving is
 * simply disabled.
 *
 * @param argc argc from main(), passed through to the DLDI init.
 * @param argv argv from main(); NULL means there is no FAT device.
 * @return true if at least NitroFS came up.
 */
bool initFilesystem(int argc, char **argv);

/**
 * @brief Reads an entire file into a freshly allocated buffer.
 *
 * @param filename file name.
 * @param dir      directory to look in; may be empty for the current one.
 * @param size     out: the file size in bytes. May be NULL.
 * @param binary   true to open in binary mode.
 * @return the buffer, which the caller owns, or NULL if the file could not be read.
 */
void* bufferizeFile(char* filename, char* dir, u32* size, bool binary);

#endif
