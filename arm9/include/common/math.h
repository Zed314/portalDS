/**
 * @file math.h
 * @brief Fixed point vector maths, angle tables and screen fades for the ARM9.
 *
 * Like the ARM7, the ARM9 works in 20.12 fixed point - an @c int32 where 4096
 * is 1.0 - but here the DS's hardware maths unit is available, so this file
 * leans on the libnds primitives (@c mulf32, @c divf32, @c sqrt64,
 * @c normalizef32, @c crossf32) instead of the hand written ARM routines the
 * ARM7 needs.
 *
 * @par Two angle conventions live in this file
 * Be careful which you are using:
 *  - libnds' @c cosLerp / @c sinLerp take a 15 bit binary angle, where a full
 *    turn is 32768;
 *  - @ref Math_Cos / @ref Math_Sin below index @ref TABLE_SIN with a 9 bit
 *    angle, where a full turn is 512, and return a value scaled to 256 rather
 *    than 4096. These come from Mollusk's PAlib-era code and are used for the
 *    2D interface and particle work.
 *
 * @par Matrices
 * 3x3 only, row-major in a flat 9 element array: @c (row,col) is at
 * @c m[col+row*3]. Same convention as the ARM7.
 *
 * @note arm7/include/math.h is the sibling of this file. Shared behaviour has
 *       to be kept in step by hand.
 */

#ifndef __GENMATH9__
#define __GENMATH9__

#define EPSINT32 2 /**< Tolerance used by @ref equals when comparing f32 values. */ //check that

#define min(a,b) (((a)>(b))?(b):(a)) /**< @brief Smaller of two values. Evaluates its arguments twice. */
#define max(a,b) (((a)>(b))?(a):(b)) /**< @brief Larger of two values. Evaluates its arguments twice. */

#define vect(x,y,z) ((vect3D){(x),(y),(z)}) /**< @brief Compound literal building a @ref vect3D. */
#define vect2(x,y) ((vect2D){(x),(y)})      /**< @brief Compound literal building a @ref vect2D. */

/** @brief A 3D vector in 20.12 fixed point; also used for positions and sizes. */
typedef struct
{
	int32 x, y, z;
}vect3D;

/** @brief A 2D vector in 20.12 fixed point; mostly screen and texture coordinates. */
typedef struct
{
	int32 x, y;
}vect2D;

/**
 * @brief Fades both screens up from black to the player's brightness setting.
 * @note Blocks on vblank, so nothing else runs during the fade.
 */
static inline void fadeIn(void)
{
	//The fade ends at settings.brightness rather than at zero, so that every
	//screen a fade precedes comes up at the level the player chose. This is the
	//only place that has to know: every state fades in, so nothing else needs
	//to apply the setting on its way up.
	const int target=settings.brightness;

	//Wait first, then write. The other way round put every step of the fade in
	//the middle of a frame, so the screen was drawn part at the old brightness
	//and part at the new one - a seam travelling down it for the length of the
	//fade. Emulators say so out loud; DeSmuME prints "Changing master
	//brightness outside of vblank" once per step.
	int i;for(i=0;i<=16;i++){swiWaitForVBlank();setBrightness(3,-16+i+(target*i)/16);}
}

/**
 * @brief Fades both screens down to black over 17 frames.
 * @note Blocks on vblank, so nothing else runs during the fade.
 */
static inline void fadeOut(void)
{
	//See fadeIn(): vblank first, then the brightness write.
	int i;for(i=0;i<=16;i++){swiWaitForVBlank();setBrightness(3,-i);}
}

/**
 * @brief Transposes a 3x3 matrix: @p m2 = @p m1 transposed.
 *
 * For a pure rotation this is also the inverse, which is how view matrices are
 * built from camera orientations.
 */
static inline void transposeMatrix33(int32* m1, int32* m2) //3x3
{
	int i, j;
	for(i=0;i<3;i++)for(j=0;j<3;j++)m2[j+i*3]=m1[i+j*3];
}

/** @brief Transforms a vector by a 3x3 matrix, accumulating in 64 bits. */
static inline vect3D evalVectMatrix33(int32* m, vect3D v) //3x3
{
	return vect(((int64_t)v.x*m[0]+(int64_t)v.y*m[1]+(int64_t)v.z*m[2])>>12,
				((int64_t)v.x*m[3]+(int64_t)v.y*m[4]+(int64_t)v.z*m[5])>>12,
				((int64_t)v.x*m[6]+(int64_t)v.y*m[7]+(int64_t)v.z*m[8])>>12);
}

/** @brief Component-wise minimum; together with @ref maxVect this builds bounding boxes. */
static inline vect3D minVect(vect3D u, vect3D v)
{
	return vect(min(u.x,v.x),min(u.y,v.y),min(u.z,v.z));
}

/** @brief Component-wise maximum. */
static inline vect3D maxVect(vect3D u, vect3D v)
{
	return vect(max(u.x,v.x),max(u.y,v.y),max(u.z,v.z));
}

/** @brief Component-wise sum of two vectors. */
static inline vect3D addVect(vect3D p1, vect3D p2)
{
	return vect(p1.x+p2.x,p1.y+p2.y,p1.z+p2.z);
}

/** @brief Scales a vector by an f32 scalar. */
static inline vect3D vectMult(vect3D v, int32 k)
{
	return vect(mulf32(v.x,k), mulf32(v.y,k), mulf32(v.z,k));
}

/** @brief Scales a vector by a plain integer (no fixed point shift). */
static inline vect3D vectMultInt(vect3D v, int k)
{
	return vect((v.x*k), (v.y*k), (v.z*k));
}

/** @brief Divides a vector by a plain integer (no fixed point shift). */
static inline vect3D vectDivInt(vect3D v, int k)
{
	return vect((v.x/k), (v.y/k), (v.z/k));
}

/** @brief Computes @p p1 - @p p2. */
static inline vect3D vectDifference(vect3D p1, vect3D p2)
{
	return vect(p1.x-p2.x,p1.y-p2.y,p1.z-p2.z);
}

/**
 * @brief Squared distance between two points.
 *
 * Preferred over @ref distance whenever the result is only being compared
 * against another distance, since it skips the square root.
 */
static inline int32 sqDistance(vect3D p1, vect3D p2)
{
    int32_t xdiff=(p1.x-p2.x);
    int32_t ydiff=(p1.y-p2.y);
    int32_t zdiff=(p1.z-p2.z);
    return ((int64_t)xdiff*xdiff+(int64_t)ydiff*ydiff+(int64_t)zdiff*zdiff)>>12;
}

/** @brief Distance between two points, in f32. */
static inline int32 distance(vect3D p1, vect3D p2)
{
    int32_t xdiff=(p1.x-p2.x);
    int32_t ydiff=(p1.y-p2.y);
    int32_t zdiff=(p1.z-p2.z);
	return sqrt64((int64_t)xdiff*xdiff+(int64_t)ydiff*ydiff+(int64_t)zdiff*zdiff);
}

/** @brief Length of a vector, in f32. */
static inline int32 magnitude(vect3D p1)
{
	return sqrt64((int64_t)p1.x*p1.x  +(int64_t)p1.y*p1.y  +(int64_t)p1.z*p1.z);
}

/** @brief Squared length of a vector; cheaper than @ref magnitude when only comparing. */
static inline int32 sqMagnitude(vect3D p1)
{
	return ((int64_t)p1.x*p1.x  +(int64_t)p1.y*p1.y  +(int64_t)p1.z*p1.z)>>12;
}

/** @brief Divides a vector by an f32 scalar. */
static inline vect3D divideVect(vect3D v, int32 d)
{
	return vect(divf32(v.x,d),divf32(v.y,d),divf32(v.z,d));
}

/**
 * @brief Scales a vector to unit length using the DS maths unit.
 *
 * The branch on @c sizeof lets the hardware read the struct in place when it
 * is laid out as three packed words, and copies through a temporary otherwise.
 * It is resolved at compile time, so it costs nothing at runtime.
 */
static inline vect3D normalize(vect3D v)
{
    if ( sizeof(vect3D)==3*(sizeof(int32_t)) )
    {
        normalizef32(&v);
        return v;
    } 
    else 
    {
        int32_t a[3];
        a[0]=v.x;
        a[1]=v.y;
        a[2]=v.z;
        normalizef32(&a);
        v.x=a[0];
        v.y=a[1];
        v.z=a[2];
        return v;
    }
}

/** @brief Dot product of two vectors, accumulated in 64 bits. */
static inline int32 dotProduct(vect3D v1, vect3D v2)
{
    return ((int64_t)v1.x*v2.x+(int64_t)v1.y*v2.y+(int64_t)v1.z*v2.z)>>12;
}

/** @brief Cross product of two vectors, via the DS maths unit. */
static inline vect3D vectProduct(vect3D v1, vect3D v2)
{
    int32_t a[3]={v1.x,v1.y,v1.z};
    int32_t b[3]={v2.x,v2.y,v2.z};
    int32_t result[3];
    crossf32(&a[0], &b[0], &result[0]);
    return vect(result[0], result[1], result[2]);
}

/**
 * @brief Dot product without the fixed point rescale.
 *
 * Keeps the extra 12 bits of precision, so it truncates to 32 bits. Only the
 * *sign* of the result is meaningful - which is all the callers want, since
 * they use it to test which side of a plane something is on.
 */
static inline int32 fakeDotProduct(vect3D v1, vect3D v2)
{
	return ((int64_t)v1.x*v2.x+(int64_t)v1.y*v2.y+(int64_t)v1.z*v2.z);
}

/** @brief Compares two f32 values with a tolerance of @ref EPSINT32. */
static inline bool equals(int32 a, int32 b)
{
	return abs(a-b)<=EPSINT32;
}

/** @brief Writes a vector to a file in raw binary. Used by the editor's level format. */
static inline void writeVect(vect3D v, FILE* f)
{
	fwrite(&v,sizeof(vect3D),1,f);
}

/** @brief Reads a vector written by @ref writeVect. */
static inline void readVect(vect3D* v, FILE* f)
{
	fread(v,sizeof(vect3D),1,f);
}

/**
 * @name Mollusk's angle helpers
 *
 * A separate trigonometry convention from libnds': angles run 0-511 for a full
 * turn and results are scaled to 256, not 4096.
 * @{
 */

//Original code by Mollusk (below this point)

/**
 * @brief Two-argument arctangent.
 * @return the angle of the vector (@p dx, @p dy) in the 0-511 convention.
 */
int ArcTan2(int dx, int dy);

/** @brief Sine table over a full turn of 512 steps, scaled so 1.0 is 256. */
static const short TABLE_SIN[512] = {
	0x0000,0x0003,0x0006,0x0009,0x000D,0x0010,0x0013,0x0016,	0x0019,0x001C,0x001F,0x0022,0x0026,0x0029,0x002C,0x002F,
	0x0032,0x0035,0x0038,0x003B,0x003E,0x0041,0x0044,0x0047,	0x004A,0x004D,0x0050,0x0053,0x0056,0x0059,0x005C,0x005F,
	0x0062,0x0065,0x0068,0x006B,0x006D,0x0070,0x0073,0x0076,	0x0079,0x007B,0x007E,0x0081,0x0084,0x0086,0x0089,0x008C,
	0x008E,0x0091,0x0093,0x0096,0x0098,0x009B,0x009D,0x00A0,	0x00A2,0x00A5,0x00A7,0x00AA,0x00AC,0x00AE,0x00B1,0x00B3,

	0x00B5,0x00B7,0x00B9,0x00BC,0x00BE,0x00C0,0x00C2,0x00C4,	0x00C6,0x00C8,0x00CA,0x00CC,0x00CE,0x00CF,0x00D1,0x00D3,
	0x00D5,0x00D7,0x00D8,0x00DA,0x00DC,0x00DD,0x00DF,0x00E0,	0x00E2,0x00E3,0x00E5,0x00E6,0x00E7,0x00E9,0x00EA,0x00EB,
	0x00ED,0x00EE,0x00EF,0x00F0,0x00F1,0x00F2,0x00F3,0x00F4,	0x00F5,0x00F6,0x00F7,0x00F8,0x00F8,0x00F9,0x00FA,0x00FA,
	0x00FB,0x00FC,0x00FC,0x00FD,0x00FD,0x00FE,0x00FE,0x00FE,	0x00FF,0x00FF,0x00FF,0x0100,0x0100,0x0100,0x0100,0x0100,

	0x0100,0x0100,0x0100,0x0100,0x0100,0x0100,0x00FF,0x00FF,	0x00FF,0x00FE,0x00FE,0x00FE,0x00FD,0x00FD,0x00FC,0x00FC,
	0x00FB,0x00FA,0x00FA,0x00F9,0x00F8,0x00F8,0x00F7,0x00F6,	0x00F5,0x00F4,0x00F3,0x00F2,0x00F1,0x00F0,0x00EF,0x00EE,
	0x00ED,0x00EB,0x00EA,0x00E9,0x00E7,0x00E6,0x00E5,0x00E3,	0x00E2,0x00E0,0x00DF,0x00DD,0x00DC,0x00DA,0x00D8,0x00D7,
	0x00D5,0x00D3,0x00D1,0x00CF,0x00CE,0x00CC,0x00CA,0x00C8,	0x00C6,0x00C4,0x00C2,0x00C0,0x00BE,0x00BC,0x00B9,0x00B7,

	0x00B5,0x00B3,0x00B1,0x00AE,0x00AC,0x00AA,0x00A7,0x00A5,	0x00A2,0x00A0,0x009D,0x009B,0x0098,0x0096,0x0093,0x0091,
	0x008E,0x008C,0x0089,0x0086,0x0084,0x0081,0x007E,0x007B,	0x0079,0x0076,0x0073,0x0070,0x006D,0x006B,0x0068,0x0065,
	0x0062,0x005F,0x005C,0x0059,0x0056,0x0053,0x0050,0x004D,	0x004A,0x0047,0x0044,0x0041,0x003E,0x003B,0x0038,0x0035,
	0x0032,0x002F,0x002C,0x0029,0x0026,0x0022,0x001F,0x001C,	0x0019,0x0016,0x0013,0x0010,0x000D,0x0009,0x0006,0x0003,

	0x0000,0xFFFD,0xFFFA,0xFFF7,0xFFF3,0xFFF0,0xFFED,0xFFEA,	0xFFE7,0xFFE4,0xFFE1,0xFFDE,0xFFDA,0xFFD7,0xFFD4,0xFFD1,
	0xFFCE,0xFFCB,0xFFC8,0xFFC5,0xFFC2,0xFFBF,0xFFBC,0xFFB9,	0xFFB6,0xFFB3,0xFFB0,0xFFAD,0xFFAA,0xFFA7,0xFFA4,0xFFA1,
	0xFF9E,0xFF9B,0xFF98,0xFF95,0xFF93,0xFF90,0xFF8D,0xFF8A,	0xFF87,0xFF85,0xFF82,0xFF7F,0xFF7C,0xFF7A,0xFF77,0xFF74,
	0xFF72,0xFF6F,0xFF6D,0xFF6A,0xFF68,0xFF65,0xFF63,0xFF60,	0xFF5E,0xFF5B,0xFF59,0xFF56,0xFF54,0xFF52,0xFF4F,0xFF4D,

	0xFF4B,0xFF49,0xFF47,0xFF44,0xFF42,0xFF40,0xFF3E,0xFF3C,	0xFF3A,0xFF38,0xFF36,0xFF34,0xFF32,0xFF31,0xFF2F,0xFF2D,
	0xFF2B,0xFF29,0xFF28,0xFF26,0xFF24,0xFF23,0xFF21,0xFF20,	0xFF1E,0xFF1D,0xFF1B,0xFF1A,0xFF19,0xFF17,0xFF16,0xFF15,
	0xFF13,0xFF12,0xFF11,0xFF10,0xFF0F,0xFF0E,0xFF0D,0xFF0C,	0xFF0B,0xFF0A,0xFF09,0xFF08,0xFF08,0xFF07,0xFF06,0xFF06,
	0xFF05,0xFF04,0xFF04,0xFF03,0xFF03,0xFF02,0xFF02,0xFF02,	0xFF01,0xFF01,0xFF01,0xFF00,0xFF00,0xFF00,0xFF00,0xFF00,

	0xFF00,0xFF00,0xFF00,0xFF00,0xFF00,0xFF00,0xFF01,0xFF01,	0xFF01,0xFF02,0xFF02,0xFF02,0xFF03,0xFF03,0xFF04,0xFF04,
	0xFF05,0xFF06,0xFF06,0xFF07,0xFF08,0xFF08,0xFF09,0xFF0A,	0xFF0B,0xFF0C,0xFF0D,0xFF0E,0xFF0F,0xFF10,0xFF11,0xFF12,
	0xFF13,0xFF15,0xFF16,0xFF17,0xFF19,0xFF1A,0xFF1B,0xFF1D,	0xFF1E,0xFF20,0xFF21,0xFF23,0xFF24,0xFF26,0xFF28,0xFF29,
	0xFF2B,0xFF2D,0xFF2F,0xFF31,0xFF32,0xFF34,0xFF36,0xFF38,	0xFF3A,0xFF3C,0xFF3E,0xFF40,0xFF42,0xFF44,0xFF47,0xFF49,

	0xFF4B,0xFF4D,0xFF4F,0xFF52,0xFF54,0xFF56,0xFF59,0xFF5B,	0xFF5E,0xFF60,0xFF63,0xFF65,0xFF68,0xFF6A,0xFF6D,0xFF6F,
	0xFF72,0xFF74,0xFF77,0xFF7A,0xFF7C,0xFF7F,0xFF82,0xFF85,	0xFF87,0xFF8A,0xFF8D,0xFF90,0xFF93,0xFF95,0xFF98,0xFF9B,
	0xFF9E,0xFFA1,0xFFA4,0xFFA7,0xFFAA,0xFFAD,0xFFB0,0xFFB3,	0xFFB6,0xFFB9,0xFFBC,0xFFBF,0xFFC2,0xFFC5,0xFFC8,0xFFCB,
	0xFFCE,0xFFD1,0xFFD4,0xFFD7,0xFFDA,0xFFDE,0xFFE1,0xFFE4,	0xFFE7,0xFFEA,0xFFED,0xFFF0,0xFFF3,0xFFF7,0xFFFA,0xFFFD};

#define Math_Cos(angle) TABLE_SIN[((angle) + 128)&511] /**< @brief Cosine of a 0-511 angle, scaled to 256. Cosine is sine shifted a quarter turn. */
#define Math_Sin(angle) TABLE_SIN[((angle))&511]       /**< @brief Sine of a 0-511 angle, scaled to 256. */


/**
 * @brief One bisection step of an iterative "point towards a target" search.
 *
 * Tries rotating @p angle by plus and minus @p anglerot and keeps whichever
 * points closer to the target. The commented-out getAngle() below shows the
 * intended use: call repeatedly with a shrinking @p anglerot to converge on
 * the bearing from (@p startx, @p starty) to (@p targetx, @p targety).
 *
 * @param angle    current angle, 0-511.
 * @param anglerot how far to try rotating this step.
 * @param startx   source x.
 * @param starty   source y.
 * @param targetx  target x.
 * @param targety  target y.
 * @return the better of the two candidate angles.
 */
u16 Math_AdjustAngle(u16 angle, s16 anglerot, s32 startx, s32 starty, s32 targetx, s32 targety);
/*
extern inline u16 getAngle(s32 startx, s32 starty, s32 targetx, s32 targety) {
	u16 angle = 0;
	u16 anglerot = 180;


	while(anglerot > 5) {
		angle = Math_AdjustAngle(angle, anglerot, startx, starty, targetx, targety);
		anglerot = (anglerot - ((3 * anglerot) >> 3)); // On diminue petit ? petit la rotation...
	}

	// Ajustement encore plus pr?cis...
	anglerot = 4;
	angle = Math_AdjustAngle(angle, anglerot, startx, starty, targetx, targety);
	anglerot = 2;
	angle = Math_AdjustAngle(angle, anglerot, startx, starty, targetx, targety);
	anglerot = 1;
	angle = Math_AdjustAngle(angle, anglerot, startx, starty, targetx, targety);

	return angle;
}*/
/** @} */

#endif
