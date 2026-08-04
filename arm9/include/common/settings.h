/**
 * @file settings.h
 * @brief The player's settings, and the config.ini they are kept in.
 *
 * Everything the options menu can change lives in one @ref settings_struct, so
 * that reading a setting is a field access from anywhere and saving is one
 * call. There is exactly one instance, @ref settings, and it is valid from
 * @ref loadSettings onwards - which main() calls before any state is entered,
 * so no code has to cope with settings that have not been read yet.
 *
 * The file is @c config.ini in @ref basePath, the same file
 * @ref loadControlConfiguration reads the button mapping out of. That is
 * deliberate: the player has one file to edit by hand, not two. Saving
 * therefore reads the file back in first and writes it out whole, so the
 * @c [controls] section survives a save even though nothing here understands
 * it. Comments and key order do not survive - iniparser only keeps the
 * key/value pairs themselves.
 *
 * Every value read from the file is clamped to the range its @c _MIN and
 * @c _MAX give. A settings file is user-editable and lives on a card that can
 * be pulled mid-write, so a value out of range is an expected input rather
 * than a corrupt one, and it must not be able to leave the game unplayable -
 * see @ref SETTINGS_BRIGHTNESS_MIN in particular.
 *
 * @note The card is the only writable filesystem, so there is nowhere to save
 *       to when the game runs from a plain ROM. @ref saveSettings returns
 *       false in that case, and loading still works: the settings are simply
 *       whatever the defaults are.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#define SETTINGSFILE "config.ini" /**< Settings and button mapping, in @ref basePath. */

/** @name Look sensitivity, as a percentage of the 1:1 rate a drag rotates at. */
///@{
#define SETTINGS_SENSITIVITY_MIN (25)
#define SETTINGS_SENSITIVITY_MAX (200)
#define SETTINGS_SENSITIVITY_DEFAULT (100)
#define SETTINGS_SENSITIVITY_STEP (25)
///@}

/**
 * @name Effect volume, as a percentage.
 *
 * A percentage rather than the 0-127 the hardware takes, because this is a
 * number the player reads in the options menu and edits by hand in the file.
 * @ref playSFX scales it.
 */
///@{
#define SETTINGS_VOLUME_MIN (0)
#define SETTINGS_VOLUME_MAX (100)
#define SETTINGS_VOLUME_DEFAULT (100)
#define SETTINGS_VOLUME_STEP (10)
#define SETTINGS_VOLUME_HARDWARE_MAX (127) /**< What 100% is, to the sound hardware. */
///@}

/**
 * @name Screen brightness, as an offset applied to the panel's own.
 *
 * The hardware accepts -16 to 16, but -16 is a black screen and 16 a white
 * one. The range stops well short of both because this is a setting the player
 * can save and then has to be able to see well enough to change back.
 */
///@{
#define SETTINGS_BRIGHTNESS_MIN (-8)
#define SETTINGS_BRIGHTNESS_MAX (8)
#define SETTINGS_BRIGHTNESS_DEFAULT (0)
#define SETTINGS_BRIGHTNESS_STEP (2)
///@}

/** @brief Everything the options menu can change. */
typedef struct
{
	u8 lookSensitivity; /**< Percentage of the 1:1 rate a touch drag turns the camera at. */
	bool invertLookY;   /**< True to make dragging down look up. */
	u8 sfxVolume;       /**< Volume every effect is played at. */
	s8 brightness;      /**< Master brightness offset; 0 is the panel's own. */
}settings_struct;

extern settings_struct settings; /**< The live settings. Valid from @ref loadSettings onwards. */

/** @brief Fills a settings struct with the defaults. Never fails. */
void defaultSettings(settings_struct* s);

/**
 * @brief Reads @ref settings from the settings file.
 *
 * Call once, after @ref initFilesystem. Settings the file does not mention -
 * or does not exist to mention - keep their default, so this always leaves
 * @ref settings usable.
 */
void loadSettings(void);

/**
 * @brief Writes @ref settings back to the settings file.
 * @return false if there was nowhere to write, or the write failed.
 */
bool saveSettings(void);

/**
 * @brief Whether there is anywhere to save to at all.
 *
 * True when the game was started from a card. Lets the options menu say so
 * while it is up, rather than leaving the player to find out that nothing was
 * kept the next time they switch on.
 */
bool settingsCanBeSaved(void);

/**
 * @brief Reads @ref settings from a named file.
 *
 * @ref loadSettings once it has worked out the path. Resets to the defaults
 * first, so what comes back is the file's values over the defaults rather than
 * over whatever was there before.
 *
 * @param path file to read.
 * @return true if a file was read; false if it could not be opened, in which
 *         case @ref settings is left at the defaults.
 */
bool loadSettingsFile(const char* path);

/**
 * @brief Asks for the screen brightness to be applied at the next vblank.
 *
 * Brightness is a hardware register that must not be written mid-frame - the
 * screen is drawn part at the old level and part at the new one, leaving a
 * seam travelling down it. So a change made from a button callback is only
 * recorded here, and @ref updatePendingBrightness does the write.
 */
void requestBrightnessUpdate(void);

/**
 * @brief Applies a brightness change asked for since the last vblank.
 *
 * Call from a vblank handler - menuVBL() does. Does nothing unless
 * @ref requestBrightnessUpdate was called, which is what keeps it from
 * fighting the fades in @ref fadeIn and @ref fadeOut.
 */
void updatePendingBrightness(void);

/**
 * @brief Writes @ref settings to a named file, keeping the rest of it.
 *
 * @ref saveSettings once it has worked out the path. The file is read back in
 * before it is written out, so sections this module knows nothing about are
 * preserved - see the note on @c [controls] at the top of this file.
 *
 * @param path file to write.
 * @return true if the file was written.
 */
bool saveSettingsFile(const char* path);

#endif
