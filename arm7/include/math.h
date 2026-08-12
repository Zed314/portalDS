/**
 * @file math.h
 * @brief Fixed point vector and matrix maths for the ARM7.
 *
 * The DS has no FPU, so every quantity in the physics engine is a 20.12 fixed
 * point number: an @c int32 where the low 12 bits are the fraction, so
 * @c 4096 represents 1.0. The type is spelled @c int32 (or @c f32 in libnds
 * documentation) and the conversion macros at the top of this file are the
 * only place the convention is written down.
 *
 * @par Multiplication conventions
 * There are two multiply helpers and picking the wrong one is the classic
 * source of bugs here:
 *  - @ref mulf32 widens to 64 bits first, so it is correct for any inputs;
 *  - @ref mulv16 stays in 32 bits, so it is faster but overflows once the
 *    product of the two inputs exceeds ~2^31. It is only safe for small values
 *    such as normalised vector components.
 *
 * @par Matrices
 * All matrices are 3x3, stored row-major in a flat 9 element array: element
 * @c (row,col) lives at @c m[col+row*3]. Only rotations are ever stored, never
 * translations - positions are carried separately as a @ref vect3D.
 *
 * @note The ARM9 has its own near-identical copy of this file
 *       (arm9/include/common/math.h) with the extra helpers the renderer needs.
 *       Changes to shared behaviour have to be made in both.
 */

#ifndef MATH_H
#define MATH_H

/**
 * @name Fixed point conversions
 * 20.12 fixed point: 4096 == 1.0.
 * @{
 */
#define inttof32(n)          ((n) << (12)) /*!< \brief convert int to f32 */
#define f32toint(n)          ((n) >> (12)) /*!< \brief convert f32 to int */
#define floattof32(n)        ((int)((n) * (1 << (12)))) /*!< \brief convert float to f32 */
#define f32tofloat(n)        (((float)(n)) / (float)(1<<(12))) /*!< \brief convert f32 to float */
/** @} */

#define min(a,b) (((a)>(b))?(b):(a)) /**< @brief Smaller of two values. Evaluates its arguments twice. */
#define max(a,b) (((a)>(b))?(a):(b)) /**< @brief Larger of two values. Evaluates its arguments twice. */

float cosf(float x); /**< @brief Single precision cosine, from libc. */
float sinf(float x); /**< @brief Single precision sine, from libc. */

/**
 * @brief A 3D vector in 20.12 fixed point.
 *
 * Doubles as a position, a direction and a set of half extents depending on
 * context.
 */
typedef struct
{
	int32_t x, y, z;
}vect3D;

/**
 * @brief Integer square root of an f32.
 *
 * Uses a hand written ARM routine for the common case, then refines the result
 * with one Newton step for large inputs.
 *
 * @param x value to take the root of, in f32.
 * @return the square root, in f32.
 */
uint32_t sqrtv(uint32_t x);

/** @brief Builds a vector from three components. */
__attribute__((always_inline ))static inline vect3D vect(int32_t x, int32_t y, int32_t z)
{
    vect3D v;
    v.x=x;
    v.y=y;
    v.z=z;
    return v;
}

/**
 * @brief Fixed point multiply, widening to 64 bits.
 *
 * The safe multiply: correct for any pair of f32 inputs.
 */
static inline int32_t mulf32(int32_t a, int32_t b)
{
	return ((int64_t)a*b)>>12;
}

// static inline int32 divf32(int32 a, int32 b)
// {
	// long long result = (((long long)a)<<12) / (long long)b;
	// return (int32)(result);
// }

/**
 * @brief Fixed point divide, staying in 32 bits.
 * @warning @p a is shifted left by 12 first, so it must be below 2^19 in
 *          magnitude or the result is garbage.
 */
static inline int32_t divv16(int32_t a, int32_t b)
{
	return (a<<12)/b;;
}

/**
 * @brief Fast fixed point multiply, staying in 32 bits.
 * @warning Overflows if @c a*b exceeds 32 bits. Use @ref mulf32 unless both
 *          operands are known to be small.
 */
static inline int32_t mulv16(int32_t a, int32_t b)
{
	return (a*b)>>12;
}

/** @brief Component-wise sum of two vectors. */
static inline vect3D addVect(vect3D p1, vect3D p2)
{
	return vect(p1.x+p2.x,p1.y+p2.y,p1.z+p2.z);
}

/** @brief Scales a vector by an f32 scalar, safely. */
static inline vect3D vectMult(vect3D v, int32 k)
{
	return vect(mulf32(v.x,k), mulf32(v.y,k), mulf32(v.z,k));
}

/** @brief Scales a vector by an f32 scalar, using the fast 32 bit multiply. */
static inline vect3D vectMultv16(vect3D v, int32 k)
{
	return vect(mulv16(v.x,k), mulv16(v.y,k), mulv16(v.z,k));
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

/** @brief Dot product of two vectors, accumulated in 64 bits. */
static inline int32 dotProduct(vect3D v1, vect3D v2)
{
	return ((int64_t)v1.x*v2.x+(int64_t)v1.y*v2.y+(int64_t)v1.z*v2.z)>>12;
}

/**
 * @brief Scales a vector to unit length.
 *
 * Implemented in normalize.c with a reciprocal square root rather than a
 * divide, since this is one of the hottest routines in the engine.
 *
 * @param v vector to normalise; a zero vector is returned unchanged.
 * @return the normalised vector.
 */
vect3D normalize(vect3D v);

/** @brief Length of a vector, in f32. */
static inline int32 magnitude(vect3D v)
{
	int32 d=sqrtv(((int64_t)v.x*v.x+(int64_t)v.y*v.y+(int64_t)v.z*v.z)>>12);
	return d;
}

/** @brief Distance between two points, in f32. */
static inline int32 distance(vect3D v1, vect3D v2)
{
	return magnitude(vectDifference(v2,v1));
}

/**
 * @brief Divides a vector by an f32 scalar.
 * @warning Inherits the range limits of @ref divv16.
 */
static inline vect3D divideVect(vect3D v, int32 d)
{
	return vect(divv16(v.x,d),divv16(v.y,d),divv16(v.z,d));
}

/**
 * @brief Hand written ARM cross product over three-element f32 arrays.
 *
 * Uses @c smull / @c smlal so each component is accumulated at full 64 bit
 * precision and shifted back down in one go. Call @ref vectProduct instead of
 * this directly.
 *
 * @param a      first operand, 3 elements.
 * @param b      second operand, 3 elements.
 * @param result output, 3 elements.
 */
ARM_CODE void asm_crossf32(const int32_t *a,const int32_t * b,int32_t *result);

/**
 * @brief Cross product of two vectors.
 *
 * The branch on @c sizeof exists so the assembly can read the structs directly
 * when @ref vect3D happens to be laid out as three tightly packed words, and
 * fall back to copying through a temporary array otherwise. The compiler
 * resolves it at build time, so there is no runtime cost.
 */
static inline vect3D vectProduct(vect3D v1, vect3D v2)
{
    int32_t result[3];
    if (sizeof(vect3D)!=3*sizeof(int32_t))
    {
        int32_t a[3]={v1.x, v1.y,v1.z};
        int32_t b[3]={v2.x,v2.y,v2.z};
        asm_crossf32(&a[0], &b[0],&result[0]);
    }
    else
    {
        asm_crossf32((const int32_t*)&v1,(const int32_t*) &v2,&result[0]);
    }
    return vect(result[0],result[1], result[2]);
}

// static inline int cosLerp(int32 x){return floattof32(cos((x*PI)/16384));}
// static inline int sinLerp(int32 x){return floattof32(sin((x*PI)/16384));}
#if 0
static inline int cosLerp(int32 x){return 1;}
static inline int sinLerp(int32 x){return 1;}
#else
/**
 * @name Trigonometry
 *
 * These take an f32 *radian* angle and return an f32 result, i.e. they compute
 * @c cos(x/4096)*4096. Note that this is not the libnds convention (a 15 bit
 * binary angle) that the commented-out versions above used.
 *
 * They currently go through the software float library, which is slow; the
 * comment below is the original author's note on that trade-off.
 * @{
 */
//this is a bad solution but I cant be bothered to do the performant solution right now
//hopefully this together with my other improvements cancels to keep performance reasonable
/** @brief Cosine of an f32 radian angle, returned in f32. */
static inline int cosLerp(int32_t x)
{
    return cosf((float)x/(1<<12))*(1<<12);
}
/** @brief Sine of an f32 radian angle, returned in f32. */
static inline int sinLerp(int32_t x)
{
    return sinf((float)x/(1<<12))*(1<<12);
}
/** @} */
#endif

/**
 * @brief Multiplies two 3x3 matrices: @p m = @p m1 * @p m2.
 * @param m1 left operand.
 * @param m2 right operand.
 * @param m  destination; must not alias either operand.
 */
void multMatrix33(int32_t * m1, int32_t * m2, int32_t *restrict m); //3x3

/**
 * @brief Column-major variant of @ref multMatrix33.
 * @warning The middle term of the sum reads @c m1[i+2*3] twice, which looks
 *          like a typo for @c m1[i+1*3]. Nothing in the shipped game calls
 *          this, so the bug has never bitten.
 */
void multMatrix332(int32_t * m1, int32_t * m2, int32_t *restrict m); //3x3

/**
 * @brief Adds two 3x3 matrices element-wise: @p m = @p m1 + @p m2.
 *
 * Used by the integrator, where the derivative of a rotation matrix is added
 * straight onto the orientation.
 */
static inline void addMatrix33(int32* m1, int32* m2, int32* m) //3x3
{
	for(int i=0;i<3;i++)
        for(int j=0;j<3;j++)
            m[j+i*3]=m1[j+i*3]+m2[j+i*3];
}

/**
 * @brief Transposes a 3x3 matrix: @p m2 = @p m1 transposed.
 *
 * For a pure rotation this is also its inverse, which is how the world-space
 * inverse inertia tensor is built each step.
 */
void transposeMatrix33(int32* m1, int32* m2); //3x3


/**
 * @brief Transforms a vector by a 3x3 matrix.
 * @param m row-major 3x3 matrix.
 * @param v vector to transform.
 * @return @p m * @p v.
 */
vect3D evalVectMatrix33(int32* m, vect3D v); //3x3


/**
 * @brief Clamps a value between two bounds.
 *
 * Tolerates the bounds being given the wrong way round, which several call
 * sites rely on.
 *
 * @param v value to clamp.
 * @param m one bound.
 * @param M the other bound.
 */
static inline int32 clamp(int32 v, int32 m, int32 M)
{
	if(m<M)
        return max(m,min(v,M));
	else
        return min(m,max(v,M));
}

/**
 * @brief Rotates a matrix about the X axis, in place.
 * @param tm matrix to rotate.
 * @param x  f32 radian angle.
 * @param r  true to pre-multiply (rotate in world space), false to
 *           post-multiply (rotate in the matrix's own space).
 */
static inline void rotateMatrixX(int32* tm, int32 x, bool r)
{
	int32_t rm[9], m[9];
	for(int i=0;i<9;i++)
        rm[i]=0;
	rm[0]=inttof32(1);
	rm[4]=cosLerp(x);
	rm[5]=sinLerp(x);
	rm[7]=-sinLerp(x);
	rm[8]=cosLerp(x);
	if(r)
        multMatrix33(rm,tm,m);
	else
        multMatrix33(tm,rm,m);
	memcpy(tm,m,9*sizeof(int32_t));
}

/**
 * @brief Rotates a matrix about the Y axis, in place.
 * @see rotateMatrixX for the meaning of @p r.
 */
static inline void rotateMatrixY(int32* tm, int32 x, bool r)
{
	int i;
	int32_t rm[9], m[9];
	for(i=0;i<9;i++)rm[i]=0;
	rm[0]=cosLerp(x);
	rm[2]=sinLerp(x);
	rm[4]=inttof32(1);
	rm[6]=-sinLerp(x);
	rm[8]=cosLerp(x);
	if(r)multMatrix33(rm,tm,m);
	else multMatrix33(tm,rm,m);
	memcpy(tm,m,9*sizeof(int32_t));
}

/**
 * @brief Rotates a matrix about the Z axis, in place.
 * @see rotateMatrixX for the meaning of @p r.
 */
static inline void rotateMatrixZ(int32* tm, int32 x, bool r)
{
	int32_t rm[9], m[9];
	for(int i=0;i<9;i++)
        rm[i]=0;
	rm[0]=cosLerp(x);
	rm[1]=sinLerp(x);
	rm[3]=-sinLerp(x);
	rm[4]=cosLerp(x);
	rm[8]=inttof32(1);
	if(r)
        multMatrix33(rm,tm,m);
	else
        multMatrix33(tm,rm,m);
	memcpy(tm,m,9*sizeof(int32));
}

/**
 * @brief Rotates a matrix about an arbitrary axis, in place.
 * @param tm matrix to rotate.
 * @param x  f32 radian angle.
 * @param a  rotation axis; must be normalised.
 * @param r  true to pre-multiply, false to post-multiply.
 */
void rotateMatrixAxis(int32_t * tm, int32_t x, vect3D a, bool r);


/**
 * @brief Removes the component of a vector along a given direction, in place.
 *
 * Despite the name @p n is a direction, not a plane: the result is @p v
 * projected onto the plane whose normal is @p n.
 *
 * @param v vector to project; ignored if NULL.
 * @param n plane normal; must be normalised.
 */
static inline void projectVectorPlane(vect3D* v, vect3D n)
{
	if(!v)
        return;
	int32_t r=dotProduct(*v,n);
	*v=vectDifference(*v,vectMult(n,r));
}

/**
 * @brief Re-orthonormalises a rotation matrix in place.
 *
 * Integrating an orientation matrix directly makes it drift away from being a
 * pure rotation - boxes visibly shear after a few seconds without this. Gram
 * Schmidt is applied with the Y column as the reference, then all three
 * columns are renormalised.
 *
 * @param m 3x3 matrix to fix up; ignored if NULL.
 */
void fixMatrix(int32_t * m); //3x3


#endif
