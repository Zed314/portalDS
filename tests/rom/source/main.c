/*
 * portalDS FIFO round trip tests.
 *
 * This is the ARM9 half of a test ROM. The ARM7 half is the real, unmodified
 * arm7.elf - the actual physics engine and the actual FIFO command decoder in
 * PI7.c. So the thing under test is the contract between the two processors:
 * command encoding, argument order, the reply packing, and the fact that the
 * simulation on the far side really does what the near side asked for.
 *
 * That contract is the one part of portalDS a host test cannot reach. Both
 * halves have to be running, on their own CPUs, over a real FIFO, in the right
 * order - so this runs under an emulator. See tests/rom/README.md.
 *
 * The driver deliberately re-implements the sender rather than linking PI9.c:
 * a test that shares its encoder with the decoder cannot catch the two
 * drifting apart. See fifo_protocol.h.
 *
 * Results leave the emulator one byte at a time through dsprobe.h; the harness
 * reassembles them into the text below.
 */

#include <nds.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dsprobe.h"
#include "fifo_protocol.h"

/* --- sending commands ----------------------------------------------------- */

static void cmd(u32 opcode, u32 id)
{
    fifoSendValue32(FIFO_USER_08, opcode | (id << PI_SIGNAL_BITS));
}

static void arg(u32 word)
{
    fifoSendValue32(FIFO_USER_08, word);
}

/* PI_ADDBOX: id; [sizex|sizey][sizez|mass][posx][posy][posz][cosine][sine] */
static void addBox(u32 id, s32 sx, s32 sy, s32 sz, s32 mass,
                   s32 px, s32 py, s32 pz, s32 cosine, s32 sine)
{
    cmd(CMD_ADDBOX, id);
    arg((sx & 0xFFFF) | ((u32)(sy & 0xFFFF) << 16));
    arg((sz & 0xFFFF) | ((u32)(mass & 0xFFFF) << 16));
    arg(px);
    arg(py);
    arg(pz);
    arg(cosine);
    arg(sine);
}

/* PI_ADDAAR: id; [sizex][sizey][sizez][normal][posx][posy][posz] */
static void addAAR(u32 id, s32 sx, s32 sy, s32 sz, u32 normalBits,
                   s32 px, s32 py, s32 pz)
{
    cmd(CMD_ADDAAR, id);
    arg(sx); arg(sy); arg(sz);
    arg(normalBits);
    arg(px); arg(py); arg(pz);
}

/* PI_SETVELOCITY: id; [vx][vy][vz] */
static void setVelocity(u32 id, s32 vx, s32 vy, s32 vz)
{
    cmd(CMD_SETVELOCITY, id);
    arg(vx); arg(vy); arg(vz);
}

/* PI_APPLYFORCE: id; [posx|posy][posz][vx][vy][vz] - the offset is s16. */
static void applyForce(u32 id, s16 ox, s16 oy, s16 oz, s32 vx, s32 vy, s32 vz)
{
    cmd(CMD_APPLYFORCE, id);
    arg((u32)(ox & 0xFFFF) | ((u32)(oy & 0xFFFF) << 16));
    arg((u32)(oz & 0xFFFF));
    arg(vx); arg(vy); arg(vz);
}

/* PI_ADDPLATFORM: id; [origx][origy][origz][destx][desty][destz]
 * An id at or above NUM_PLATFORM_SLOTS selects a back-and-forth platform; the
 * slot itself is the id modulo the pool size. */
static void addPlatform(u32 id, s32 ox, s32 oy, s32 oz, s32 dx, s32 dy, s32 dz)
{
    cmd(CMD_ADDPLATFORM, id);
    arg(ox); arg(oy); arg(oz);
    arg(dx); arg(dy); arg(dz);
}

/* PI_UPDATEPORTAL: id; [px][py][pz][normal][p0x][p0y][p0z]
 *
 * The ARM7 echoes the tangent back on the command channel, so the caller has
 * to drain three words afterwards or they pile up unread. */
static void updatePortal(u32 id, s32 px, s32 py, s32 pz, u32 normalBits,
                         s32 t0x, s32 t0y, s32 t0z)
{
    cmd(CMD_UPDATEPORTAL, id);
    arg(px); arg(py); arg(pz);
    arg(normalBits);
    arg(t0x); arg(t0y); arg(t0z);
}

static void drainPortalEcho(void)
{
    for (int i = 0; i < 3; i++)
    {
        int spins = 0;
        while (!fifoCheckValue32(FIFO_USER_08) && spins < 100000)
            spins++;
        if (fifoCheckValue32(FIFO_USER_08))
            fifoGetValue32(FIFO_USER_08);
    }
}

/* --- receiving replies ---------------------------------------------------- */

typedef struct
{
    bool seen;
    s32 x, y, z;
    s32 m[9];      /* only the six transmitted elements are filled in */
    s16 groundID;
    bool portaled; /* sticky: set once seen, so a single frame is not missed */
    int updates;   /* how many reports arrived since the last clear */
} report_t;

static report_t reports[NUM_BODIES];

/* Platforms report on the same channel with an id at or above NUM_BODIES, and
 * carry a position only. */
#define NUM_PLATFORM_SLOTS (8)
static report_t platformReports[NUM_PLATFORM_SLOTS];

static void clearReports(void)
{
    memset(reports, 0, sizeof(reports));
    memset(platformReports, 0, sizeof(platformReports));
}

/*
 * Drains one frame's worth of replies. Mirrors the decode in listenPI9(),
 * including the platform reports that carry only three words rather than six.
 */
static void drainReports(void)
{
    while (fifoCheckValue32(FIFO_USER_01))
    {
        const u32 h = fifoGetValue32(FIFO_USER_01);
        const int k = h & 0xFFFF;

        while (!fifoCheckValue32(FIFO_USER_02));
        while (!fifoCheckValue32(FIFO_USER_03));
        while (!fifoCheckValue32(FIFO_USER_04));

        const s32 px = fifoGetValue32(FIFO_USER_02);
        const s32 py = fifoGetValue32(FIFO_USER_03);
        const s32 pz = fifoGetValue32(FIFO_USER_04);

        if (k >= NUM_BODIES)
        {
            /* A platform: position only, nothing more to read. */
            const int slot = k - NUM_BODIES;
            if (slot < NUM_PLATFORM_SLOTS)
            {
                report_t *p = &platformReports[slot];
                p->seen = true;
                p->updates++;
                p->x = px; p->y = py; p->z = pz;
            }
            continue;
        }

        while (!fifoCheckValue32(FIFO_USER_05));
        while (!fifoCheckValue32(FIFO_USER_06));
        while (!fifoCheckValue32(FIFO_USER_07));

        report_t *r = &reports[k];
        r->seen = true;
        r->updates++;
        r->x = px; r->y = py; r->z = pz;
        r->groundID = (s16)((h >> 17) - 1);
        if ((h >> 16) & 1)
            r->portaled = true;

        s32 w = fifoGetValue32(FIFO_USER_05);
        r->m[0] = (s16)w - F32_ONE;
        r->m[3] = (s16)(w >> 16) - F32_ONE;
        w = fifoGetValue32(FIFO_USER_06);
        r->m[6] = (s16)w - F32_ONE;
        r->m[1] = (s16)(w >> 16) - F32_ONE;
        w = fifoGetValue32(FIFO_USER_07);
        r->m[4] = (s16)w - F32_ONE;
        r->m[7] = (s16)(w >> 16) - F32_ONE;
    }
}

static void stepFrames(int n)
{
    for (int i = 0; i < n; i++)
    {
        swiWaitForVBlank();
        drainReports();
    }
}

/*
 * Steps until a body is reported, or gives up.
 *
 * The first reply after a burst of commands is not immediate: the words are
 * queued on this side, shifted across by interrupt, drained by the ARM7's own
 * loop, and only then does a frame's worth of results come back. That is
 * several frames, and it varies. Waiting for the reply rather than guessing a
 * frame count is what keeps these tests from being timing flakes.
 */
static bool waitForReport(int id, int maxFrames)
{
    for (int i = 0; i < maxFrames && !reports[id].seen; i++)
    {
        swiWaitForVBlank();
        drainReports();
    }
    return reports[id].seen;
}

/* Frames to allow for a reply before calling it a failure. */
#define REPLY_TIMEOUT (60)

/* --- assertions ----------------------------------------------------------- */

static int checks, failures;

__attribute__((format(printf, 1, 2)))
static void failf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    probeStr("       ");
    probeStr(buf);
    probeStr("\n");
    failures++;
}

static void checkTrue(bool cond, const char *what)
{
    checks++;
    if (!cond)
        failf("%s", what);
}

static void checkNear(s32 expected, s32 actual, s32 tol, const char *what)
{
    checks++;
    const s32 d = expected > actual ? expected - actual : actual - expected;
    if (d > tol)
        failf("%s: expected %ld +/-%ld, got %ld", what, (long)expected, (long)tol, (long)actual);
}

/* --- world setup shared by most tests ------------------------------------- */

/* Wipes all ARM7 state and lays down a wide floor at y=0, with a grid. */
static void resetWorldWithFloor(void)
{
    const s32 h = INT_TO_F32(16);

    cmd(CMD_RESETALL, 0);
    addAAR(0, h * 2, 0, h * 2, AAR_NORMAL_POS_Y, -h, 0, -h);
    cmd(CMD_MAKEGRID, 0);
    cmd(CMD_START, 0);
    clearReports();
}

static void resetWorldEmpty(void)
{
    cmd(CMD_RESETALL, 0);
    cmd(CMD_START, 0);
    clearReports();
}

/* A one unit half-extent cube of mass 1, unrotated. */
static void addUnitCube(u32 id, s32 px, s32 py, s32 pz)
{
    addBox(id, INT_TO_F32(1), INT_TO_F32(1), INT_TO_F32(1), INT_TO_F32(1),
           px, py, pz, F32_ONE, 0);
}

/* --- the tests ------------------------------------------------------------ */

static void test_a_body_is_reported_back(void)
{
    resetWorldWithFloor();
    addUnitCube(0, INT_TO_F32(2), INT_TO_F32(8), INT_TO_F32(3));

    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived for the body we created");
    /* Gravity only moves it downwards, so x and z are the exact check and y
     * gets a unit of slack for however many frames the reply took. */
    checkNear(INT_TO_F32(2), reports[0].x, 4, "reported x");
    checkNear(INT_TO_F32(3), reports[0].z, 4, "reported z");
    checkNear(INT_TO_F32(8), reports[0].y, INT_TO_F32(1), "reported y");
}

static void test_an_unrotated_body_reports_the_identity(void)
{
    resetWorldWithFloor();
    addUnitCube(0, 0, INT_TO_F32(8), 0);

    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");
    checkNear(F32_ONE, reports[0].m[0], 8, "m[0]");
    checkNear(0, reports[0].m[3], 8, "m[3]");
    checkNear(0, reports[0].m[1], 8, "m[1]");
    checkNear(F32_ONE, reports[0].m[4], 8, "m[4]");
    checkNear(0, reports[0].m[6], 8, "m[6]");
    checkNear(0, reports[0].m[7], 8, "m[7]");
}

static void test_a_rotated_body_reports_its_yaw(void)
{
    /*
     * initTransformationMatrix puts the yaw in m[0]=cos and m[6]=sin, both of
     * which are transmitted. This is the check on the whole orientation path:
     * the cosine and sine we send, through createOBB, back through the packed
     * reply words.
     */
    const s32 c = 2896, s = 2896; /* cos/sin of 45 degrees in f32 */

    resetWorldWithFloor();
    addBox(0, INT_TO_F32(1), INT_TO_F32(1), INT_TO_F32(1), INT_TO_F32(1),
           0, INT_TO_F32(8), 0, c, s);

    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");
    checkNear(c, reports[0].m[0], 32, "m[0] should be cos(yaw)");
    checkNear(s, reports[0].m[6], 32, "m[6] should be sin(yaw)");
}

static void test_the_last_argument_word_is_waited_for(void)
{
    /*
     * Every argument word of PI_ADDBOX is guarded by its own
     * "while(!fifoCheckValue32(...))" on the ARM7 - except the last one, the
     * sine, which is read straight after the cosine with no wait of its own.
     *
     * On hardware the ARM9 usually writes all seven words in one burst, so the
     * word is already there and nothing goes wrong. Here the send is split
     * deliberately: six words, a frame's pause, then the seventh. If the
     * receiver does not wait, it reads an empty queue, the yaw comes back as
     * zero, and the box is silently created facing the wrong way.
     */
    const s32 c = 2896, s = 2896; /* 45 degrees */

    resetWorldWithFloor();
    /* Let the ARM7 finish the setup commands first. Otherwise it is still
     * working through those when the split send goes out, catches up during
     * the pause, and the last word is waiting for it after all. */
    stepFrames(20);

    cmd(CMD_ADDBOX, 0);
    arg((INT_TO_F32(1) & 0xFFFF) | ((u32)(INT_TO_F32(1) & 0xFFFF) << 16));
    arg((INT_TO_F32(1) & 0xFFFF) | ((u32)(INT_TO_F32(1) & 0xFFFF) << 16));
    arg(0);
    arg(INT_TO_F32(8));
    arg(0);
    arg(c);
    /* Give the ARM7 several frames to consume everything up to and including
     * the cosine, so that the read of the sine genuinely finds an empty queue.
     * fifoGetValue32 does not block - libnds documents it as returning 0 when
     * there is no message - so an unguarded read silently yields zero. */
    stepFrames(5);
    arg(s);

    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");
    checkNear(c, reports[0].m[0], 32, "m[0] after a split send");
    checkNear(s, reports[0].m[6], 32, "m[6] after a split send");
}

static void test_a_body_falls(void)
{
    resetWorldEmpty();
    addUnitCube(0, 0, INT_TO_F32(40), 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");
    const s32 startY = reports[0].y;

    stepFrames(40);

    checkTrue(reports[0].y < startY, "the body did not fall");
}

static void test_a_body_lands_on_the_floor(void)
{
    /*
     * The full pipeline in one go: rectangle, broadphase, body, simulation,
     * reply. The ARM7 runs five substeps per frame, so this is around 650
     * solver steps - enough for the bounce to damp out.
     */
    resetWorldWithFloor();
    addUnitCube(0, 0, INT_TO_F32(3), 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");

    stepFrames(130);

    /* Half height is 1, and the solver allows a little sink. */
    checkNear(INT_TO_F32(1), reports[0].y, 700, "resting height");
}

static void test_the_supporting_rectangle_is_reported(void)
{
    /* groundID is how the ARM9 makes a body ride a moving platform. */
    resetWorldWithFloor();
    addUnitCube(0, 0, INT_TO_F32(3), 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");

    stepFrames(130);

    checkTrue(reports[0].groundID == 0, "the floor should be reported as ground");
}

static void test_set_velocity_is_obeyed(void)
{
    resetWorldEmpty();
    addUnitCube(0, 0, 0, 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");
    const s32 startY = reports[0].y;

    setVelocity(0, 0, INT_TO_F32(20), 0);
    stepFrames(10);

    checkTrue(reports[0].y > startY, "an upward velocity did not move the body up");
}

static void test_apply_force_is_obeyed(void)
{
    resetWorldEmpty();
    addUnitCube(0, 0, 0, 0);
    addUnitCube(1, INT_TO_F32(8), 0, 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");
    checkTrue(waitForReport(1, REPLY_TIMEOUT), "no reply arrived for body 1");

    applyForce(0, 0, 0, 0, 0, INT_TO_F32(4000), 0);
    stepFrames(10);

    checkTrue(reports[0].y > reports[1].y,
              "the pushed body should be above the one left to fall");
}

static void test_pausing_stops_the_simulation(void)
{
    resetWorldEmpty();
    addUnitCube(0, 0, INT_TO_F32(40), 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");

    cmd(CMD_PAUSE, 0);
    /* Let any replies already in flight land before deciding it has gone
     * quiet, then note where it stopped. */
    stepFrames(10);
    const s32 beforePause = reports[0].y;
    clearReports();
    stepFrames(20);

    /* sendDataPI7 only runs while the engine is started, so a paused engine
     * transmits nothing at all. */
    checkTrue(!reports[0].seen, "the engine kept transmitting while paused");

    cmd(CMD_START, 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply after resuming");
    stepFrames(20);
    checkTrue(reports[0].y < beforePause, "the body did not resume falling");
}

static void test_killing_a_body_stops_its_reports(void)
{
    resetWorldEmpty();
    addUnitCube(0, 0, INT_TO_F32(40), 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived before the kill");

    cmd(CMD_KILLBOX, 0);
    clearReports();
    stepFrames(10);

    checkTrue(!reports[0].seen, "a killed body is still being reported");
}

static void test_reset_clears_every_body(void)
{
    resetWorldEmpty();
    addUnitCube(0, 0, INT_TO_F32(40), 0);
    addUnitCube(1, INT_TO_F32(4), INT_TO_F32(40), 0);
    waitForReport(0, REPLY_TIMEOUT);
    waitForReport(1, REPLY_TIMEOUT);
    checkTrue(reports[0].seen && reports[1].seen, "bodies were not reported");

    cmd(CMD_RESET, 0);
    clearReports();
    stepFrames(10);

    checkTrue(!reports[0].seen && !reports[1].seen, "bodies survived a reset");
}

static void test_several_bodies_keep_their_identities(void)
{
    /* Ids are packed into the reply header, so a mistake there would show up
     * as two bodies swapping positions. */
    resetWorldEmpty();
    addUnitCube(0, -INT_TO_F32(6), INT_TO_F32(40), 0);
    addUnitCube(1, 0, INT_TO_F32(40), 0);
    addUnitCube(2, INT_TO_F32(6), INT_TO_F32(40), 0);

    waitForReport(0, REPLY_TIMEOUT);
    waitForReport(1, REPLY_TIMEOUT);
    waitForReport(2, REPLY_TIMEOUT);

    checkTrue(reports[0].seen && reports[1].seen && reports[2].seen,
              "not every body was reported");
    checkNear(-INT_TO_F32(6), reports[0].x, 4, "body 0 x");
    checkNear(0, reports[1].x, 4, "body 1 x");
    checkNear(INT_TO_F32(6), reports[2].x, 4, "body 2 x");
}

static void test_a_sleeping_body_stops_being_transmitted(void)
{
    /*
     * Sleeping bodies are skipped by sendDataPI7 - that is what keeps a busy
     * room inside the FIFO's budget. So a settled body should go quiet.
     */
    resetWorldWithFloor();
    addUnitCube(0, 0, INT_TO_F32(3), 0);
    stepFrames(200);

    clearReports();
    stepFrames(20);

    checkTrue(!reports[0].seen, "a settled body is still being transmitted");
}

/* --- a whole level in one burst ------------------------------------------- */

/*
 * Rectangles sent back to back per level load test.
 *
 * A room may hold up to NUMAARS (300) of them, but transferRectangles() does
 * not send them back to back: it waits for a VBlank every eight, which is 64
 * FIFO words per frame. That one line is the only thing keeping a level load
 * inside what the queue can hold, and nothing in the sending path would notice
 * if it were removed - createAAR() ignores fifoSendValue32()'s return value,
 * like every other sender in PI9.c.
 *
 * This sends an unpaced burst on purpose, to show what the pacing is standing
 * between the game and. Past roughly 48 rectangles the stream breaks up here:
 * words are dropped, the ARM7 stalls waiting for ones that never arrive, and
 * nothing comes back at all. See "the FIFO has no backpressure" in README.md.
 *
 * The size below is not asserted anywhere on purpose. Where exactly it breaks
 * is a race between how fast the ARM9 fills the queue and how fast the ARM7
 * drains it, and FIFO timing is the thing this emulator is least likely to
 * reproduce faithfully. So this stays comfortably clear of the cliff and
 * covers the burst path rather than the edge.
 */
#define BURST_RECTS (32)

static void test_a_full_level_load_survives_the_burst(void)
{
    /*
     * A level load is a long run of createAAR() calls, eight FIFO words each,
     * with nothing checked on the way out. If one of those words is dropped the
     * stream desynchronises: every following word is read as the wrong field,
     * and collision geometry silently goes missing. In game that is a wall you
     * fall through in one chamber, with nothing in the logs.
     *
     * The probe is the *last* rectangle of the burst - the one most likely to
     * be lost - and it is the only thing holding the test body up. If a single
     * word went astray, the body falls forever and this fails.
     */
    cmd(CMD_RESETALL, 0);

    /* Filler, spread over the XZ plane so the broadphase stays sensible, and
     * parked far above so it cannot interfere with the fall. */
    for (u32 i = 0; i < BURST_RECTS - 1; i++)
    {
        const s32 x = -INT_TO_F32(16) + (s32)(i % 16) * INT_TO_F32(2);
        const s32 z = -INT_TO_F32(16) + (s32)(i / 16) * INT_TO_F32(2);
        addAAR(i, INT_TO_F32(1), 0, INT_TO_F32(1), AAR_NORMAL_POS_Y,
               x, INT_TO_F32(50), z);
    }

    /* The real floor, sent last. */
    const s32 h = INT_TO_F32(16);
    addAAR(BURST_RECTS - 1, h * 2, 0, h * 2, AAR_NORMAL_POS_Y, -h, 0, -h);

    cmd(CMD_MAKEGRID, 0);
    cmd(CMD_START, 0);
    clearReports();

    addUnitCube(0, 0, INT_TO_F32(3), 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply after a full level load");

    stepFrames(130);

    /* Landed on the last rectangle of the burst. */
    checkNear(INT_TO_F32(1), reports[0].y, 700, "resting height after a level load");
    checkTrue(reports[0].groundID == BURST_RECTS - 1,
              "the last rectangle of the burst is not what it landed on");
}

static void test_the_command_stream_is_still_aligned_after_a_burst(void)
{
    /*
     * A desynchronised stream can still look healthy if the test only checks
     * that something arrived. This sends a burst and then a command whose
     * arguments are easy to verify exactly - a body at a known position - so a
     * stream that slipped by even one word shows up as a wrong coordinate
     * rather than as silence.
     */
    cmd(CMD_RESETALL, 0);
    for (u32 i = 0; i < BURST_RECTS; i++)
    {
        const s32 x = -INT_TO_F32(16) + (s32)(i % 16) * INT_TO_F32(2);
        const s32 z = -INT_TO_F32(16) + (s32)(i / 16) * INT_TO_F32(2);
        addAAR(i, INT_TO_F32(1), 0, INT_TO_F32(1), AAR_NORMAL_POS_Y,
               x, INT_TO_F32(50), z);
    }
    cmd(CMD_MAKEGRID, 0);
    cmd(CMD_START, 0);
    clearReports();

    addUnitCube(0, INT_TO_F32(5), INT_TO_F32(7), -INT_TO_F32(3));
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply after the burst");

    checkNear(INT_TO_F32(5), reports[0].x, 4, "x after a burst");
    checkNear(-INT_TO_F32(3), reports[0].z, 4, "z after a burst");
    checkNear(INT_TO_F32(7), reports[0].y, INT_TO_F32(1), "y after a burst");
}

/* --- platforms ------------------------------------------------------------ */

static void test_platform_commands_round_trip(void)
{
    /*
     * Platforms report on the same channel as bodies but with an id at or above
     * NUM_BODIES and only three words instead of six. A receiver that got that
     * wrong would desynchronise the reply stream the moment a level contained
     * one.
     */
    resetWorldEmpty();
    addPlatform(0, 0, 0, 0, 0, INT_TO_F32(4), 0);

    for (int i = 0; i < REPLY_TIMEOUT && !platformReports[0].seen; i++)
    {
        swiWaitForVBlank();
        drainReports();
    }

    checkTrue(platformReports[0].seen, "no platform reply arrived");
    checkNear(0, platformReports[0].y, 64, "a new platform sits at its origin");

    /* Started, it should climb. */
    cmd(CMD_TOGGLEPLATFORM, 0);
    arg(1);
    stepFrames(30);

    checkTrue(platformReports[0].y > 0, "a started platform did not move");
}

static void test_moving_a_platform_is_reported(void)
{
    resetWorldEmpty();
    addPlatform(0, 0, 0, 0, 0, INT_TO_F32(4), 0);
    for (int i = 0; i < REPLY_TIMEOUT && !platformReports[0].seen; i++)
    {
        swiWaitForVBlank();
        drainReports();
    }
    checkTrue(platformReports[0].seen, "no platform reply arrived");

    cmd(CMD_UPDATEPLATFORM, 0);
    arg(INT_TO_F32(6));
    arg(INT_TO_F32(2));
    arg(-INT_TO_F32(4));
    stepFrames(10);

    checkNear(INT_TO_F32(6), platformReports[0].x, 64, "moved platform x");
    checkNear(INT_TO_F32(2), platformReports[0].y, 64, "moved platform y");
    checkNear(-INT_TO_F32(4), platformReports[0].z, 64, "moved platform z");
}

/* --- portals -------------------------------------------------------------- */

static void test_a_body_travels_through_a_portal_pair(void)
{
    /*
     * Portal transport is the one command that changes a body's position
     * without the solver moving it, and the teleport flag rides back in the
     * spare bit of the reply header. Both are checked here.
     */
    resetWorldEmpty();

    /* Two portals facing up: one under the body, one a long way off. */
    updatePortal(0, 0, 0, 0, AAR_NORMAL_POS_Y, F32_ONE, 0, 0);
    drainPortalEcho();
    updatePortal(1, INT_TO_F32(60), 0, 0, AAR_NORMAL_POS_Y, F32_ONE, 0, 0);
    drainPortalEcho();

    /* Just in front of portal 0, driven down through it. */
    addUnitCube(0, 0, F32_ONE / 4, 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply arrived");
    setVelocity(0, 0, -INT_TO_F32(30), 0);

    stepFrames(40);

    checkTrue(reports[0].portaled, "the body never reported a teleport");
    checkTrue(reports[0].x > INT_TO_F32(40),
              "the body did not come out of the far portal");
}

/* --- robustness ----------------------------------------------------------- */

static void test_an_unknown_opcode_does_not_wedge_the_engine(void)
{
    /*
     * The decoder's default case throws the whole queue away rather than trying
     * to resynchronise, which is the right call - but it does mean a stray word
     * eats whatever was queued behind it. What must not happen is the engine
     * getting stuck, so this checks it still accepts work afterwards.
     */
    resetWorldEmpty();
    addUnitCube(0, 0, INT_TO_F32(40), 0);
    checkTrue(waitForReport(0, REPLY_TIMEOUT), "no reply before the bad opcode");

    /* 40 is not a valid opcode. */
    cmd(40, 0);
    stepFrames(10);

    /* The engine should still be alive and taking commands. */
    clearReports();
    cmd(CMD_RESETALL, 0);
    cmd(CMD_START, 0);
    addUnitCube(1, INT_TO_F32(2), INT_TO_F32(9), 0);

    checkTrue(waitForReport(1, REPLY_TIMEOUT),
              "the engine stopped responding after an unknown opcode");
    checkNear(INT_TO_F32(2), reports[1].x, 4, "x after an unknown opcode");
}

/* --- runner --------------------------------------------------------------- */

typedef struct
{
    const char *name;
    void (*fn)(void);
} testcase_t;

static const testcase_t tests[] = {
    {"a body is reported back",              test_a_body_is_reported_back},
    {"an unrotated body reports identity",   test_an_unrotated_body_reports_the_identity},
    {"a rotated body reports its yaw",       test_a_rotated_body_reports_its_yaw},
    {"the last argument word is waited for", test_the_last_argument_word_is_waited_for},
    {"a body falls",                         test_a_body_falls},
    {"a body lands on the floor",            test_a_body_lands_on_the_floor},
    {"the supporting rectangle is reported", test_the_supporting_rectangle_is_reported},
    {"set velocity is obeyed",               test_set_velocity_is_obeyed},
    {"apply force is obeyed",                test_apply_force_is_obeyed},
    {"pausing stops the simulation",         test_pausing_stops_the_simulation},
    {"killing a body stops its reports",     test_killing_a_body_stops_its_reports},
    {"reset clears every body",              test_reset_clears_every_body},
    {"several bodies keep their identities", test_several_bodies_keep_their_identities},
    {"a sleeping body stops being sent",     test_a_sleeping_body_stops_being_transmitted},
    {"a full level load survives the burst", test_a_full_level_load_survives_the_burst},
    {"the stream is aligned after a burst",  test_the_command_stream_is_still_aligned_after_a_burst},
    {"platform commands round trip",         test_platform_commands_round_trip},
    {"moving a platform is reported",        test_moving_a_platform_is_reported},
    {"a body travels through a portal pair", test_a_body_travels_through_a_portal_pair},
    {"an unknown opcode does not wedge",     test_an_unknown_opcode_does_not_wedge_the_engine},
};

#define NUM_TESTS ((int)(sizeof(tests) / sizeof(tests[0])))

int main(void)
{
    /*
     * No irqInit()/fifoInit() here on purpose. libnds' ARM9 start up has
     * already done both, and calling them again wipes the interrupt handler
     * that receives FIFO words - after which nothing the ARM7 sends ever
     * arrives and every test fails with "no reply arrived".
     */
    irqEnable(IRQ_VBLANK);

    /* Give the ARM7 a moment to finish its own start up before talking to it. */
    for (int i = 0; i < 10; i++)
        swiWaitForVBlank();

    probeStr("portalDS FIFO round trip tests\n");

    int passed = 0;
    for (int i = 0; i < NUM_TESTS; i++)
    {
        const int failuresBefore = failures;

        tests[i].fn();

        if (failures == failuresBefore)
        {
            passed++;
            probePrintf("  ok   %s\n", tests[i].name);
        }
        else
        {
            probePrintf("  FAIL %s\n", tests[i].name);
        }
    }

    probePrintf("DONE %d/%d tests, %d checks, %d failures\n",
                passed, NUM_TESTS, checks, failures);

    /*
     * The harness reads its verdict from this line, and requires one. Without
     * it a hung ROM and a passing one would look identical, because the
     * emulator never exits on its own and has to be killed either way.
     *
     * It is emitted twice because the last thing printed before the emulator
     * is killed is the most likely to be damaged - the X server's own shutdown
     * message has been seen landing in the middle of a line. The harness takes
     * whichever copy parses, and fails if neither does rather than guessing.
     */
    for (int i = 0; i < 2; i++)
        probePrintf("STATUS pass=%d total=%d checks=%d failures=%d\n",
                    passed, NUM_TESTS, checks, failures);

    while (1)
        swiWaitForVBlank();
    return 0;
}
