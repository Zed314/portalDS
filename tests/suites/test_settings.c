/*
 * The settings file - arm9/source/settings.c.
 *
 * Like test_pcx this suite needs no stand-in for the thing under test: the
 * settings are plain stdio and the real iniparser, so the tests hand it actual
 * files written to /tmp and read back what it wrote.
 *
 * @par What is being pinned
 * Two things, and the second is the one that matters.
 *
 * The first is that a file the player has been editing cannot produce settings
 * the game cannot work with. A missing file, a missing key, a key that is not
 * a number and a number outside its range all have to come back as the
 * default or as the nearest value in range - never as-is. Brightness is the
 * pointed case: the hardware takes -16, the game does not offer it, and a
 * hand-typed -16 must not reach setBrightness().
 *
 * The second is that saving keeps the rest of the file. config.ini holds the
 * button mapping as well as the settings, and controls.c is the only thing
 * that understands that section - so a save that wrote out only what
 * settings.c knows about would silently throw a player's remapping away. The
 * round trip tests below are what stop that.
 */

#include <unistd.h>

#include "unity.h"
#include "level_fixture.h"

/* --- fixture ------------------------------------------------------------- */

static const char* tempPath(void) { return "/tmp/portalds_test_config.ini"; }

/** Writes a settings file, byte for byte as given. */
static void writeConfig(const char* contents)
{
	FILE* f=fopen(tempPath(), "w");
	TEST_ASSERT_NOT_NULL(f);
	fputs(contents, f);
	fclose(f);
}

/** Reads the file back whole, for the tests that assert on what was written. */
static char* readConfig(void)
{
	static char buffer[4096];

	FILE* f=fopen(tempPath(), "r");
	TEST_ASSERT_NOT_NULL(f);

	const size_t n=fread(buffer, 1, sizeof(buffer)-1, f);
	fclose(f);

	buffer[n]='\0';
	return buffer;
}

void setUp(void) { defaultSettings(&settings); }
void tearDown(void) { unlink(tempPath()); }

/* --- defaults ------------------------------------------------------------ */

static void test_a_missing_file_leaves_the_defaults(void)
{
	settings.sfxVolume=3;

	TEST_ASSERT_FALSE(loadSettingsFile("/tmp/portalds_no_such_config.ini"));

	TEST_ASSERT_EQUAL_INT(SETTINGS_VOLUME_DEFAULT, settings.sfxVolume);
	TEST_ASSERT_EQUAL_INT(SETTINGS_SENSITIVITY_DEFAULT, settings.lookSensitivity);
	TEST_ASSERT_EQUAL_INT(SETTINGS_BRIGHTNESS_DEFAULT, settings.brightness);
	TEST_ASSERT_FALSE(settings.invertLookY);
}

static void test_a_null_path_leaves_the_defaults(void)
{
	settings.sfxVolume=3;

	TEST_ASSERT_FALSE(loadSettingsFile(NULL));

	TEST_ASSERT_EQUAL_INT(SETTINGS_VOLUME_DEFAULT, settings.sfxVolume);
}

/*
 * A file with only some of the settings in it is the normal case for one a
 * player edited by hand, and for one written by an older build.
 */
static void test_a_key_the_file_does_not_have_keeps_its_default(void)
{
	writeConfig("[audio]\nsfx_volume = 40\n");

	TEST_ASSERT_TRUE(loadSettingsFile(tempPath()));

	TEST_ASSERT_EQUAL_INT(40, settings.sfxVolume);
	TEST_ASSERT_EQUAL_INT(SETTINGS_SENSITIVITY_DEFAULT, settings.lookSensitivity);
}

/* --- reading ------------------------------------------------------------- */

static void test_every_setting_is_read(void)
{
	writeConfig("[video]\nbrightness = -3\n"
	            "[audio]\nsfx_volume = 64\n"
	            "[look]\nsensitivity = 150\ninvert_y = 1\n");

	TEST_ASSERT_TRUE(loadSettingsFile(tempPath()));

	TEST_ASSERT_EQUAL_INT(-3, settings.brightness);
	TEST_ASSERT_EQUAL_INT(64, settings.sfxVolume);
	TEST_ASSERT_EQUAL_INT(150, settings.lookSensitivity);
	TEST_ASSERT_TRUE(settings.invertLookY);
}

static void test_a_value_that_is_not_a_number_falls_back_to_the_default(void)
{
	writeConfig("[audio]\nsfx_volume = loud\n");

	TEST_ASSERT_TRUE(loadSettingsFile(tempPath()));

	TEST_ASSERT_EQUAL_INT(SETTINGS_VOLUME_DEFAULT, settings.sfxVolume);
}

/* --- clamping ------------------------------------------------------------ */

static void test_a_value_above_its_range_is_clamped(void)
{
	writeConfig("[audio]\nsfx_volume = 4000\n"
	            "[look]\nsensitivity = 4000\n"
	            "[video]\nbrightness = 16\n");

	TEST_ASSERT_TRUE(loadSettingsFile(tempPath()));

	TEST_ASSERT_EQUAL_INT(SETTINGS_VOLUME_MAX, settings.sfxVolume);
	TEST_ASSERT_EQUAL_INT(SETTINGS_SENSITIVITY_MAX, settings.lookSensitivity);
	TEST_ASSERT_EQUAL_INT(SETTINGS_BRIGHTNESS_MAX, settings.brightness);
}

/*
 * -16 is the value that turns the screen black. The hardware accepts it, so
 * nothing below this clamp would stop it reaching setBrightness().
 */
static void test_a_value_below_its_range_is_clamped(void)
{
	writeConfig("[audio]\nsfx_volume = -1\n"
	            "[look]\nsensitivity = 0\n"
	            "[video]\nbrightness = -16\n");

	TEST_ASSERT_TRUE(loadSettingsFile(tempPath()));

	TEST_ASSERT_EQUAL_INT(SETTINGS_VOLUME_MIN, settings.sfxVolume);
	TEST_ASSERT_EQUAL_INT(SETTINGS_SENSITIVITY_MIN, settings.lookSensitivity);
	TEST_ASSERT_EQUAL_INT(SETTINGS_BRIGHTNESS_MIN, settings.brightness);
}

/* --- writing ------------------------------------------------------------- */

static void test_what_is_saved_is_what_is_loaded_back(void)
{
	settings.brightness=-4;
	settings.sfxVolume=90;
	settings.lookSensitivity=175;
	settings.invertLookY=true;

	TEST_ASSERT_TRUE(saveSettingsFile(tempPath()));

	defaultSettings(&settings);
	TEST_ASSERT_TRUE(loadSettingsFile(tempPath()));

	TEST_ASSERT_EQUAL_INT(-4, settings.brightness);
	TEST_ASSERT_EQUAL_INT(90, settings.sfxVolume);
	TEST_ASSERT_EQUAL_INT(175, settings.lookSensitivity);
	TEST_ASSERT_TRUE(settings.invertLookY);
}

static void test_saving_over_a_file_replaces_the_old_values(void)
{
	writeConfig("[audio]\nsfx_volume = 33\n");

	settings.sfxVolume=90;
	TEST_ASSERT_TRUE(saveSettingsFile(tempPath()));

	TEST_ASSERT_TRUE(loadSettingsFile(tempPath()));
	TEST_ASSERT_EQUAL_INT(90, settings.sfxVolume);

	//and the old value is gone from the file, not merely shadowed by a second
	//copy of the key further down it
	TEST_ASSERT_NULL(strstr(readConfig(), "33"));
}

/*
 * The one that matters: controls.c owns [controls] and settings.c has never
 * heard of it, but they share the file.
 *
 * The keys come back lower case because that is what iniparser stores them as
 * - it lower cases section and key names on the way in, though not values.
 * actionByString() compares values, so the mapping still reads back.
 */
static void test_saving_keeps_the_button_mapping(void)
{
	writeConfig("[controls]\n"
	            "INPUT_A = CONTROL_JUMP\n"
	            "INPUT_B = CONTROL_USE\n"
	            "[audio]\nsfx_volume = 10\n");

	settings.sfxVolume=90;
	TEST_ASSERT_TRUE(saveSettingsFile(tempPath()));

	const char* written=readConfig();
	TEST_ASSERT_NOT_NULL(strstr(written, "[controls]"));
	TEST_ASSERT_NOT_NULL(strstr(written, "input_a"));
	TEST_ASSERT_NOT_NULL(strstr(written, "CONTROL_JUMP"));
	TEST_ASSERT_NOT_NULL(strstr(written, "input_b"));
	TEST_ASSERT_NOT_NULL(strstr(written, "CONTROL_USE"));
}

static void test_saving_with_no_file_there_writes_one(void)
{
	settings.lookSensitivity=50;

	TEST_ASSERT_TRUE(saveSettingsFile(tempPath()));

	defaultSettings(&settings);
	TEST_ASSERT_TRUE(loadSettingsFile(tempPath()));
	TEST_ASSERT_EQUAL_INT(50, settings.lookSensitivity);
}

static void test_saving_to_a_path_that_cannot_be_written_fails(void)
{
	TEST_ASSERT_FALSE(saveSettingsFile("/tmp/portalds_no_such_dir/config.ini"));
	TEST_ASSERT_FALSE(saveSettingsFile(NULL));
}

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_a_missing_file_leaves_the_defaults);
	RUN_TEST(test_a_null_path_leaves_the_defaults);
	RUN_TEST(test_a_key_the_file_does_not_have_keeps_its_default);

	RUN_TEST(test_every_setting_is_read);
	RUN_TEST(test_a_value_that_is_not_a_number_falls_back_to_the_default);

	RUN_TEST(test_a_value_above_its_range_is_clamped);
	RUN_TEST(test_a_value_below_its_range_is_clamped);

	RUN_TEST(test_what_is_saved_is_what_is_loaded_back);
	RUN_TEST(test_saving_over_a_file_replaces_the_old_values);
	RUN_TEST(test_saving_keeps_the_button_mapping);
	RUN_TEST(test_saving_with_no_file_there_writes_one);
	RUN_TEST(test_saving_to_a_path_that_cannot_be_written_fails);

	return UNITY_END();
}
