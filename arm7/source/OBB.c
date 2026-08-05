/**
 * @file OBB.c
 * @brief The rigid body solver - integration, collision response and sleeping.
 *
 * Implements @ref OBB.h, and is where the ARM7 spends most of its time. The
 * approach follows Chris Hecker's rigid body dynamics articles: bodies carry
 * linear and angular momentum, contacts are resolved with instantaneous
 * impulses, and the timestep was meant to be bisected whenever a body ends up
 * too deeply penetrated, so that collisions are applied close to the true time
 * of impact. That last part does not currently happen - see the note on the
 * bisection branch in simulate().
 *
 * Reading order, roughly in the order the step executes:
 *  - simulate() drives one frame for one body: gravity, then the
 *    integrate/collide/bisect/impulse loop, then the sleep bookkeeping;
 *  - integrate() advances position, orientation and momenta, re-orthonormalises
 *    the orientation matrix and refreshes the world-space inverse inertia;
 *  - checkOBBCollisions() gathers contacts against other bodies and the static
 *    world;
 *  - @ref collideOBBs and @ref clipSegmentOBB do the box-box narrow phase;
 *  - applyOBBImpulsePlane() and applyOBBImpulseOBB() compute the response;
 *  - @ref updateOBBPortals teleports a body that crossed a portal.
 *
 * @par Fixed point caveats
 * The integrator carries the timestep as a 0.32 fixed point fraction of a
 * frame (hence all the @c >>32 shifts) so that bisecting it a dozen times does
 * not quantise to zero. This is also why @ref divv is used instead of a
 * general divide - its second operand is known to be small.
 */

#include "stdafx.h"

#define TIMEPREC (6) /**< Legacy timestep precision; superseded by the 0.32 fixed point time used in simulate(). */

contactPoint_struct contactPoints[MAXCONTACTPOINTS];

/*
 * Contacts are written through here rather than straight into the array,
 * because nothing used to check the count against MAXCONTACTPOINTS. Eight
 * bodies heaped on a finely tiled floor is enough to run past the end of it:
 * the box-box narrow phase alone can produce two contacts for each of twelve
 * segments against each of seven other bodies, and the static world adds more
 * on top of that. Measured with eight cubes heaped on a tiled floor: 29
 * box-box contacts and 8 from the world, against a buffer of 32.
 *
 * Contacts past the cap are dropped rather than replacing existing ones. There
 * is nothing to rank them by - every site but one sets penetration to zero -
 * and a dropped contact only means that surface goes unresolved for a frame,
 * which the next step picks up. Overwriting a contact that had already been
 * counted would be worse.
 */
contactPoint_struct* nextContactPoint(OBB_struct* o)
{
	if(!o || o->numContactPoints>=MAXCONTACTPOINTS)
        return NULL;
	return &o->contactPoints[o->numContactPoints++];
}

OBB_struct objects[NUMOBJECTS];

uint32_t coll, integ, impul;
uint8_t sleeping;

/*s16 divLUT[4097];

void initDivision(void)
{
	int i;
	for(i=1;i<4097;i++)
	{
		divLUT[i]=divv16(4096,i);
	}
	divLUT[0]=0;
}*/

static inline int32_t divv(int32_t a, int32_t b) // b in 1-4096
{
	/*return divf32(a,b);
	s32 r=(((s32)a)*((s32)divLUT[b]))>>12;
	return (s16)r;*/
	return divv16(a,b);
}

ARM_CODE void initOBB(OBB_struct* o, vect3D size, vect3D pos, int32_t mass, s32 cosine, s32 sine)
{
	if(!o)
        return;

	o->used=true;
	o->position=pos;
	o->angularMomentum=vect(0,0,0);
	o->numContactPoints=0;
	o->size=size;
	o->mass=mass;
	o->invMass=divv16(inttof32(1),mass);
	o->maxPenetration=0;

	o->energy=0;
	o->sleep=false;

	// createOBB overwrites its slot unconditionally and the ARM9 recycles ids,
	// so these have to be cleared too - otherwise a new box inherits the
	// previous occupant's state. A stale counter is the one that bites: it is
	// the number of calm frames behind the sleep heuristic, so a box spawned
	// into the slot of one that had settled falls asleep almost immediately,
	// freezing in mid-air.
	o->counter=0;
	o->portaled=false;
	o->groundID=-1;

	o->velocity=vect(0,0,0);
	o->angularVelocity=vect(0,0,0);
	o->moment=vect(0,0,0);
	o->forces=vect(0,0,0);

	initTransformationMatrix(o->transformationMatrix, cosine, sine);

	int32_t x2=mulf32(o->size.x,o->size.x);
	int32_t y2=mulf32(o->size.y,o->size.y);
	int32_t z2=mulf32(o->size.z,o->size.z);

    for(int i=0;i<9;i++)
        o->invInertiaMatrix[i]=0;
    //o->invInertiaMatrix[0]=divf32(inttof32(3),(mulf32(o->mass,(y2+z2))));
    //o->invInertiaMatrix[4]=divf32(inttof32(3),(mulf32(o->mass,(x2+z2))));
    //o->invInertiaMatrix[8]=divf32(inttof32(3),(mulf32(o->mass,(x2+y2))));
    o->invInertiaMatrix[0]=divv16(inttof32(3),(mulf32(o->mass,(y2+z2))));
    o->invInertiaMatrix[4]=divv16(inttof32(3),(mulf32(o->mass,(x2+z2))));
    o->invInertiaMatrix[8]=divv16(inttof32(3),(mulf32(o->mass,(x2+y2))));

	//temporary
	o->contactPoints=contactPoints;

	//rotateMatrixX(o->transformationMatrix,4096,false);
	//rotateMatrixZ(o->transformationMatrix,4096,false);

	if(portal[0].used&&portal[1].used)
	{
		updateOBBPortals(o,0,true);
		updateOBBPortals(o,1,true);
	}
}

void initOBBs(void)
{
	for(int i=0;i<NUMOBJECTS;i++)
	{
		objects[i].used=false;
	}
}

void copyOBB(OBB_struct* o1, OBB_struct* o2)
{
	if(!o1 || !o2)
        return;

	o2->angularMomentum=o1->angularMomentum;
	o2->numContactPoints=o1->numContactPoints;
	o2->mass=o1->mass;
	o2->invMass=o1->invMass;
	o2->position=o1->position;
	o2->size=o1->size;
	o2->maxPenetration=o1->maxPenetration;

	o2->velocity=o1->velocity;
	o2->angularVelocity=o1->angularVelocity;
	o2->forces=o1->forces;
	o2->moment=o1->moment;

	o2->energy=o1->energy;
	o2->sleep=o1->sleep;

	//every OBB shares the one global contactPoints buffer, so pointing o2 at
	//it carries the list o1 just counted - there is nothing to copy.
	o2->contactPoints=contactPoints;
	memcpy(o2->transformationMatrix,o1->transformationMatrix,sizeof(int32_t)*9);
	memcpy(o2->invInertiaMatrix,o1->invInertiaMatrix,sizeof(int32_t)*9);
	memcpy(o2->invWInertiaMatrix,o1->invWInertiaMatrix,sizeof(int32_t)*9);
}

bool collideAABB(vect3D o1, vect3D s1, vect3D o2, vect3D s2)
{
	// <, not <=, on the z term: the other five comparisons treat boxes that
	// exactly touch as overlapping and this one did not, so a pair meeting
	// flush along z was reported as clear while the same pair meeting flush
	// along x or y was not. Fixed point coordinates land on exact equality
	// often enough for that to matter - bodies come to rest on a grid.
	return !(o2.x>o1.x+s1.x || o2.y>o1.y+s1.y || o2.z>o1.z+s1.z
		  || o2.x+s2.x<o1.x || o2.y+s2.y<o1.y || o2.z+s2.z<o1.z);
}

vect3D projectPointAABB(vect3D size, vect3D p, vect3D* n)
{
	if(!n)
        return vect(0,0,0);
	vect3D v=p;
	*n=vect(0,0,0);

	/*if(p.x<-size.x){v.x=-size.x;n->x=-1;}
	else if(p.x>size.x){v.x=size.x;n->x=1;}

	if(p.y<-size.y){v.y=-size.y;n->y=-1;}
	else if(p.y>size.y){v.y=size.y;n->y=1;}

	if(p.z<-size.z){v.z=-size.z;n->z=-1;}
	else if(p.z>size.z){v.z=size.z;n->z=1;}*/

	// The chain is deliberate, and not the same thing as the commented out
	// version above it. Chained, exactly one component of n is ever set, so n
	// comes back as a single box face - which is what the caller needs, since
	// it uses n directly as a contact normal without normalising it. The
	// independent form would return a diagonal for a point outside on two
	// axes, and an impulse along a non-unit normal is not a contact response.
	//
	// The cost is that v is then the closest point only along that one axis,
	// so the penetration derived from it reads low for a corner. That is a
	// bias in the response, not a wrong direction, which is the right way
	// round for a solver to be inexact.
	if(p.x<-size.x){v.x=-size.x;n->x=-1;}
	else if(p.x>size.x){v.x=size.x;n->x=1;}
	else if(p.y<-size.y){v.y=-size.y;n->y=-1;}
	else if(p.y>size.y){v.y=size.y;n->y=1;}
	else if(p.z<-size.z){v.z=-size.z;n->z=-1;}
	else if(p.z>size.z){v.z=size.z;n->z=1;}

	if(!n->x && !n->y && !n->z)
	{
		int32_t d1=abs(p.x+size.x);int32_t d2=abs(p.x-size.x);
		int32_t d3=abs(p.y+size.y);int32_t d4=abs(p.y-size.y);
		int32_t d5=abs(p.z+size.z);int32_t d6=abs(p.z-size.z);
		int32_t d=min(d1,d2);
		d=min(d,min(d3,d4));
		d=min(d,min(d5,d6));
		if(d==d1){v.x=-size.x;n->x=-1;}
		else if(d==d2){v.x=size.x;n->x=1;}
		else if(d==d3){v.y=-size.y;n->y=-1;}
		else if(d==d4){v.y=size.y;n->y=1;}
		else if(d==d5){v.z=-size.z;n->z=-1;}
		else {v.z=size.z;n->z=1;}
	}

	return v;
}

ARM_CODE bool collideLineRectangle(vect3D ro, vect3D ru1, vect3D ru2, vect3D rn, int32_t rs1, int32_t rs2, vect3D o, vect3D v, int32_t d, vect3D* ip)
{
	int32_t p1=dotProduct(v,rn);
	if(abs(p1)>10) //margin of error
	{
		int32_t p2=dotProduct(vectDifference(ro,o),rn);
		//int32 k=divf32(p2,p1);
		int32_t k=divv16(p2,p1);
		if(k<0 || k>d)
            return false;
		vect3D i=addVect(o,vectMult(v,k));
		if(ip)
            *ip=i; //real position at this point
		i=vectDifference(i,ro);
		i=vect(dotProduct(i,ru1),dotProduct(i,ru2),0);

		return i.x>=0 && i.x<=rs1 && i.y>=0 && i.y<=rs2;
	}
	return false;
}

ARM_CODE bool clipSegmentOBB(int32_t* ss, vect3D *uu, vect3D* p1, vect3D* p2, vect3D vv, vect3D* uu1, vect3D* uu2, vect3D vv1, vect3D* n1, vect3D* n2, bool* b1, bool* b2, int32_t* k1, int32_t* k2)
{
	if(!p1 || !p2 || !uu1 || !uu2 || !n1 || !n2 || !b1 || !b2 || !k1 || !k2)
        return false;

	if(uu1->x<-ss[0])
	{
		if(uu2->x>-ss[0])
		{
			int32_t k=divv(abs(uu1->x+ss[0]),abs(vv1.x));
				*uu1=addVect(*uu1,vectMult(vv1,k));
				*p1=addVect(*p1,vectMult(vv,k));
			*k1=max(*k1,k);
			*n1=vect(-uu[0].x,-uu[0].y,-uu[0].z);
			*b1=true;
		}else 
            return false;
	}else{
		if(uu2->x<-ss[0])
		{
			int32_t k=divv(abs(uu1->x+ss[0]),abs(vv1.x));
				*uu2=addVect(*uu1,vectMult(vv1,k));
				*p2=addVect(*p1,vectMult(vv,k));
			*k2=min(*k2,k);
			*n2=vect(-uu[0].x,-uu[0].y,-uu[0].z);
			*b2=true;
		}
	}

	if(uu1->x<ss[0])
	{
		if(uu2->x>ss[0])
		{
			int32_t k=divv(abs(uu1->x-ss[0]),abs(vv1.x));
				*uu2=addVect(*uu1,vectMult(vv1,k));
				*p2=addVect(*p1,vectMult(vv,k));
			*k2=min(*k2,k);
			*n2=(uu[0]);
			*b2=true;
		}
	}else{
		if(uu2->x<ss[0])
		{
			int32_t k=divv(abs(uu1->x-ss[0]),abs(vv1.x));
				*uu1=addVect(*uu1,vectMult(vv1,k));
				*p1=addVect(*p1,vectMult(vv,k));
			*k1=max(*k1,k);
			*n1=(uu[0]);
			*b1=true;
		}else 
            return false;
	}


	if(uu1->y<-ss[1])
	{
		if(uu2->y>-ss[1])
		{
			int32_t k=divv(abs(uu1->y+ss[1]),abs(vv1.y));
				*uu1=addVect(*uu1,vectMult(vv1,k));
				*p1=addVect(*p1,vectMult(vv,k));
			*k1=max(*k1,k);
			*n1=vect(-uu[1].x,-uu[1].y,-uu[1].z);
			*b1=true;
		}else 
            return false;
	}else{
		if(uu2->y<-ss[1])
		{
			int32_t k=divv(abs(uu1->y+ss[1]),abs(vv1.y));
				*uu2=addVect(*uu1,vectMult(vv1,k));
				*p2=addVect(*p1,vectMult(vv,k));
			*k2=min(*k2,k);
			*n2=vect(-uu[1].x,-uu[1].y,-uu[1].z);
			*b2=true;
		}
	}

	if(uu1->y<ss[1])
	{
		if(uu2->y>ss[1])
		{
			int32_t k=divv(abs(uu1->y-ss[1]),abs(vv1.y));
				*uu2=addVect(*uu1,vectMult(vv1,k));
				*p2=addVect(*p1,vectMult(vv,k));
			*k2=min(*k2,k);
			*n2=(uu[1]);
			*b2=true;
		}
	}else{
		if(uu2->y<ss[1])
		{
			int32_t k=divv(abs(uu1->y-ss[1]),abs(vv1.y));
				*uu1=addVect(*uu1,vectMult(vv1,k));
				*p1=addVect(*p1,vectMult(vv,k));
			*k1=max(*k1,k);
			*n1=(uu[1]);
			*b1=true;
		}else return false;
	}


	if(uu1->z<-ss[2])
	{
		if(uu2->z>-ss[2])
		{
			int32_t k=divv(abs(uu1->z+ss[2]),abs(vv1.z));
				*uu1=addVect(*uu1,vectMult(vv1,k));
				*p1=addVect(*p1,vectMult(vv,k));
			*k1=max(*k1,k);
			*n1=vect(-uu[2].x,-uu[2].y,-uu[2].z);
			*b1=true;
		}else return false;
	}else{
		if(uu2->z<-ss[2])
		{
			int32_t k=divv(abs(uu1->z+ss[2]),abs(vv1.z));
				*uu2=addVect(*uu1,vectMult(vv1,k));
				*p2=addVect(*p1,vectMult(vv,k));
			*k2=min(*k2,k);
			*n2=vect(-uu[2].x,-uu[2].y,-uu[2].z);
			*b2=true;
		}
	}

	if(uu1->z<ss[2])
	{
		if(uu2->z>ss[2])
		{
			int32_t k=divv(abs(uu1->z-ss[2]),abs(vv1.z));
				*uu2=addVect(*uu1,vectMult(vv1,k));
				*p2=addVect(*p1,vectMult(vv,k));
			*k2=min(*k2,k);
			*n2=(uu[2]);
			*b2=true;
		}
	}else{
		if(uu2->z<ss[2])
		{
			int32_t k=divv(abs(uu1->z-ss[2]),abs(vv1.z));
				*uu1=addVect(*uu1,vectMult(vv1,k));
				*p1=addVect(*p1,vectMult(vv,k));
			*k1=max(*k1,k);
			*n1=(uu[2]);
			*b1=true;
		}else return false;
	}
	return true;
}

ARM_CODE void collideOBBs(OBB_struct* o1, OBB_struct* o2)
{
	if(!o1 || !o2)
        return;
	if(o1==o2)
        return;

	if(!collideAABB(vect(o1->AABBo.x-MAXPENETRATIONBOX,o1->AABBo.y-MAXPENETRATIONBOX,o1->AABBo.z-MAXPENETRATIONBOX),
					vect(o1->AABBs.x+2*MAXPENETRATIONBOX,o1->AABBs.y+2*MAXPENETRATIONBOX,o1->AABBs.z+2*MAXPENETRATIONBOX),
					vect(o2->AABBo.x-MAXPENETRATIONBOX,o2->AABBo.y-MAXPENETRATIONBOX,o2->AABBo.z-MAXPENETRATIONBOX),
					vect(o2->AABBs.x+2*MAXPENETRATIONBOX,o2->AABBs.y+2*MAXPENETRATIONBOX,o2->AABBs.z+2*MAXPENETRATIONBOX)))
        return;

	vect3D v[8],v2[8];
	getOBBVertices(o1,v);

	vect3D z1=vect(o1->transformationMatrix[0],o1->transformationMatrix[3],o1->transformationMatrix[6]);
	vect3D z2=vect(o1->transformationMatrix[1],o1->transformationMatrix[4],o1->transformationMatrix[7]);
	vect3D z3=vect(o1->transformationMatrix[2],o1->transformationMatrix[5],o1->transformationMatrix[8]);

	vect3D u1=vect(o2->transformationMatrix[0],o2->transformationMatrix[3],o2->transformationMatrix[6]);
	vect3D u2=vect(o2->transformationMatrix[1],o2->transformationMatrix[4],o2->transformationMatrix[7]);
	vect3D u3=vect(o2->transformationMatrix[2],o2->transformationMatrix[5],o2->transformationMatrix[8]);

	vect3D pp=vectDifference(o1->position,o2->position);

	getVertices(o1->size, vect(dotProduct(pp,u1),dotProduct(pp,u2),dotProduct(pp,u3)), vect(dotProduct(z1,u1),dotProduct(z1,u2),dotProduct(z1,u3)),
		vect(dotProduct(z2,u1),dotProduct(z2,u2),dotProduct(z2,u3)), vect(dotProduct(z3,u1),dotProduct(z3,u2),dotProduct(z3,u3)), v2);

	/*for(i=0;i<8;i++)
	{
		//v2[i]=vectDifference(v[i],o2->position);
		//v2[i]=vect(dotProduct(v2[i],u1),dotProduct(v2[i],u2),dotProduct(v2[i],u3));

		if(v2[i].x>-o2->size.x-MAXPENETRATIONBOX && v2[i].x<o2->size.x+MAXPENETRATIONBOX
		&& v2[i].y>-o2->size.y-MAXPENETRATIONBOX && v2[i].y<o2->size.y+MAXPENETRATIONBOX
		&& v2[i].z>-o2->size.z-MAXPENETRATIONBOX && v2[i].z<o2->size.z+MAXPENETRATIONBOX)
		{
			vect3D n;
			vect3D p=projectPointAABB(o2->size,v2[i],&n);
			o1->contactPoints[o1->numContactPoints].point=v[i];
			o1->contactPoints[o1->numContactPoints].normal=normalize(vect(n.x*u1.x+n.y*u2.x+n.z*u3.x,n.x*u1.y+n.y*u2.y+n.z*u3.y,n.x*u1.z+n.y*u2.z+n.z*u3.z));
			if(abs(n.x)+abs(n.y)+abs(n.z)!=1)
			{
				printf("erf\n");
			}
			o1->contactPoints[o1->numContactPoints].penetration=distance(v2[i],p);
			o1->contactPoints[o1->numContactPoints].penetration=0;
			o1->contactPoints[o1->numContactPoints].target=o2;
			o1->contactPoints[o1->numContactPoints].type=BOXCOLLISION;
			o1->maxPenetration=max(o1->maxPenetration,o1->contactPoints[o1->numContactPoints].penetration);
			o1->numContactPoints++;
		}
	}*/
	//optimize by working in o2 space ?
	vect3D vv2[8];
	getOBBVertices(o2,vv2);
	vect3D u[3];
	u[0]=vect(o1->transformationMatrix[0],o1->transformationMatrix[3],o1->transformationMatrix[6]);
	u[1]=vect(o1->transformationMatrix[1],o1->transformationMatrix[4],o1->transformationMatrix[7]);
	u[2]=vect(o1->transformationMatrix[2],o1->transformationMatrix[5],o1->transformationMatrix[8]);
	vect3D uu[3];
	uu[0]=vect(o2->transformationMatrix[0],o2->transformationMatrix[3],o2->transformationMatrix[6]);
	uu[1]=vect(o2->transformationMatrix[1],o2->transformationMatrix[4],o2->transformationMatrix[7]);
	uu[2]=vect(o2->transformationMatrix[2],o2->transformationMatrix[5],o2->transformationMatrix[8]);
	int32_t s[3];
	s[0]=o1->size.x;
	s[1]=o1->size.y;
	s[2]=o1->size.z;
	int32_t ss[3];
	ss[0]=o2->size.x;//+MAXPENETRATIONBOX;
	ss[1]=o2->size.y;//+MAXPENETRATIONBOX;
	ss[2]=o2->size.z;//+MAXPENETRATIONBOX;
	for(int i=0;i<NUMOBBSEGMENTS;i++)
	{
		vect3D uu1=v2[OBBSegments[i][0]];
		vect3D uu2=v2[OBBSegments[i][1]];
		const bool t=!((uu1.x<-ss[0] && uu2.x<-ss[0]) || (uu1.x>ss[0] && uu2.x>ss[0])
				  || (uu1.y<-ss[1] && uu2.y<-ss[1]) || (uu1.y>ss[1] && uu2.y>ss[1])
				  || (uu1.z<-ss[2] && uu2.z<-ss[2]) || (uu1.z>ss[2] && uu2.z>ss[2]));
		if(t)
		{
			do
            {
				const vect3D vv=u[OBBSegmentsPD[i][1]];
				const vect3D vv1=vect(dotProduct(vv,uu[0]),dotProduct(vv,uu[1]),dotProduct(vv,uu[2]));
				vect3D p1=v[OBBSegments[i][0]];
				vect3D p2=v[OBBSegments[i][1]];
				vect3D n1=vect(0,0,0);
				vect3D n2=vect(0,0,0);
				int32_t k1=0;
				int32_t k2=s[OBBSegmentsPD[i][1]]*2;
				bool b1=false;
				bool b2=false;
				if(!clipSegmentOBB(ss,uu,&p1,&p2,vv,&uu1,&uu2,vv1,&n1,&n2,&b1,&b2,&k1,&k2))
                {
                    //NOGBA("NO collision found\n");
                    break;
                }
                //NOGBA("finding contacts\n");

				if(b1&&b2)
				{
					//p1=addVect(p1,vectMult(vv,k1));
					//p2=addVect(p2,vectMult(vv,k2));
					contactPoint_struct* cp=nextContactPoint(o1);
					if(cp)
					{
						cp->point=vectDivInt(addVect(p1,p2),2);
						cp->type=TESTPOINT;
						vect3D n;
						vect3D oo=vectDivInt(addVect(uu1,uu2),2);
						vect3D p=projectPointAABB(o2->size,oo,&n);
						cp->normal=(vect(n.x*u1.x+n.y*u2.x+n.z*u3.x,n.x*u1.y+n.y*u2.y+n.z*u3.y,n.x*u1.z+n.y*u2.z+n.z*u3.z));
						cp->penetration=distance(p,oo);
						cp->target=o2;
					}
				}else{
					if(b1)
					{
						//p1=addVect(p1,vectMult(vv,k1));
						contactPoint_struct* cp=nextContactPoint(o1);
						if(cp)
						{
							cp->point=p1;
							cp->type=TESTPOINT;
							cp->normal=n1;
							cp->penetration=0;
							cp->target=o2;
						}
					}
					if(b2)
					{
						//p2=addVect(p2,vectMult(vv,k2));
						contactPoint_struct* cp=nextContactPoint(o1);
						if(cp)
						{
							cp->point=p2;
							cp->type=TESTPOINT;
							cp->normal=n2;
							cp->penetration=0;
							cp->target=o2;
						}
					}
				}
			}while(0);
		}
	}
}

void initTransformationMatrix(int32_t* m, s32 cosine, s32 sine)
{
	if(!m)
        return;
	for(int i=0;i<9;i++)
        m[i]=0;

	m[4]=inttof32(1);

	m[0]=cosine;
	m[6]=sine;

	m[2]=-sine;
	m[8]=cosine;
}

ARM_CODE void getVertices(vect3D s, vect3D p, vect3D u1, vect3D u2, vect3D u3, vect3D* v)
{
	int32_t m2[9];
	m2[0]=mulf32(u1.x,s.x);m2[3]=mulf32(u1.y,s.x);m2[6]=mulf32(u1.z,s.x);
	m2[1]=mulf32(u2.x,s.y);m2[4]=mulf32(u2.y,s.y);m2[7]=mulf32(u2.z,s.y);
	m2[2]=mulf32(u3.x,s.z);m2[5]=mulf32(u3.y,s.z);m2[8]=mulf32(u3.z,s.z);

	v[0]=vect(-m2[0]-m2[1]-m2[2],-m2[3]-m2[4]-m2[5],-m2[6]-m2[7]-m2[8]);
	v[1]=vect(m2[0]-m2[1]-m2[2],m2[3]-m2[4]-m2[5],m2[6]-m2[7]-m2[8]);
	v[2]=vect(m2[0]-m2[1]+m2[2],m2[3]-m2[4]+m2[5],m2[6]-m2[7]+m2[8]);
	v[3]=vect(-m2[0]-m2[1]+m2[2],-m2[3]-m2[4]+m2[5],-m2[6]-m2[7]+m2[8]);

	v[4]=vect(-v[1].x,-v[1].y,-v[1].z);
	v[5]=vect(-v[2].x,-v[2].y,-v[2].z);
	v[6]=vect(-v[3].x,-v[3].y,-v[3].z);
	v[7]=vect(-v[0].x,-v[0].y,-v[0].z);

	v[0]=addVect(v[0],p);v[1]=addVect(v[1],p);v[2]=addVect(v[2],p);v[3]=addVect(v[3],p);
	v[4]=addVect(v[4],p);v[5]=addVect(v[5],p);v[6]=addVect(v[6],p);v[7]=addVect(v[7],p);
}

ARM_CODE void getOBBVertices(OBB_struct* o, vect3D* v)
{
	if(!o || !v)
        return;
	int32_t* m=o->transformationMatrix;
	getVertices(o->size,o->position,vect(m[0],m[3],m[6]),vect(m[1],m[4],m[7]),vect(m[2],m[5],m[8]),v);
	vect3D mm=o->position;
	vect3D M=o->position;
	for(int i=0;i<8;i++)
	{
		//v[i]=addVect(v[i],o->position);
		if(v[i].x<mm.x)mm.x=v[i].x;
		if(v[i].y<mm.y)mm.y=v[i].y;
		if(v[i].z<mm.z)mm.z=v[i].z;
		if(v[i].x>M.x)M.x=v[i].x;
		if(v[i].y>M.y)M.y=v[i].y;
		if(v[i].z>M.z)M.z=v[i].z;
	}
	o->AABBo=mm;
	o->AABBs=vectDifference(M,mm);
}

ARM_CODE void applyOBBForce(OBB_struct* o, vect3D p, vect3D f)
{
	if(!o)return;
	o->forces=addVect(o->forces,f);
	o->moment=addVect(o->moment,vectProduct(vectDifference(p,o->position),f));
}

ARM_CODE void applyOBBImpulsePlane(OBB_struct* o, u8 pID)
{
	if(!o || pID>=o->numContactPoints)return;
	contactPoint_struct* cp=&o->contactPoints[pID];
	vect3D r=vectDifference(cp->point,o->position);
	vect3D v=addVect(o->velocity,vectProduct(o->angularVelocity,r));

	const int32_t CoefficientOfRestitution=floattof32(0.2f);

	int32_t iN=-mulf32((floattof32(1)+CoefficientOfRestitution),dotProduct(v,cp->normal));
	const int32_t invMass=o->invMass;
	int32_t iD=invMass+dotProduct(vectProduct(evalVectMatrix33(o->invWInertiaMatrix,vectProduct(r,cp->normal)),r),cp->normal);
    //iN=divf32(iN,iD);
    iN=divv16(iN,iD);
	if(iN<0)
        iN=0;
	//vect3D imp=vectMult(cp->normal,iN);
	vect3D imp=vectMult(cp->normal,iN+cp->penetration/2); //added bias adds jitter, but prevents sinking.

	//printf("imp : %d",iN);

    // apply impulse to primary quantities
	o->velocity=addVect(o->velocity,vectMult(imp,invMass));
	o->angularMomentum=addVect(o->angularMomentum,vectProduct(r,imp));

    // compute affected auxiliary quantities
	o->angularVelocity=evalVectMatrix33(o->invWInertiaMatrix,o->angularMomentum);

	{
		vect3D tangent=vect(0,0,0);
		tangent=vectDifference(v,(vectMult(cp->normal,dotProduct(v, cp->normal))));
		if(magnitude(tangent)<1)
            return;
		tangent=normalize(tangent);

		int32_t kTangent=invMass+dotProduct(tangent,vectProduct(evalVectMatrix33(o->invWInertiaMatrix,(vectProduct(r, tangent))), r));

		int32_t vt = dotProduct(v, tangent);
		//int32 dPt = divf32((-vt),kTangent);
		int32_t dPt = divv16((-vt),kTangent);

		const int32_t frictionCONST=floattof32(1.0f);

		int32_t maxPt=abs(mulf32(frictionCONST,iN));
		if(dPt<-maxPt)
            dPt=-maxPt;
		else if(dPt>maxPt)
            dPt=maxPt;

		// Apply contact impulse
		vect3D P = vectMult(tangent,dPt);

		o->velocity=addVect(o->velocity,vectMult(P,invMass));
		o->angularMomentum=addVect(o->angularMomentum,vectProduct(r,P));

		// compute affected auxiliary quantities
		o->angularVelocity=evalVectMatrix33(o->invWInertiaMatrix,o->angularMomentum);
	}
}

ARM_CODE void applyOBBImpulseOBB(OBB_struct* o, u8 pID)
{
	if(!o || pID>=o->numContactPoints)
        return;
	contactPoint_struct* cp=&o->contactPoints[pID];
	OBB_struct* o2=(OBB_struct*)cp->target;
	if(!o2)
        return;
	vect3D r1=vectDifference(cp->point,o->position);
	vect3D r2=vectDifference(cp->point,o2->position);
	vect3D v1=addVect(o->velocity,vectProduct(o->angularVelocity,r1));
	vect3D v2=addVect(o2->velocity,vectProduct(o2->angularVelocity,r2));
	vect3D dv=vectDifference(v1,v2);

	const int32_t CoefficientOfRestitution=floattof32(0.5f);

	int32_t iN=-mulf32((floattof32(1)+CoefficientOfRestitution),dotProduct(dv,cp->normal));
	const int32_t invMass1=o->invMass;
	const int32_t invMass2=o2->invMass;
	int32_t iD=invMass1+invMass2+dotProduct(addVect(vectProduct(evalVectMatrix33(o->invWInertiaMatrix,vectProduct(r1,cp->normal)),r1),vectProduct(evalVectMatrix33(o2->invWInertiaMatrix,vectProduct(r2,cp->normal)),r2)),cp->normal);
	//iN=divf32(iN,iD);
	iN=divv16(iN,iD);
	if(iN<0)
        iN=0;
	//vect3D imp=vectMult(cp->normal,iN);
	vect3D imp=vectMult(cp->normal,iN+cp->penetration); //added bias adds jitter, but prevents sinking.

	//printf("norm : %d %d %d\n",cp->normal.x,cp->normal.y,cp->normal.z);

    // apply impulse to primary quantities
	o->velocity=addVect(o->velocity,vectMult(imp,invMass1));
	o->angularMomentum=addVect(o->angularMomentum,vectProduct(r1,imp));

	o2->velocity=vectDifference(o2->velocity,vectMult(imp,invMass2));
	o2->angularMomentum=vectDifference(o2->angularMomentum,vectProduct(r2,imp));

    // compute affected auxiliary quantities
	o->angularVelocity=evalVectMatrix33(o->invWInertiaMatrix,o->angularMomentum);

	o2->angularVelocity=evalVectMatrix33(o2->invWInertiaMatrix,o2->angularMomentum);

	{
		vect3D tangent=vect(0,0,0);
		tangent=vectDifference(dv,(vectMult(cp->normal,dotProduct(dv, cp->normal))));
		if(magnitude(tangent)<1)
            return;
		tangent=normalize(tangent);

		int32_t kTangent=invMass1+invMass2+dotProduct(tangent,addVect(vectProduct(evalVectMatrix33(o->invWInertiaMatrix,(vectProduct(r1, tangent))), r1),
																	vectProduct(evalVectMatrix33(o->invWInertiaMatrix,(vectProduct(r2, tangent))), r2)));

		int32_t vt = dotProduct(dv, tangent);
		//int32 dPt = divf32((-vt),kTangent);
		int32_t dPt = divv16((-vt),kTangent);

		const int32_t frictionCONST=floattof32(0.5f);

		int32_t maxPt=abs(mulf32(frictionCONST,iN));
		if(dPt<-maxPt)
            dPt=-maxPt;
		else if(dPt>maxPt)
            dPt=maxPt;

		// Apply contact impulse
		vect3D P = vectMult(tangent,dPt);

		o->velocity=addVect(o->velocity,vectMult(P,invMass1));
		o->angularMomentum=addVect(o->angularMomentum,vectProduct(r1,P));

		o2->velocity=vectDifference(o2->velocity,vectMult(P,invMass2));
		o2->angularMomentum=vectDifference(o2->angularMomentum,vectProduct(r2,P));

		// compute affected auxiliary quantities
		o->angularVelocity=evalVectMatrix33(o->invWInertiaMatrix,o->angularMomentum);
		o2->angularVelocity=evalVectMatrix33(o2->invWInertiaMatrix,o2->angularMomentum);
	}
}

void applyOBBImpulses(OBB_struct* o)
{
	if(!o)
        return;
	for(int i=0;i<o->numContactPoints;i++)
	{
		switch(o->contactPoints[i].type)
		{
			case PLANECOLLISION:
				applyOBBImpulsePlane(o,i);
				break;
			case AARCOLLISION:
				applyOBBImpulsePlane(o,i);
				break;
			case BOXCOLLISION:
				// printf("collision ! %d",i);
				//applyOBBImpulseOBB(o,i);
				break;
			case TESTPOINT:
				// printf("test collision ! %d",i);
				applyOBBImpulseOBB(o,i);
				break;
		}
	}
}

ARM_CODE static void integrate(OBB_struct* o, int32_t DT)
{
	if(!o)
        return;

    //NOGBA("dt is %f\n", dt);
    int32_t x,y,z;
    x=o->position.x;
    y=o->position.y;
    z=o->position.z;
    int32_t vx=(int64_t)o->velocity.x;
    int32_t vy=(int64_t)o->velocity.y;
    int32_t vz=(int64_t)o->velocity.z;
    int32_t fx=((int64_t)o->forces.x*DT)>>32;
    int32_t fy=((int64_t)o->forces.y*DT)>>32;
    int32_t fz=((int64_t)o->forces.z*DT)>>32;
    int32_t nvx=vx+mulf32(fx,o->invMass);
    int32_t nvy=vy+mulf32(fy,o->invMass);
    int32_t nvz=vz+mulf32(fz,o->invMass);
    int32_t dx=((int64_t)(vx+nvx)*DT)>>32;
    int32_t dy=((int64_t)(vy+nvy)*DT)>>32;
    int32_t dz=((int64_t)(vz+nvz)*DT)>>32;
    o->position.x=x+(dx>>1);
    o->position.y=y+(dy>>1);
    o->position.z=z+(dz>>1);
    o->velocity.x=nvx;
    o->velocity.y=nvy;
    o->velocity.z=nvz;
	int32_t m[9], m2[9];
	m[0]=0;
    m[1]=((int64_t)-DT*o->angularVelocity.z)>>32;
    m[2]=((int64_t)DT*o->angularVelocity.y)>>32;
    m[3]=-m[1];
    m[4]=0;
    m[5]=((int64_t)-DT*o->angularVelocity.x)>>32;
    m[6]=-m[2];
    m[7]=-m[5];
    m[8]=0;
	multMatrix33(m,o->transformationMatrix,m2);
	addMatrix33(o->transformationMatrix,m2,o->transformationMatrix);
	fixMatrix(o->transformationMatrix);
    // compute auxiliary quantities
	transposeMatrix33(o->transformationMatrix,m2);
	multMatrix33(m2,o->invInertiaMatrix,m);
	multMatrix33(m,o->transformationMatrix,o->invWInertiaMatrix);
    o->angularMomentum.x+=((int64_t)o->moment.x*DT)>>32;
    o->angularMomentum.y+=((int64_t)o->moment.y*DT)>>32;
    o->angularMomentum.z+=((int64_t)o->moment.z*DT)>>32;
	o->angularVelocity=evalVectMatrix33(o->invWInertiaMatrix,o->angularMomentum);

}

//extern plane_struct testPlane;

void checkOBBCollisions(OBB_struct* o, bool sleep)
{
	if(!o)
        return;
	o->numContactPoints=0;
	// planeOBBContacts(&testPlane,o);
	for(int i=0;i<NUMOBJECTS;i++)
	{
		if(objects[i].used && o!=&objects[i] && (!sleep || !objects[i].sleep))
		{
			collideOBBs(o,&objects[i]);
		}
	}
	AARsOBBContacts(o, sleep);
}

void wakeOBBs(void)
{
	for(int i=0;i<NUMOBJECTS;i++)
	{
		if(objects[i].used)
		{
			objects[i].counter=0;
			objects[i].sleep=false;
		}
	}
}

ARM_CODE void calculateOBBEnergy(OBB_struct* o)
{
	if(!o)
        return;

	u32 tmp=dotProduct(o->velocity,o->velocity)+dotProduct(o->angularVelocity,o->angularVelocity);
	// o->energy=(o->energy*9+tmp)/10;
	o->energy=tmp;
}



ARM_CODE static void simulate(OBB_struct* o, int32_t dt2)
{
	if(!o)
        return;
    int32_t dt=dt2*(1<<20);
    int32_t currentTime=0;
    int32_t targetTime=dt;
	//gravity acts through the centre of mass, so the applyOBBForce lever arm
	//would be exactly zero - add it to the force accumulator directly rather
	//than paying a cross product for a guaranteed-zero torque.
	o->forces.y-=inttof32(2);
	//o->forces=addVect(o->forces,vectDivInt(o->velocity,-25)); //some weird friction force?
	//o->moment=addVect(o->moment,vectDivInt(o->angularVelocity,-20));//another weird friction?
	if(!o->sleep)
	{
		while(currentTime<dt)
		{
			integrate(o,(targetTime-currentTime));
			checkOBBCollisions(o, false);
			// There used to be a timestep bisection here, rolling back to a
			// copyOBB backup when maxPenetration exceeded its threshold. Its
			// writers were already commented out, so the branch never ran and
			// the backup was 40 dead struct copies a frame; both are gone (see
			// git history). Reviving it is a physics change - contacts would
			// start being resolved near the time of impact rather than at the
			// end of the step - so it wants play testing, not just restoring.
			if(o->numContactPoints)applyOBBImpulses(o);
			currentTime=targetTime;
			targetTime=dt;
		}
		// collideSpherePlatforms(&o->position,o->size.x-8);
	}else 
        sleeping++;

	calculateOBBEnergy(o);
	bool oldSleep=o->sleep;
	if(o->energy>=SLEEPTHRESHOLD*10)o->sleep=false;
	else if(o->energy<=SLEEPTHRESHOLD)
	{
		o->counter++;
		if(o->counter>=SLEEPTIMETHRESHOLD)o->sleep=true;
	}else o->counter=0;

	if(o->sleep)
	{
		if(oldSleep)
            checkOBBCollisions(o, true); //make it so sleeping collisions don't have to check vs AARs and other sleeping OBBs
		{
			bool canSleep=oldSleep;
			for(int i=0;i<o->numContactPoints;i++)
			{
				switch(o->contactPoints[i].type)
				{
					case AARCOLLISION:
						canSleep=true;
						break;
					case PLANECOLLISION:
						canSleep=true;
						break;
					case TESTPOINT:
					case BOXCOLLISION:
						{
							OBB_struct* o2=(OBB_struct*)o->contactPoints[i].target;
							if(o2->energy<=SLEEPTHRESHOLD && o2->counter>=SLEEPTIMETHRESHOLD && o->energy<=SLEEPTHRESHOLD)
							{
								canSleep=true;
								o->sleep=true;
								o2->sleep=true;
								// i=o->numContactPoints; //get a similar system to work with more than 2 boxes in contact ? (only make it for !o2->sleep ?)
							}else{
								canSleep=false;
								o2->sleep=false;
								i=o->numContactPoints;
							}
						}
						break;
				}
			}
			if(!canSleep)
                o->sleep=false;
		}
	}

	o->forces=vect(0,0,0);
	o->moment=vect(0,0,0);
	//o->forces=vectDivInt(o->forces,2);
	//o->moment=vectDivInt(o->moment,2);
}

ARM_CODE bool pointInFrontOfPortal(portal_struct* p, vect3D pos, int32* z) //assuming correct normal
{
	if(!p)
        return false;
	const vect3D v2=vectDifference(pos, p->position); //then, project onto portal base
	vect3D v=vect(dotProduct(p->plane[0],v2),dotProduct(p->plane[1],v2),0);
	*z=dotProduct(p->normal,v2);
	return (v.y>-PORTALSIZEY*4 && v.y<PORTALSIZEY*4 && v.x>-PORTALSIZEX*4 && v.x<PORTALSIZEX*4);
}

ARM_CODE void updateOBBPortals(OBB_struct* o, u8 id, bool init)
{
	// || and >=, not && and <: as written this returned only for a NULL o with
	// an in-range id, which is the one combination that needed no guard. A
	// NULL o or an id past the two portal slots went straight through into
	// o->oldPortal[id] and portal[id].
	if(!o || id>=2)
        return;

	int32_t z;
	o->oldPortal[id]=o->portal[id];
	o->portal[id]=((dotProduct(vectDifference(o->position,portal[id].position),portal[id].normal)>0)&1)|(((pointInFrontOfPortal(&portal[id],o->position,&z))&1)<<1);

	if(init)
	{
		o->oldPortal[id]=o->portal[id];
	}
	else
	{
		if(((o->oldPortal[id]&1) && !(o->portal[id]&1) && (o->oldPortal[id]&2 || o->portal[id]&2)) || (o->portal[id]&2 && z<=0 && z>-16))
			{
				o->position=addVect(portal[id].targetPortal->position,warpVector(&portal[id],vectDifference(o->position,portal[id].position)));
				o->velocity=warpVector(&portal[id],o->velocity);
				o->forces=warpVector(&portal[id],o->forces);
				o->angularVelocity=warpVector(&portal[id],o->angularVelocity);
				o->moment=warpVector(&portal[id],o->moment);

				warpMatrix(&portal[id], o->transformationMatrix);
				warpMatrix(&portal[id], o->invWInertiaMatrix);

				o->portaled=true;
			}
	}

}

static void updateOBB(OBB_struct* o)
{
	if(!o)return;

	simulate(o,20);

	if(portal[0].used&&portal[1].used)
	{
		updateOBBPortals(o,0,false);
		updateOBBPortals(o,1,false);
	}
}

ARM_CODE __attribute__((noinline)) void updateOBBs(void)
{
	sleeping=0;
	for(int i=0;i<NUMOBJECTS;i++)
	{
		if(objects[i].used)
		{
			updateOBB(&objects[i]);
		}
	}
}

OBB_struct* createOBB(u8 id, vect3D size, vect3D position, int32_t mass, s32 cosine, s32 sine)
{
	int i=id;
	//if(!objects[i].used)
	{
			initOBB(&objects[i],size,position,mass,cosine,sine);
			return &objects[i];
	}
	return NULL;
}
#if 0

void drawOBBs(void)
{
	for(int i=0;i<NUMOBJECTS;i++)
	{
		if(objects[i].used)
		{
			drawOBB(&objects[i]);
		}
	}
}

void drawOBB(OBB_struct* o)
{
    return;
}
#endif
