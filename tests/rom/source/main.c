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

/* --- receiving replies ---------------------------------------------------- */

typedef struct
{
    bool seen;
    s32 x, y, z;
    s32 m[9];      /* only the six transmitted elements are filled in */
    s16 groundID;
    bool portaled;
    int updates;   /* how many reports arrived since the last clear */
} report_t;

static report_t reports[NUM_BODIES];

static void clearReports(void)
{
    memset(reports, 0, sizeof(reports));
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
            continue; /* a platform: position only, nothing more to read */

        while (!fifoCheckValue32(FIFO_USER_05));
        while (!fifoCheckValue32(FIFO_USER_06));
        while (!fifoCheckValue32(FIFO_USER_07));

        report_t *r = &reports[k];
        r->seen = true;
        r->updates++;
        r->x = px; r->y = py; r->z = pz;
        r->groundID = (s16)((h >> 17) - 1);
        r->portaled = (h >> 16) & 1;

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
