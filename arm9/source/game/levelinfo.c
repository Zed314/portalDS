/**
 * @file levelinfo.c
 * @brief The title and author banner shown when a level starts.
 *
 * Split out of game.c so it can be exercised on the host: this is the only
 * part of the level metadata path that is pure string handling, and it sits
 * directly on untrusted input. @ref setLevelInfo is fed by readMapInfo() in
 * game/room.c straight from the level's @c .ini, so both arguments are as long
 * as whoever wrote the map felt like - the ini parser will hand back values of
 * up to @c ASCIILINESZ bytes.
 *
 * Both buffers are therefore fixed size and both copies are bounded. Anything
 * that does not fit is truncated rather than rejected; the banner these feed
 * is drawn with drawCenteredString() and has no room for more than this
 * anyway.
 *
 * @see tests/suites/test_levelfile.c
 */

#include "game/game_main.h"

char levelTitle[LEVELINFOCHARS];
char levelAuthor[LEVELINFOCHARS];

void setLevelInfo(char* title, char* author)
{
	levelTitle[0]='\0';
	levelAuthor[0]='\0';

	if(title)snprintf(levelTitle, sizeof(levelTitle), "%s", title);
	if(author)snprintf(levelAuthor, sizeof(levelAuthor), "by %s", author);
}
