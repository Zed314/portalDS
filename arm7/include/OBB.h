/**
 * @file OBB.h
 * @brief Oriented bounding boxes - the rigid bodies of the ARM7 physics engine.
 *
 * Everything the player can push around (weighted cubes, mostly) is an OBB: a
 * box with a mass, an orientation matrix and a full set of linear and angular
 * state. The solver is an impulse based one modelled on Chris Hecker's rigid
 * body dynamics articles (see the acknowledgements in the README).
 *
 * @par How a step works
 * @ref updateOBBs walks every live body and, for each, runs the loop in
 * simulate():
 *  1. gravity is applied as a constant force;
 *  2. the body is integrated forward to the end of the timestep;
 *  3. contacts are gathered against the other bodies and against the static
 *     world (@ref AARsOBBContacts);
 *  4. if anything is penetrating too deeply the body is rolled back to a
 *     saved copy and the step is bisected, so collisions land at (nearly) the
 *     exact time of impact rather than after the fact;
 *  5. impulses are applied at the surviving contact points, and the loop
 *     continues from that time until the full step is consumed.
 *
 * @par Sleeping
 * A body whose kinetic energy stays below @ref SLEEPTHRESHOLD for
 * @ref SLEEPTIMETHRESHOLD frames goes to sleep and stops being simulated or
 * transmitted to the ARM9 - which is what keeps a room full of cubes
 * affordable. A body may only sleep if everything it rests on is also asleep,
 * so stacks settle together rather than freezing mid-collapse.
 *
 * @see AAR.h for the static geometry, plane.h for the half space contacts.
 */

#ifndef OBB_H
#define OBB_H

#define NUMOBJECTS (8) /**< Hard cap on simultaneously simulated rigid bodies. */

#define NUMOBBSEGMENTS (12) /**< Edges of a box. */
#define NUMOBBFACES (6)     /**< Faces of a box. */
#define MAXCONTACTPOINTS (32) /**< Size of the shared contact scratch buffer. */ // pool system ?
#define PENETRATIONTHRESHOLD (1<<6) /**< Penetration depth above which the timestep is bisected. */
#define MAXPENETRATIONBOX (1<<6)    /**< Penetration depth beyond which a box-box contact is discarded as bogus. */

#define SLEEPTHRESHOLD (50)     /**< Energy below which a body is a candidate for sleeping. */
#define SLEEPTIMETHRESHOLD (48) /**< Frames a body must stay calm before it actually sleeps. */

/**
 * @brief Vertex index pairs describing the twelve edges of a box.
 *
 * Indices refer to the vertex order produced by @ref getVertices.
 */
static const u8 OBBSegments[NUMOBBSEGMENTS][2]={{0,1},{1,2},{3,2},{0,3},
										 {5,4},{5,6},{6,7},{4,7},
										 {3,4},{0,5},{1,6},{2,7}};

/**
 * @brief Start vertex and axis of each edge, companion to @ref OBBSegments.
 *
 * For edge @c i, entry @c [i][0] is the vertex the edge starts from and
 * @c [i][1] is which of the box's three local axes it runs along. The clipping
 * code uses this to avoid recomputing edge directions from the vertices.
 */
static const u8 OBBSegmentsPD[NUMOBBSEGMENTS][2]={{0,0},{1,2},{3,0},{0,2},
										 {5,2},{5,0},{6,2},{4,0},
										 {3,1},{0,1},{1,1},{2,1}};


//static const s8 OBBFacesPDDN[NUMOBBFACES][4]={{0,0,2,-2},{5,0,2,2},{0,1,2,-1},{0,0,1,-3},{1,1,2,1},{3,0,1,3}};

/**
 * @brief What kind of geometry a contact point was generated against.
 *
 * The type decides which impulse routine runs: @ref PLANECOLLISION and
 * @ref AARCOLLISION are treated as collisions with immovable geometry, while
 * @ref TESTPOINT is a genuine two-body contact.
 */
typedef enum
{
	BOXCOLLISION, /**< Box against box. Currently generated but not solved. */
	PLANECOLLISION, /**< Box against an infinite plane. */
	TESTPOINT,      /**< Box against box, vertex-in-face form; this is the one that is actually solved. */
	AARCOLLISION    /**< Box against a static axis aligned rectangle. */
}contactPoint_type;

/**
 * @brief A single point of contact awaiting an impulse.
 */
typedef struct
{
	vect3D point;   /**< Contact position in world space. */
	vect3D normal;  /**< Contact normal, pointing away from the other body. */
	u16 penetration;/**< How deep the interpenetration is; drives the timestep bisection. */
	void* target;   /**< The other party: an OBB_struct for two-body contacts, NULL otherwise. */
	contactPoint_type type; /**< Which kind of geometry produced this contact. */
}contactPoint_struct;

/**
 * @brief Scratch buffer shared by every body's contact list.
 *
 * Contacts only live for the duration of one body's solve, so a single global
 * buffer is enough and saves precious ARM7 RAM. Every OBB_struct::contactPoints
 * points here.
 */
extern contactPoint_struct contactPoints[MAXCONTACTPOINTS];

/**
 * @brief A rigid body: an oriented box with full linear and angular state.
 */
typedef struct
{
	int32 mass;                  /**< Body mass, f32. */
	int32 transformationMatrix[9]; /**< Orientation, row-major 3x3, columns are the box's local axes. */ //3x3
	int32 invInertiaMatrix[9];   /**< Inverse inertia tensor in body space (constant, diagonal). */ //3x3
	int32 invWInertiaMatrix[9];  /**< Inverse inertia tensor rotated into world space; recomputed each integration. */ //3x3
	u16 maxPenetration;          /**< Deepest penetration among this frame's contacts. */
	contactPoint_struct* contactPoints; /**< Points at the shared @ref contactPoints buffer. */ //all point to the same array, temporary
	u8 numContactPoints;         /**< Number of valid entries in ::contactPoints. */
	vect3D size;                 /**< Half extents along the box's own axes. */
	vect3D position;             /**< Centre of mass in world space. */
	vect3D velocity;             /**< Linear velocity. */
	vect3D angularVelocity;      /**< Angular velocity, derived from ::angularMomentum. */
	vect3D forces;               /**< Force accumulator, cleared at the end of every step. */
	vect3D moment;               /**< Torque accumulator, cleared at the end of every step. */
	vect3D angularMomentum;      /**< Angular momentum; the integrated quantity, more stable than integrating velocity. */
	vect3D AABBo;                /**< Origin of the cached world-space bounding box. */
	vect3D AABBs;                /**< Size of the cached world-space bounding box. */
	u8 portal[2];                /**< Per portal: bit 0 = in front of the portal plane, bit 1 = inside its outline. */
	u8 oldPortal[2];             /**< Previous frame's ::portal bits; a change is what triggers a teleport. */
	u32 energy;                  /**< Pseudo kinetic energy used by the sleep heuristic. */
	u16 counter;                 /**< Frames spent below @ref SLEEPTHRESHOLD. */
	s16 groundID;                /**< Id of the platform the body is standing on, or -1. Reported to the ARM9 so it can ride along. */
	bool portaled;               /**< Set on the frame the body teleported; the ARM9 uses it to skip interpolation. */
	bool sleep;                  /**< True while the body is asleep and not being simulated. */
	bool used;                   /**< False when this slot is free. */
}OBB_struct;
// 4 + 9*4*3 + 4 +

extern OBB_struct objects[NUMOBJECTS]; /**< The rigid body pool; index is the id used on the FIFO. */
extern u32 coll, integ, impul;         /**< Profiling counters: time spent colliding, integrating and applying impulses. */
extern u8 sleeping;                    /**< How many bodies slept through the last @ref updateOBBs call. */

/**
 * @brief Initialises a body in place.
 *
 * Zeroes the dynamic state and builds the inverse inertia tensor for a solid
 * box of the given size and mass. If both portals already exist the body's
 * portal side bits are primed so it does not immediately teleport.
 *
 * @param o      body to initialise; ignored if NULL.
 * @param size   half extents.
 * @param pos    initial centre of mass.
 * @param mass   mass in f32.
 * @param cosine cosine of the initial yaw.
 * @param sine   sine of the initial yaw.
 */
void initOBB(OBB_struct* o, vect3D size, vect3D pos, int32 mass, s32 cosine, s32 sine);

/**
 * @brief Fills a 3x3 matrix with a rotation about the Y axis.
 *
 * Takes the cosine and sine directly rather than an angle, because the ARM9
 * has already computed them and sends them over the FIFO.
 *
 * @param m      destination 3x3 matrix.
 * @param cosine cosine of the yaw.
 * @param sine   sine of the yaw.
 */
void initTransformationMatrix(int32* m, s32 cosine, s32 sine);

/**
 * @brief Computes the eight world-space corners of a body.
 * @param o body to expand.
 * @param v output array of at least 8 vectors.
 */
void getOBBVertices(OBB_struct* o, vect3D* v);

/** @brief Debug draw hook. Compiled out. */
void drawOBB(OBB_struct* o);

/**
 * @brief Resolves every contact gathered for a body.
 *
 * Dispatches on contactPoint_struct::type. Note that @ref BOXCOLLISION
 * contacts are deliberately skipped - box-box response is handled through the
 * @ref TESTPOINT contacts instead.
 *
 * @param o body to solve; ignored if NULL.
 */
void applyOBBImpulses(OBB_struct* o);

/**
 * @brief Accumulates a force applied at a world-space point.
 *
 * Adds to both the linear force accumulator and the torque accumulator; the
 * torque is the moment arm from the centre of mass crossed with the force.
 *
 * @param o body to push.
 * @param p application point in world space.
 * @param f force vector.
 */
void applyOBBForce(OBB_struct* o, vect3D p, vect3D f);
//void updateOBB(OBB_struct* o);

/** @brief Marks every rigid body slot as free. */
void initOBBs(void);

/**
 * @brief Advances every live rigid body by one frame.
 *
 * Also resets the @ref sleeping counter, and handles portal traversal for each
 * body once it has been integrated.
 */
void updateOBBs(void);

/** @brief Debug draw hook. Compiled out. */
void drawOBBs(void);

/**
 * @brief Wakes every rigid body.
 *
 * Called whenever the world changes underneath the simulation - most notably
 * when a portal moves, since a sleeping box may suddenly have nothing to rest
 * on.
 */
void wakeOBBs(void);

/**
 * @brief Creates a rigid body in a specific slot.
 *
 * The slot is overwritten unconditionally, so the ARM9 is free to recycle ids.
 *
 * @param id       slot index, must be below @ref NUMOBJECTS.
 * @param size     half extents.
 * @param position initial centre of mass.
 * @param mass     mass in f32.
 * @param cosine   cosine of the initial yaw.
 * @param sine     sine of the initial yaw.
 * @return the body that was created.
 */
OBB_struct* createOBB(u8 id, vect3D size, vect3D position, int32 mass, s32 cosine, s32 sine);

/**
 * @brief Updates a body's relationship to one portal, teleporting it if needed.
 *
 * Recomputes which side of the portal plane the body is on and whether it lies
 * within the portal outline. A body that crossed from front to back while
 * inside the outline is transported: position, velocity, forces, angular
 * velocity, moment and both matrices are all warped through to the far portal.
 *
 * @param o    body to test.
 * @param id   portal index, 0 or 1.
 * @param init true on the frame the body is created, to record the current
 *             side without triggering a teleport.
 */
void updateOBBPortals(OBB_struct* o, u8 id, bool init);

/**
 * @brief Expands a box into its eight corners.
 * @param s  half extents.
 * @param p  centre.
 * @param u1 first local axis.
 * @param u2 second local axis.
 * @param u3 third local axis.
 * @param v  output array of at least 8 vectors.
 */
void getVertices(vect3D s, vect3D p, vect3D u1, vect3D u2, vect3D u3, vect3D* v);

/**
 * @brief Generates contact points between two rigid bodies.
 *
 * Rejects the pair cheaply on their axis aligned bounding boxes first, then
 * clips each body's edges against the other's faces. Contacts are appended to
 * @p o1's contact list.
 *
 * @param o1 body being solved.
 * @param o2 the other body.
 */
void collideOBBs(OBB_struct* o1, OBB_struct* o2);

/**
 * @brief Clips one box edge against another box, tracking the nearest features.
 *
 * The workhorse of @ref collideOBBs, and the reason it takes so many
 * parameters: it both clips the segment and reports which faces it entered and
 * left through, so the caller can build a contact with a sensible normal
 * without a second pass.
 *
 * @param ss  half extents of the box being clipped against.
 * @param uu  its three local axes.
 * @param p1  in/out: first endpoint of the segment.
 * @param p2  in/out: second endpoint of the segment.
 * @param vv  segment direction.
 * @param uu1 axes of the box the segment belongs to.
 * @param uu2 axes of the box being clipped against.
 * @param vv1 centre-to-centre vector between the two boxes.
 * @param n1  out: face normal the segment enters through.
 * @param n2  out: face normal the segment leaves through.
 * @param b1  out: whether the entry point is a real clip.
 * @param b2  out: whether the exit point is a real clip.
 * @param k1  out: parametric position of the entry point.
 * @param k2  out: parametric position of the exit point.
 * @return true if any part of the segment survives inside the box.
 */
bool clipSegmentOBB(int32* ss, vect3D *uu, vect3D* p1, vect3D* p2, vect3D vv, vect3D* uu1, vect3D* uu2, vect3D vv1, vect3D* n1, vect3D* n2, bool* b1, bool* b2, int32* k1, int32* k2);

#endif
