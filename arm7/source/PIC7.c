/**
 * @file PIC7.c
 * @brief ARM7 implementation of the shared portal transport maths.
 *
 * Implements the two non-inline functions declared in @ref PIC.h. The ARM9 has
 * its own copy of the same maths in PIC.c - the two must agree exactly, or a
 * cube would come out of a portal somewhere different from where it is drawn.
 *
 * @ref warpVector is the heart of it: decompose the vector in the entry
 * portal's frame, then rebuild it in the exit portal's frame with the normal
 * and first tangent negated. That double flip is a 180 degree turn about the
 * remaining axis, which is exactly what makes you emerge facing out of the far
 * portal rather than back into it.
 */

#include "stdafx.h"
#include "../../common/include/PIC.h"
ARM_CODE vect3D warpVector(portal_struct* p, vect3D v)
{
	if(!p)return vect(0,0,0);
	portal_struct* p2=p->targetPortal;
	if(!p2)return vect(0,0,0);
	
	// computePortalPlane(p2);
	
	int32 x=dotProduct(v,p->plane[0]);
	int32 y=dotProduct(v,p->plane[1]);
	int32 z=dotProduct(v,p->normal);
	
	return addVect(vectMult(p2->normal,-z),addVect(vectMult(p2->plane[0],-x),vectMult(p2->plane[1],y)));
}

void warpMatrix(portal_struct* p, int32* m) //3x3
{
	if(!m)return;
	
	vect3D x=warpVector(p,vect(m[0],m[3],m[6]));
	vect3D y=warpVector(p,vect(m[1],m[4],m[7]));
	vect3D z=warpVector(p,vect(m[2],m[5],m[8]));
	
	m[0]=x.x;m[3]=x.y;m[6]=x.z;
	m[1]=y.x;m[4]=y.y;m[7]=y.z;
	m[2]=z.x;m[5]=z.y;m[8]=z.z;
}
