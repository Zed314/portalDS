/*
 * Implements level_fixture.h - see there for what this is standing in for.
 *
 * Every function below is a stub for something that lives in an entity pool
 * on the ARM9. They return static storage rather than NULL so that the reader
 * takes its longer path, and they count their calls so that a test can assert
 * on what a file produced.
 */

#include "level_fixture.h"

levelFixtureCounts_struct levelFixtureCounts;
createdEntity_struct levelFixtureCreated[MAX_CREATED];
int levelFixtureCreatedCount;

/* Records one create* call. Silently stops recording past MAX_CREATED rather
 * than growing: the tests that go that far are the ones checking that a bogus
 * count is capped well below it, and a fixture that overflowed while proving
 * the reader does not would be its own joke. */
static createdEntity_struct* record(createdKind_type kind)
{
	static createdEntity_struct discard;

	levelFixtureCounts.entities++;
	if(levelFixtureCreatedCount >= MAX_CREATED)
	{
		memset(&discard, 0, sizeof(discard));
		return &discard;
	}

	createdEntity_struct* e = &levelFixtureCreated[levelFixtureCreatedCount++];
	memset(e, 0, sizeof(*e));
	e->kind = kind;
	return e;
}

/* Reader-visible game globals. */
room_struct gameRoom;
wallDoor_struct entryWallDoor;
wallDoor_struct exitWallDoor;
bool isNextRoom;
char* basePath = (char*)"";

/* Geometry engine registers; see the note in nds.h. */
u32 GFX_PAL_FORMAT, GFX_TEX_FORMAT, GFX_COLOR;

/* Storage handed back by the create* stubs. One of each is enough: the tests
 * care how many times the reader asked, not what it got. */
static energyDevice_struct   theEnergyDevice;
static timedButton_struct    theTimedButton;
static bigButton_struct      theBigButton;
static cubeDispenser_struct  theCubeDispenser;
static platform_struct       thePlatform;
static door_struct           theDoor;
static turret_struct         theTurret;
static light_struct          theLight;
static material_struct       theMaterial;
static physicsObject_struct  thePlayerObject;
static player_struct         thePlayer;

void levelFixtureReset(void)
{
	memset(&levelFixtureCounts, 0, sizeof(levelFixtureCounts));
	memset(levelFixtureCreated, 0, sizeof(levelFixtureCreated));
	levelFixtureCreatedCount = 0;

	levelFixtureFreeRectangles(&gameRoom);

	memset(&entryWallDoor, 0, sizeof(entryWallDoor));
	memset(&exitWallDoor, 0, sizeof(exitWallDoor));
	isNextRoom = false;

	memset(&theEnergyDevice, 0, sizeof(theEnergyDevice));
	memset(&theTimedButton, 0, sizeof(theTimedButton));
	memset(&theBigButton, 0, sizeof(theBigButton));
	memset(&theCubeDispenser, 0, sizeof(theCubeDispenser));
	memset(&thePlatform, 0, sizeof(thePlatform));
	memset(&theDoor, 0, sizeof(theDoor));
	memset(&theTurret, 0, sizeof(theTurret));
	memset(&theLight, 0, sizeof(theLight));
	memset(&theMaterial, 0, sizeof(theMaterial));

	memset(&thePlayerObject, 0, sizeof(thePlayerObject));
	memset(&thePlayer, 0, sizeof(thePlayer));
	thePlayer.object = &thePlayerObject;

	memset(entityTargetArray, 0, sizeof(entityTargetArray));
	memset(entityActivatorArray, 0, sizeof(entityActivatorArray));
	memset(entityEntityArray, 0, sizeof(entityEntityArray));
	memset(entityTargetTypeArray, 0, sizeof(entityTargetTypeArray));

	setLevelInfo(NULL, NULL);
}

/* --- entity pools -------------------------------------------------------- */

energyDevice_struct* createEnergyDevice(room_struct* r, vect3D pos, deviceOrientation_type or, bool type)
{
	(void)r;
	createdEntity_struct* e = record(CREATED_ENERGY_DEVICE);
	e->pos = pos;
	e->param = or;
	e->flag = type;
	return &theEnergyDevice;
}

timedButton_struct* createTimedButton(room_struct* r, vect3D position, u16 angle)
{
	(void)r;
	createdEntity_struct* e = record(CREATED_TIMED_BUTTON);
	e->pos = position;
	e->param = angle;
	return &theTimedButton;
}

bigButton_struct* createBigButton(room_struct* r, vect3D position)
{
	(void)r;
	record(CREATED_BIG_BUTTON)->pos = position;
	return &theBigButton;
}

cubeDispenser_struct* createCubeDispenser(room_struct* r, vect3D pos, bool companion)
{
	(void)r;
	createdEntity_struct* e = record(CREATED_CUBE_DISPENSER);
	e->pos = pos;
	e->flag = companion;
	return &theCubeDispenser;
}

platform_struct* createPlatform(room_struct* r, vect3D orig, vect3D dest, bool BAF)
{
	(void)r;
	createdEntity_struct* e = record(CREATED_PLATFORM);
	e->pos = orig;
	e->pos2 = dest;
	e->flag = BAF;
	return &thePlatform;
}

door_struct* createDoor(room_struct* r, vect3D position, bool orientation)
{
	(void)r;
	createdEntity_struct* e = record(CREATED_DOOR);
	e->pos = position;
	e->param = orientation;
	return &theDoor;
}

turret_struct* createTurret(room_struct* r, vect3D position, u8 d)
{
	(void)r;
	createdEntity_struct* e = record(CREATED_TURRET);
	e->pos = position;
	e->param = d;
	levelFixtureCounts.turrets++;
	return &theTurret;
}

light_struct* createLight(vect3D pos, int32 intensity)
{
	createdEntity_struct* e = record(CREATED_LIGHT);
	e->pos = pos;
	e->param = intensity;
	levelFixtureCounts.lights++;
	return &theLight;
}

void createEmancipationGrid(room_struct* r, vect3D pos, int32 l, bool dir)
{
	(void)r;
	createdEntity_struct* e = record(CREATED_GRID);
	e->pos = pos;
	e->param = l;
	e->flag = dir;
}

void setupWallDoor(room_struct* r, wallDoor_struct* wd, vect3D position, u8 orientation)
{
	(void)r;
	if(!wd)return;
	createdEntity_struct* e = record(CREATED_WALLDOOR);
	e->pos = position;
	e->param = orientation;
	e->flag = (wd == &exitWallDoor);
	wd->position = position;
	wd->orientation = orientation;
	/* Left true so the reader takes its longer branch: repositioning the
	 * player and the camera onto the arrival lift. */
	wd->used = true;
}

void addActivatorTarget(activator_struct* a, void* target, activatorTarget_type type)
{
	(void)a;(void)target;(void)type;
	levelFixtureCounts.activatorTargets++;
}

/* --- room geometry ------------------------------------------------------- */

void initRoom(room_struct* r, u16 w, u16 h, vect3D p)
{
	if(!r)return;
	memset(r, 0, sizeof(*r));
	r->width = w;
	r->height = h;
	r->position = p;
}

rectangle_struct* addRoomRectangle(room_struct* r, rectangle_struct rec, material_struct* mat, bool portalable)
{
	levelFixtureCounts.rectangles++;
	if(!r)return NULL;

	rec.material = mat;
	rec.portalable = portalable;

	/* Push at the front, as the editor's list does. The reader depends on
	 * this: it pairs rectangles with lighting by walking the list forwards
	 * against an index counting backwards. */
	listCell_struct* cell = malloc(sizeof(listCell_struct));
	if(!cell)return NULL;
	cell->data = rec;
	cell->next = r->rectangles.first;
	r->rectangles.first = cell;
	r->rectangles.num++;

	return &cell->data;
}

rectangle_struct* levelFixturePushRectangle(room_struct* r, vect3D position, vect3D size, vect3D normal)
{
	rectangle_struct rec;
	memset(&rec, 0, sizeof(rec));
	rec.position = position;
	rec.size = size;
	rec.normal = normal;
	return addRoomRectangle(r, rec, &theMaterial, false);
}

void levelFixtureFreeRectangles(room_struct* r)
{
	if(!r)return;
	listCell_struct* lc = r->rectangles.first;
	while(lc)
	{
		listCell_struct* next = lc->next;
		free(lc);
		lc = next;
	}
	r->rectangles.first = NULL;
	r->rectangles.num = 0;
}

void addSludgeRectangle(rectangle_struct* rec)
{
	(void)rec;
	levelFixtureCounts.rectangles++;
	levelFixtureCounts.sludgeRectangles++;
}

material_struct* getMaterial(u16 i) { (void)i; return &theMaterial; }

void initLightDataVL(lightingData_struct* ld, u16 n)
{
	if(!ld)return;
	ld->size = n;
	ld->type = VERTEXLIGHT_DATA;
	ld->data.vertexLighting = n ? calloc(n, sizeof(vertexLightingData_struct)) : NULL;
}

void initLightDataLM(lightingData_struct* ld, u16 n)
{
	if(!ld)return;
	ld->size = n;
	ld->type = LIGHTMAP_DATA;
}

/* --- things the reader touches but the host cannot have ------------------ */

player_struct* getPlayer(void) { return &thePlayer; }

void moveCamera(camera_struct* c, vect3D v) { (void)c;(void)v; }
void rotateCamera(camera_struct* c, vect3D a) { (void)c;(void)a; }

void drawRoom(room_struct* r, u8 mode, u16 color) { (void)r;(void)mode;(void)color; }
void unbindMtl(void) {}
void glPolyFmt(u32 params) { (void)params; }

mtlImg_struct* createReservedTextureBufferA5I3(u8* buffer, u16* buffer2, u16 x, u16 y, void* addr)
{
	(void)buffer;(void)buffer2;(void)x;(void)y;(void)addr;
	return NULL;
}

void setNextMapFilePath(char* path) { (void)path; }

/* From editor/io.c, which is not part of this suite. The header is a flat
 * struct of offsets, so reading it is a single fread. */
void readHeader(mapHeader_struct* h, FILE* f)
{
	if(!h || !f)return;
	fseek(f, 0, SEEK_SET);
	if(fread(h, MAPHEADER_SIZE, 1, f)!=1)memset(h, 0, sizeof(*h));
}

void normalizef32(void* a) { (void)a; }
void crossf32(int32* a, int32* b, int32* result) { (void)a;(void)b;(void)result; }
void setBrightness(int screen, int level) { (void)screen;(void)level; }
void swiWaitForVBlank(void) {}
