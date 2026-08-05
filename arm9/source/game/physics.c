/**
 * @file physics.c
 * @brief Swept sphere collision for the player and the camera.
 *
 * Implements @ref physics.h. Nothing here talks to the ARM7: rigid bodies are
 * simulated over there, while the player uses this much simpler move-then-push-
 * out scheme, which gives the crisp movement a first-person game needs.
 *
 * resolveSphereSurface() is the core: given the vector to the closest point of
 * a surface, it works out how far the sphere has sunk in and corrects the
 * position. checkObjectCollisionCell() feeds it every rectangle in the grid
 * cell the object stands in; collideRectangle() feeds it a bare rectangle for
 * the surfaces that are not room geometry - platforms and elevator floors.
 * The @c if(normGravityVector.x) branches are there because "down" is a
 * variable - see @ref changeGravity - so the code cannot assume Y is up.
 *
 * checkObjectElevatorCollision() is the odd one out: an elevator is a cylinder
 * the player must be kept *inside* rather than outside, which is the inverse of
 * every other case in the file.
 *
 * The author's note that used to sit here wished for octrees. The room grid
 * stays: rooms are flat grids of tiles, getCurrentCell() already narrows the
 * work to one cell's worth of rectangles, and a tree buys nothing over that
 * for geometry this shape.
 */

#include "game/game_main.h"

#define PLAYERSIZEY (TILESIZE*3)
#define PLAYERSIZEY2 (TILESIZE)
#define PLAYERSIZEX (TILESIZE-32)

#define ELEVATOR_RADIUS_IN (TILESIZE*2-64)
#define ELEVATOR_RADIUS_OUT (TILESIZE*2+128)
#define ELEVATOR_HEIGHT (TILESIZE*16)

#define ELEVATOR_ANGLE (3084)

vect3D gravityVector=(vect3D){0,-16,0};
vect3D normGravityVector=(vect3D){0,-4096,0};

bool pointInRoom(room_struct* r, vect3D p, vect3D* v)
{
	if(!r)return false;
	vect3D rp=vectDifference(p,convertVect(vect(r->position.x,0,r->position.y)));
	vect3D s=convertSize(vect(r->width,0,r->height));
	if(v)*v=rp;
	return rp.x>=0&&rp.z>=0&&rp.x<s.x&&rp.z<s.z;
}

bool objectInRoom(room_struct* r, physicsObject_struct* o, vect3D* v)
{
	if(!r || !o)return false;
	return pointInRoom(r,o->position,v);
}

vect3D convertCoord(room_struct* r, vect3D p)
{
	if(!r)return vect(0,0,0);
	vect3D rp=vectDifference(p,convertVect(vect(r->position.x,0,r->position.y)));
	// rp.x+=TILESIZE;
	// rp.z+=TILESIZE;
	if(rp.x>=0)rp.x/=TILESIZE*2;
	else rp.x=rp.x/(TILESIZE*2)-1;
	if(rp.z>=0)rp.z/=TILESIZE*2;
	else rp.z=rp.z/(TILESIZE*2)-1;
	if(rp.y>=0)rp.y/=HEIGHTUNIT;
	else rp.y=rp.y/(HEIGHTUNIT)-1;
	return rp;
}

/** How many times further (squared) the sphere reaches along gravity than
 *  laterally. Kept as a plain integer so resolveSphereSurface can divide by it
 *  with a compile-time-constant division - a multiply-high sequence - instead
 *  of a hardware divider call per rectangle. */
#define TRANSY_UNITS 5
const int32 transY=inttof32(TRANSY_UNITS);

/**
 * @brief Correction to apply when an object's centre lands exactly on a surface.
 *
 * The push-out below is along the vector from the centre to the closest point
 * on the surface, scaled by how far in the centre has sunk. When the centre
 * lands *on* that point the vector is zero: there is no direction left to push
 * along, and the scaling divides by the zero length. That is undefined - on the
 * DS the hardware divider simply returns whatever it happens to hold, so the
 * object was displaced by an arbitrary amount rather than resolved.
 *
 * Rare, but reachable: a portal exit places the player on a plane boundary, and
 * a step of a swept move can land there exactly.
 *
 * There is no direction in the geometry any more, so this takes one from the
 * motion - back out the way we came in - and falls back to straight up when the
 * object was not moving at all. Neither needs the surface's normal, whose sign
 * convention differs between here and the ARM7 (see transferRectangle()).
 *
 * @param o object being resolved.
 * @return the correction, or a zero vector if no direction can be found.
 */
static vect3D degenerateEscape(physicsObject_struct* o)
{
	vect3D back=vectMultInt(o->speed,-1);
	if(!back.x && !back.y && !back.z)back=vectMultInt(normGravityVector,-1);

	const int32 l=magnitude(back);
	if(!l)return vect(0,0,0);

	return vectMult(divideVect(back,l),o->radius);
}

/**
 * @brief Tests a sphere against a surface's closest point and pushes it out.
 *
 * The one piece of arithmetic every surface in this file goes through, whether
 * it arrived via the room grid or as a bare rectangle. @p v is the vector from
 * the object's centre to the closest point of the surface.
 *
 * The distance test is weighted: the component along gravity is divided by
 * transY, so the sphere reaches sqrt(transY) times further towards a floor
 * than towards a wall. That asymmetry is deliberate - it is what holds the
 * camera a comfortable height above the ground while letting the player stand
 * a bare radius from a wall.
 *
 * The overlap test runs at 64 bit and rounds once; the push-out length keeps
 * its raw sum of squares so that sqrtf32's <<12 lands it on the same scale as
 * @c radius<<6.
 *
 * @param o object being resolved; its position is corrected in place.
 * @param v centre-to-closest-point vector, in any space - only the difference matters.
 * @return true if the sphere overlapped and was moved.
 */
static bool resolveSphereSurface(physicsObject_struct* o, vect3D v)
{
	const int32 gval=dotProduct(v,normGravityVector);
	const vect3D lateral=vectDifference(v,vectMult(normGravityVector,gval));
	const int64_t sqLateral=(int64_t)lateral.x*lateral.x
	                       +(int64_t)lateral.y*lateral.y
	                       +(int64_t)lateral.z*lateral.z;

	//dividing by the constant TRANSY_UNITS instead of calling div64/divf32
	//with transY is arithmetically identical here (gval*gval>=0) and keeps
	//the most-executed division of the frame off the hardware divider.
	const int64_t sqGval=(int64_t)gval*gval;
	const int32 sqd=(int32)(sqLateral>>12)+(int32)(sqGval>>12)/TRANSY_UNITS;
	if(sqd>=o->sqRadius)return false;

	const int32 sqRaw=(int32)(sqLateral+(int32)sqGval/TRANSY_UNITS);
	const u32 d=sqrtf32(sqRaw);
	if(d)v=divideVect(vectMult(v,-((o->radius<<6)-d)),d);
	else v=degenerateEscape(o); //centre exactly on the surface
	o->position=addVect(o->position,v);
	return true;
}

bool checkObjectCollisionCell(gridCell_struct* gc, physicsObject_struct* o, room_struct* r)
{
	if(!gc || !o || !r)return false;

	const vect3D roomOrigin=convertVect(vect(r->position.x,0,r->position.y));
	vect3D o1=vectDifference(o->position,roomOrigin);

	//The cull box: one radius to each side, but five along gravity, because
	//the weighted test in resolveSphereSurface reaches sqrt(transY) times
	//further that way and the box must not cut it short.
	vect3D reach=vect(o->radius,o->radius,o->radius);
	if(normGravityVector.x)reach.x*=5;
	else if(normGravityVector.y)reach.y*=5;
	else reach.z*=5;

	int i;
	bool ret=false;
	for(i=0;i<gc->numRectangles;i++)
	{
		rectangle_struct* rec=gc->rectangles[i];
		if(!rec->collides)continue;

		//Cull on a single axis: the one the rectangle has no extent along.
		//The closest-point clamp below covers the other two.
		const vect3D p=vect(rec->position.x*TILESIZE*2,rec->position.y*HEIGHTUNIT,rec->position.z*TILESIZE*2);
		if(!rec->size.x)
		{
			if(p.x<o1.x-reach.x || p.x>o1.x+reach.x)continue;
		}else if(!rec->size.y)
		{
			if(p.y<o1.y-reach.y || p.y>o1.y+reach.y)continue;
		}else{
			if(p.z<o1.z-reach.z || p.z>o1.z+reach.z)continue;
		}

		//p is already the rectangle's world position - calling the Struct
		//variant would derive it a second time for every rectangle
		const vect3D s=vect(rec->size.x*TILESIZE*2,rec->size.y*HEIGHTUNIT,rec->size.z*TILESIZE*2);
		vect3D closest=getClosestPointRectangle(p,s,o1);
		if(portal1.used&&portal2.used)
		{
			//collidePortal works in world space; this is what keeps a surface
			//with a portal in it from pushing the player back out of the hole.
			closest=addVect(closest,roomOrigin);
			collidePortal(r,rec,&portal1,&closest);
			collidePortal(r,rec,&portal2,&closest);
			closest=vectDifference(closest,roomOrigin);
		}

		const bool touched=resolveSphereSurface(o,vectDifference(closest,o1));
		//only write on a change - the store dirties the rectangle's cache
		//line, and the vast majority of rectangles stay untouched
		if(rec->touched!=touched)rec->touched=touched;
		if(touched)
		{
			o1=vectDifference(o->position,roomOrigin);
			ret=true;
		}
	}
	return ret;
}

bool collideRectangle(physicsObject_struct* o, room_struct* r, vect3D p, vect3D s)
{
	if(!o||!r)return false;
	vect3D closest=getClosestPointRectangle(p,s,o->position);
	return resolveSphereSurface(o,vectDifference(closest,o->position));
}

u8 checkObjectElevatorCollision(physicsObject_struct* o, room_struct* r, elevator_struct* ev)
{
	if(!o || !r || !ev)return 0;

	u8 ret=0;

	if(collideRectangle(o,r,addVect(ev->realPosition,vect(-ELEVATOR_SIZE/2,0,-ELEVATOR_SIZE/2)),vect(ELEVATOR_SIZE,0,ELEVATOR_SIZE)))ret=2;

	//the height test is a subtract and a compare and rejects almost every
	//call - take it before spending a square root on the lateral distance
	if(abs(o->position.y-ev->position.y)>ELEVATOR_HEIGHT)return ret;

	vect3D u=vect(o->position.x-ev->position.x,0,o->position.z-ev->position.z);
	int32 v=magnitude(u);

	if(ev->state==ELEVATOR_OPEN)
	{
		const int32 cosAngle=cosLerp(ELEVATOR_ANGLE);
		switch(ev->direction&(~(1<<ELEVATOR_UPDOWNBIT)))
		{
			case 1:
				if(u.x<-mulf32(v,cosAngle))return ret;
				break;
			case 4:
				if(u.z>mulf32(v,cosAngle))return ret;
				break;
			case 5:
				if(u.z<-mulf32(v,cosAngle))return ret;
				break;
			default:
				if(u.x>mulf32(v,cosAngle))return ret;
				break;
		}
	}

	if(v<ELEVATOR_RADIUS_IN)
	{
		if(v+o->radius>=ELEVATOR_RADIUS_IN)
		{
			u=divideVect(vectMult(u,ELEVATOR_RADIUS_IN-o->radius-v),v);
			o->position=addVect(o->position,u);
			ret=1;
		}
	}else if(v<o->radius+ELEVATOR_RADIUS_OUT)
	{
		u=divideVect(vectMult(u,o->radius+ELEVATOR_RADIUS_OUT-v),v);
		o->position=addVect(o->position,u);
		ret=1;
	}

	return ret;
}

bool checkObjectCollision(physicsObject_struct* o, room_struct* r)
{
	// vect3D o1=vectDifference(o->position,convertVect(vect(r->position.x,0,r->position.y)));
	// o1.y-=128; //make ourselves taller

	bool ret=false;

	gridCell_struct* gc=getCurrentCell(r,o->position);
	if(!gc)return false;
	ret=ret||checkObjectCollisionCell(gc,o,r);

	//platforms
	int i;
	for(i=0;i<NUMPLATFORMS;i++)
	{
		if(platform[i].used && collideRectangle(o,r,addVect(platform[i].position,vect(-PLATFORMSIZE,0,-PLATFORMSIZE)),vect(PLATFORMSIZE*2,0,PLATFORMSIZE*2))) //add culling
		{
			platform[i].touched=true;
			ret=true;
		}
	}

	//elevators
	u8 val=0;
	if((entryWallDoor.used && checkObjectElevatorCollision(o,r,&entryWallDoor.elevator)) || (exitWallDoor.used && (val=checkObjectElevatorCollision(o,r,&exitWallDoor.elevator))))ret=true;
	if(val==2)closeElevator(&exitWallDoor.elevator);

	//timed buttons
	if(checkObjectTimedButtonsCollision(o,r))ret=true;

	return ret;
}

void changeGravity(vect3D v, int32 l)
{
	normGravityVector=v;
	gravityVector=vectMult(v,l);
}

void collideObjectRoom(physicsObject_struct* o, room_struct* r)
{
	if(!o)return;
	if(!r)return;
	vect3D pos=o->position;

	o->speed=addVect(o->speed, gravityVector);

	if(o->speed.y>800)o->speed.y=800;
	else if(o->speed.y<-800)o->speed.y=-800;

	int32 length=magnitude(o->speed);

	bool ret=false;

	if(length<200)
	{
		o->position=addVect(o->position,o->speed);
		ret=checkObjectCollision(o,r);
	}else{
		vect3D v=divideVect(o->speed,length);
		v=vectDivInt(v,32);

		while(length>128)
		{
			o->position=addVect(o->position,v);
			// Not ret=ret||checkObjectCollision(...): || short circuits, so once
			// anything had been touched - the floor underfoot on the very first
			// step, usually - every remaining step of the sweep skipped
			// collision entirely and the object slid the rest of the way
			// through whatever was in front of it.
			if(checkObjectCollision(o,r))ret=true;
			length-=128;
		}

		v=vectMult(v,length*32);
		o->position=addVect(o->position,v);
		checkObjectCollision(o,r);
	}

	o->contact=ret;

	vect3D os=o->speed;
	o->speed=vect(o->position.x-pos.x,o->position.y-pos.y,o->position.z-pos.z);
	o->speed=vect((os.x*o->speed.x>0)?(o->speed.x):(0),(os.y*o->speed.y>0)?(o->speed.y):(0),(os.z*o->speed.z>0)?(o->speed.z):(0));

	if(o->contact)
	{
		 //floor friction
		vect3D s=vectDifference(o->speed,vectMult(normGravityVector,dotProduct(normGravityVector,o->speed)));
		o->speed=vectDifference(o->speed,vectDivInt(s,2));
	}else{
		//air friction
		vect3D s=vectDifference(o->speed,vectMult(normGravityVector,dotProduct(normGravityVector,o->speed)));
		o->speed=vectDifference(o->speed,vectDivInt(s,32));
	}

	if(abs(o->speed.x)<3)o->speed.x=0;
	if(abs(o->speed.z)<3)o->speed.z=0;
}
