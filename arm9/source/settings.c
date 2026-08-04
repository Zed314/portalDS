/**
 * @file settings.c
 * @brief Reading and writing the player's settings.
 *
 * Implements @ref settings.h. The interesting half is saving: the settings
 * share config.ini with the button mapping that controls.c reads, so a save
 * cannot simply write out the four values it knows about. It loads the file
 * into a dictionary, sets its own keys in it, and dumps the whole dictionary
 * back - which leaves every other section exactly as it was.
 *
 * Reading is the mirror of that, one iniparser_getint per setting with the
 * default as the not-found value, and a clamp on the way out. Nothing here
 * treats a missing file, a missing key or an out of range value as an error:
 * all three mean "use the default", because all three are what a card the
 * player has been editing by hand actually looks like.
 */

#include "common/general.h"
#include <ctype.h>


settings_struct settings;


/** Section names, and the keys under them. Lower case, as iniparser stores them. */
#define SECTION_VIDEO "video"
#define SECTION_AUDIO "audio"
#define SECTION_LOOK  "look"

#define KEY_BRIGHTNESS  "brightness"
#define KEY_SFXVOLUME   "sfx_volume"
#define KEY_SENSITIVITY "sensitivity"
#define KEY_INVERTY     "invert_y"


/**
 * @brief Reads one integer setting and clamps it into range.
 *
 * The parsing is done here rather than by iniparser_getint(), which hands
 * anything strtol cannot read back as zero. Zero is a legal setting - a silent
 * game, a black screen - so a mistyped value would be indistinguishable from a
 * deliberate one. A key that is not a number is treated as a key that is not
 * there, and comes back as the default.
 *
 * Base ten, not strtol's base zero: a settings file is decimal, and a player
 * writing a leading zero means ten, not eight.
 */
static int getSetting(dictionary* dic, const char* section, const char* key, int def, int lo, int hi)
{
	char entry[64];
	snprintf(entry, sizeof(entry), "%s:%s", section, key);

	char* str=iniparser_getstring(dic, entry, NULL);
	if(!str)return def;

	char* end=NULL;
	const long v=strtol(str, &end, 10);
	if(end==str)return def;

	while(*end && isspace((int)*end))end++;
	if(*end)return def;

	return max(lo, min(hi, (int)v));
}

/**
 * @brief Writes one integer setting, creating its section if it is not there.
 *
 * A section is a key with no colon in it and no value, which is how
 * iniparser_load() records one and how iniparser_dump_ini() finds one to write
 * a header for. Setting it again when it already exists is harmless: sections
 * hold no value to overwrite.
 */
static void setSetting(dictionary* dic, const char* section, const char* key, int value)
{
	char entry[64], val[16];
	snprintf(entry, sizeof(entry), "%s:%s", section, key);
	snprintf(val, sizeof(val), "%d", value);

	dictionary_set(dic, (char*)section, NULL);
	dictionary_set(dic, entry, val);
}


void defaultSettings(settings_struct* s)
{
	if(!s)return;

	s->lookSensitivity=SETTINGS_SENSITIVITY_DEFAULT;
	s->invertLookY=false;
	s->sfxVolume=SETTINGS_VOLUME_DEFAULT;
	s->brightness=SETTINGS_BRIGHTNESS_DEFAULT;
}

bool loadSettingsFile(const char* path)
{
	defaultSettings(&settings);

	if(!path)return false;

	dictionary* dic=iniparser_load(path);
	if(!dic)return false;

	settings.lookSensitivity=getSetting(dic, SECTION_LOOK, KEY_SENSITIVITY,
		SETTINGS_SENSITIVITY_DEFAULT, SETTINGS_SENSITIVITY_MIN, SETTINGS_SENSITIVITY_MAX);
	settings.invertLookY=getSetting(dic, SECTION_LOOK, KEY_INVERTY, 0, 0, 1)!=0;
	settings.sfxVolume=getSetting(dic, SECTION_AUDIO, KEY_SFXVOLUME,
		SETTINGS_VOLUME_DEFAULT, SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX);
	settings.brightness=getSetting(dic, SECTION_VIDEO, KEY_BRIGHTNESS,
		SETTINGS_BRIGHTNESS_DEFAULT, SETTINGS_BRIGHTNESS_MIN, SETTINGS_BRIGHTNESS_MAX);

	iniparser_freedict(dic);
	return true;
}

bool saveSettingsFile(const char* path)
{
	if(!path)return false;

	//Read the file back before writing it, so that whatever else is in it -
	//the [controls] mapping above all - is written out again with it.
	dictionary* dic=iniparser_load(path);
	if(!dic)dic=dictionary_new(0);
	if(!dic)return false;

	setSetting(dic, SECTION_LOOK, KEY_SENSITIVITY, settings.lookSensitivity);
	setSetting(dic, SECTION_LOOK, KEY_INVERTY, settings.invertLookY?1:0);
	setSetting(dic, SECTION_AUDIO, KEY_SFXVOLUME, settings.sfxVolume);
	setSetting(dic, SECTION_VIDEO, KEY_BRIGHTNESS, settings.brightness);

	FILE* f=fopen(path, "w");
	if(!f){iniparser_freedict(dic);return false;}

	iniparser_dump_ini(dic, f);

	const bool ok=!ferror(f);
	fclose(f);
	iniparser_freedict(dic);
	return ok;
}


/** Set by a button callback, cleared by the vblank that acts on it. */
static volatile bool brightnessPending=false;

void requestBrightnessUpdate(void)
{
	brightnessPending=true;
}

void updatePendingBrightness(void)
{
	if(!brightnessPending)return;

	brightnessPending=false;
	setBrightness(3, settings.brightness);
}


/**
 * @brief Builds the path to the settings file on the card.
 * @return false if there is no card, and so no path to build.
 */
static bool settingsPath(char* out, size_t n)
{
	if(!basePath)return false;

	snprintf(out, n, "%s/%s", basePath, SETTINGSFILE);
	return true;
}

void loadSettings(void)
{
	char path[255];

	//The same two places loadControlConfiguration() looks, in the same order:
	//the card first, then the working directory, which is where the file is
	//when the game was started from one.
	if(settingsPath(path, sizeof(path)) && loadSettingsFile(path))return;

	loadSettingsFile(SETTINGSFILE);
}

bool saveSettings(void)
{
	char path[255];

	//No fallback to the working directory here, unlike loadSettings(): during
	//play that is NitroFS, which is inside the .nds file and read only.
	if(!settingsPath(path, sizeof(path)))return false;

	return saveSettingsFile(path);
}

bool settingsCanBeSaved(void)
{
	return basePath!=NULL;
}
