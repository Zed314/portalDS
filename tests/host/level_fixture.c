/*
 * Implements level_fixture.h - see there for what this is standing in for.
 *
 * Every function below is a stub for something that lives in an entity pool
 * on the ARM9. They return static storage rather than NULL so that the reader
 * takes its longer path, and they count their calls so that a test can assert
 * on what a file produced.
 */

#include <math.h>

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
platform_struct platform[NUMPLATFORMS];
wallDoor_struct entryWallDoor;
wallDoor_struct exitWallDoor;
bool isNextRoom;

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
	basePath = (char*)""; //owned by files.c, which is linked in for real

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

	levelFixtureCellClear();
	//portal1/portal2 are owned by game/portals.c, which is linked in for real
	memset(&portal1, 0, sizeof(portal1));
	memset(&portal2, 0, sizeof(portal2));
	memset(platform, 0, sizeof(platform));

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

/* --- the collision grid game/physics.c walks ----------------------------- */

#define MAX_CELL_RECTANGLES 32

static gridCell_struct theCell;
static rectangle_struct cellRectangles[MAX_CELL_RECTANGLES];
static rectangle_struct* cellRectanglePointers[MAX_CELL_RECTANGLES];
static bool cellDetached;

void levelFixtureCellClear(void)
{
	memset(&theCell, 0, sizeof(theCell));
	memset(cellRectangles, 0, sizeof(cellRectangles));
	memset(cellRectanglePointers, 0, sizeof(cellRectanglePointers));
	theCell.rectangles = cellRectanglePointers;
	cellDetached = false;
}

rectangle_struct* levelFixtureCellAdd(vect3D position, vect3D size, vect3D normal)
{
	if(theCell.numRectangles >= MAX_CELL_RECTANGLES)return NULL;

	rectangle_struct* rec = &cellRectangles[theCell.numRectangles];
	memset(rec, 0, sizeof(*rec));
	rec->position = position;
	rec->size = size;
	rec->normal = normal;
	rec->collides = true;
	rec->AARid = -1;

	cellRectanglePointers[theCell.numRectangles] = rec;
	theCell.numRectangles++;
	return rec;
}

void levelFixtureCellDetach(void)
{
	cellDetached = true;
}

gridCell_struct* getCurrentCell(room_struct* r, vect3D o)
{
	(void)r;(void)o;
	return cellDetached ? NULL : &theCell;
}

/* --- the rest of the collision world ------------------------------------- */

/*
 * physics.c consults the portals so a surface with a portal on it stops
 * pushing the player out - that is what lets you walk through. The real
 * version lives in game/player.c and needs the portal display lists; here it
 * is counted rather than modelled, so a test can check the reader asks
 * without pretending to answer.
 */
void collidePortal(room_struct* r, rectangle_struct* rec, portal_struct* p, vect3D* point)
{
	(void)r;(void)rec;(void)p;(void)point;
	levelFixtureCounts.portalCollisions++;
}

bool checkObjectTimedButtonsCollision(physicsObject_struct* o, room_struct* r)
{
	(void)o;(void)r;
	levelFixtureCounts.timedButtonChecks++;
	return false;
}

void closeElevator(elevator_struct* ev)
{
	(void)ev;
	levelFixtureCounts.elevatorsClosed++;
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

/* --- what game/portals.c leans on --------------------------------------- */

/*
 * Portal placement is geometry; everything below it is rendering, the camera,
 * and the bridge to the ARM7. None of that decides whether a portal may go
 * somewhere, so it is all stubbed and the placement maths is the real thing.
 */
u32 debugVal;
s16 gravityGunTarget;
u16 mainScreen[256*192];

static camera_struct thePlayerCamera;
static polygon_struct* thePolygon;

polygon_struct* createEllipse(vect3D po, vect3D v1, vect3D v2, int n)
{ (void)po;(void)v1;(void)v2;(void)n; return thePolygon; }

polygon_struct* createEllipseOutline(vect3D po, vect3D v1_1, vect3D v2_1, vect3D v1_2, vect3D v2_2, vect3D norm, int n)
{ (void)po;(void)v1_1;(void)v2_1;(void)v1_2;(void)v2_2;(void)norm;(void)n; return thePolygon; }

void drawPolygon(polygon_struct* p) { (void)p; }
void drawPolygonStrip(polygon_struct* p, u16 c1, u16 c2) { (void)p;(void)c1;(void)c2; }
void projectPolygon(camera_struct* c, polygon_struct** p)
{ (void)c;(void)p; }
void initPolygonPool(void) {}
void freePolygon(polygon_struct** p) { if(p)*p=NULL; }

u32* generateRoomDisplayList(room_struct* r, vect3D pos, vect3D normal, bool cull)
{ (void)r;(void)pos;(void)normal;(void)cull; return NULL; }

camera_struct* getPlayerCamera(void) { return &thePlayerCamera; }
vect3D getViewPosition(vect3D p) { return p; }
vect3D reverseViewPosition(vect3D p) { return p; }
void initCamera(camera_struct* c) { if(c)memset(c, 0, sizeof(*c)); }
void updateFrustum(camera_struct* c) { (void)c; }
void updateViewMatrix(camera_struct* c) { (void)c; }

void updatePortalPI(u8 id, vect3D pos, vect3D normal, vect3D plane0)
{ (void)id;(void)pos;(void)normal;(void)plane0; }
void resetPortalsPI(void) {}

void glMatrixMode(GL_MATRIX_MODE_ENUM m) { (void)m; }
void glLoadIdentity(void) {}
void glPushMatrix(void) {}
void glPopMatrix(s32 n) { (void)n; }
void glScalef32(int32 x, int32 y, int32 z) { (void)x;(void)y;(void)z; }
void glTranslate3f32(int32 x, int32 y, int32 z) { (void)x;(void)y;(void)z; }
void glOrthof32(int32 l, int32 r, int32 b, int32 t, int32 n, int32 f)
{ (void)l;(void)r;(void)b;(void)t;(void)n;(void)f; }
void glCallList(const u32* l) { (void)l; }
void dmaCopy(const void* src, void* dst, u32 size) { (void)src;(void)dst;(void)size; }

/*
 * The libnds maths the ARM9 headers call out to. Real implementations, not
 * stubs - see the note in nds.h.
 */
/*
 * The filesystem bring-up files.c does at boot. bufferizeFile itself is real -
 * it is plain stdio and works here - so test_pcx can hand the decoder actual
 * files. Only the cartridge and NitroFS calls around it need standing in for.
 */
char* fatGetDefaultCwd(void) { return (char*)"."; }
bool fatInitDefault(void) { return false; }
bool nitroFSInit(char** basepath) { (void)basepath; return false; }
void nocashMessage(const char* msg) { (void)msg; }

void normalizef32(void* a)
{
	int32* v = (int32*)a;
	const int32 len = (int32)sqrt64((int64)v[0]*v[0] + (int64)v[1]*v[1] + (int64)v[2]*v[2]);
	if(!len)return;
	v[0] = divf32(v[0], len);
	v[1] = divf32(v[1], len);
	v[2] = divf32(v[2], len);
}

void crossf32(int32* a, int32* b, int32* result)
{
	result[0] = mulf32(a[1], b[2]) - mulf32(a[2], b[1]);
	result[1] = mulf32(a[2], b[0]) - mulf32(a[0], b[2]);
	result[2] = mulf32(a[0], b[1]) - mulf32(a[1], b[0]);
}

int32 cosLerp(int16 angle)
{
	return (int32)lround(cos((double)angle * 2.0 * M_PI / 32768.0) * 4096.0);
}

int32 sinLerp(int16 angle)
{
	return (int32)lround(sin((double)angle * 2.0 * M_PI / 32768.0) * 4096.0);
}
void setBrightness(int screen, int level) { (void)screen;(void)level; }
void swiWaitForVBlank(void) {}
