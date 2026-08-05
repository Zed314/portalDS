/**
 * @file portals.c
 * @brief Portals: placement, rendering, and walking through them.
 *
 * Implements @ref portals.h - the heart of the game.
 *
 * @par Rendering
 * @ref updatePortalCamera warps the player's camera through to the far portal,
 * both position and each column of the orientation matrix.
 * @ref drawPortalRoom then renders the room from there into
 * portal_struct::viewPoint, and @ref drawPortal draws that capture onto the
 * portal's outline. The outline is a clipped ellipse, not a quad - see
 * @ref polygon.h.
 *
 * @par Walking through
 * checkPortalPlayerWarp() watches the sign of the player's distance to the
 * portal plane, remembered in portal_struct::oldZ. When it flips while the
 * player is inside the outline, warpPlayer() moves the camera and its velocity
 * to the far side. Rigid bodies cross independently on the ARM7 - see
 * @ref updateOBBPortals.
 *
 * @par Placement
 * @ref movePortal with @c actualMove clear is a trial placement: geometry only,
 * nothing sent to the ARM7 and no display list rebuilt. That is what
 * @ref isPortalOnWall uses while it slides a candidate portal around looking
 * for a position that fits entirely on one portalable surface, and
 * @ref portalToPortalIntersection then checks it does not overlap the other
 * portal.
 */

#include "game/game_main.h"

portal_struct portal1, portal2;
portal_struct* currentPortal;

/**
 * The colour-to-portal pairing everything relies on: the shot placement in
 * shootPlayerGun, the firing sound pick in controls.c, the gun tint and the
 * touch button. It has to agree with the colours initPortals paints, which is
 * why it lives next to them.
 */
portal_struct* portalForColor(bool orange)
{
	return orange?(&portal1):(&portal2);
}


void initPortals(void)
{
	initPolygonPool();

	initPortal(&portal1, vect(256*4,1024*2,0), vect(inttof32(1),0,0), true);
	initPortal(&portal2, vect(2048,1024+512,0), vect(0,inttof32(1),0), false);

	portal1.targetPortal=&portal2;
	portal2.targetPortal=&portal1;

	currentPortal=&portal1;
}

void freePortals(void)
{
	if(portal1.displayList){free(portal1.displayList);portal1.displayList=NULL;}
	if(portal2.displayList){free(portal2.displayList);portal2.displayList=NULL;}
}

void resetPortals(void)
{
	portal1.used=portal2.used=false;
	resetPortalsPI();
}

extern u32 debugVal; //TEMP

void movePortal(portal_struct* p, vect3D pos, vect3D normal, vect3D plane0, bool actualMove)
{
	if(!p)return;
	p->position=pos;
	p->normal=normal;
	p->plane[0]=plane0;
	p->animCNT=0;

	// p->angle=angle;
	p->oldZ=-1;

	computePortalPlane(p);

	if(actualMove)
	{
		updatePortalPI(p==&portal2,p->position,p->normal,p->plane[0]);
		if(p->displayList)free(p->displayList);
		p->displayList=NULL;
		debugVal=0;
		p->displayList=generateRoomDisplayList(NULL, p->position, p->normal, true);
		// debugVal=getMemFree()/1024; //TEMP
		p->used=true;
	}

	const vect3D v1=vectDivInt(p->plane[0],PORTALFRACTIONX);
	const vect3D v2=vectDivInt(p->plane[1],PORTALFRACTIONY);

	freePolygon(&p->unprojectedPolygon);
	p->unprojectedPolygon=createEllipse(vect(0,0,0), v1, v2, 32);

	freePolygon(&p->unprojectedOutline);
	p->unprojectedOutline=createEllipseOutline(vect(0,0,0), v1, v2, vectMult(v1,inttof32(11)/10), vectMult(v2,inttof32(11)/10), vect(0,0,0), 32);
	freePolygon(&p->outline);
	p->outline=createEllipseOutline(vect(0,0,0), v1, v2, vectMult(v1,inttof32(11)/10), vectMult(v2,inttof32(11)/10), vectDivInt(p->normal,256), 32);

	NOGBA("PORTAL ! %ld %ld %ld",p->position.x,p->position.y,p->position.z);
}

void initPortal(portal_struct* p, vect3D pos, vect3D normal, bool color)
{
	if(!p)return;
	p->position=pos;
	p->targetPortal=NULL;
	if(color){p->color=RGB15(31,31,0);}
	else {p->color=RGB15(0,31,31);}
	initCamera(&p->camera);

	p->oldZ=-1;
	p->used=false;
	// The assignment from the parameter used to be immediately overwritten by
	// a hardcoded +z, so the argument every caller passes was discarded. It
	// only shows before the portal is first shot - movePortal sets the real
	// orientation - but a function that ignores its own argument is a trap.
	p->normal=normal;
	p->plane[0]=vect(0,inttof32(1),0);
	computePortalPlane(p);

	p->displayList=NULL;

	p->polygon=NULL;
	p->unprojectedPolygon=NULL;

	p->outline=NULL;
	p->unprojectedOutline=NULL;
}


void drawPortal(portal_struct* p)
{
	if(!p || !p->used)return;
	// glPolyFmt(POLY_ALPHA(31) | (1<<14) | POLY_CULL_BACK | POLY_ID(32));
	// glPolyFmt(POLY_ALPHA(31) | POLY_DECAL | POLY_CULL_BACK);
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_BACK);
	// glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);

	player_struct* pl=getPlayer();
	int32 dist=distance(pl->object->position,p->position);

	if(dist<inttof32(1)/6)
	{
		glPushMatrix();
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
				glLoadIdentity();
				glOrthof32(0, inttof32(255), inttof32(191), 0, 0, inttof32(1));

				glMatrixMode(GL_MODELVIEW);
				glLoadIdentity();

				unbindMtl();
				GFX_COLOR=p->color;

				glScalef32(1<<24, 1<<24, 1<<24);

				drawPolygon(p->polygon);
				freePolygon(&p->polygon);

				glMatrixMode(GL_PROJECTION);
			glPopMatrix(1);
			glMatrixMode(GL_MODELVIEW);
		glPopMatrix(1);

		glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
		glPushMatrix();
			const vect3D v=p->position;
			glTranslate3f32(v.x,v.y,v.z);
			drawPolygonStrip(p->outline,p->innerOutlineColor,p->outlineColor);
		glPopMatrix(1);
	}
	else
	{
		glPushMatrix();
			const vect3D v=addVect(p->position,vectDivInt(p->normal,64));
			glTranslate3f32(v.x,v.y,v.z);

			unbindMtl();
			GFX_COLOR=p->color;
			drawPolygon(p->unprojectedPolygon);
			drawPolygonStrip(p->unprojectedOutline,p->innerOutlineColor,p->outlineColor);
		glPopMatrix(1);
	}
}

void getInvertedNormal(vect3D* n){if(!n->z)*n=vectMultInt(*n,-1);}
// void getInvertedNormal(vect3D* n){*n=vectMultInt(*n,-1);}

bool isPointInPortal(portal_struct* p, vect3D o, vect3D *v, int32* x, int32* y, int32* z)
{
	if(!x || !y || !z || !v || !p)return false;
	*v=vectDifference(o,p->position);
	const vect3D u1=p->plane[0], u2=p->plane[1];
	*x=dotProduct(*v,u1);
	*y=dotProduct(*v,u2);
	*z=dotProduct(*v,p->normal);
	return !(*x<-PORTALSIZEX || *y<-PORTALSIZEY || *x>=PORTALSIZEX || *y>=PORTALSIZEY);
}

u16 getCurrentPortalColor(vect3D o)
{
	u16 col=0;
	u32 dist=inttof32(10);
	int32 x, y, z;
	vect3D v;
	if(isPointInPortal(&portal1,o,&v,&x,&y,&z))
	{
		dist=abs(z);
		col=portal1.color;
	}
	if(isPointInPortal(&portal2,o,&v,&x,&y,&z))
	{
		if(z<dist)
		{
			dist=abs(z);
			col=portal2.color;
		}
	}
	if(dist>512)col=0;
	return col;
}

extern u16 mainScreen[256*192];

void warpPlayer(portal_struct* p, player_struct* pl)
{
	camera_struct* c=getPlayerCamera();
	updatePortalCamera(p,c);

	c->viewPosition=p->camera.viewPosition;
	pl->object->position=c->position=reverseViewPosition(p->camera.viewPosition);

	int32 z=dotProduct(vectDifference(c->position,p->targetPortal->position),p->targetPortal->normal);
	if(z<0)pl->object->position=c->position=addVect(c->position,vectMult(p->targetPortal->normal,-2*z));
	c->viewPosition=getViewPosition(c->position);

	pl->object->speed=warpVector(p,pl->object->speed);
	memcpy(c->transformationMatrix,p->camera.transformationMatrix,9*sizeof(int32));
	updateViewMatrix(c);
	updateFrustum(c);
	dmaCopy(mainScreen, p->targetPortal->viewPoint, 256*192*2);
}

void checkPortalPlayerWarp(portal_struct* p)
{
	if(!p)return;
	player_struct* pl=getPlayer();
	vect3D v;
	int32 x, y, z = 0;
	bool r=isPointInPortal(p, pl->object->position, &v, &x, &y, &z);
	if(r)
	{
		if(z<0 && p->oldZ>=0){currentPortal=p;warpPlayer(p,pl);gravityGunTarget=-1;}
		//Only ever set here. Whether the player is in a portal is one fact per
		//frame rather than one per portal, so updatePortals() is what clears it
		//and remembers the previous value; see the note there.
		if(abs(z)<PLAYERRADIUS)pl->inPortal=true;
	}
	p->oldZ=z;
}

void updatePortal(portal_struct* p)
{
	if(!p || !p->used)return;

	if(portal1.used&&portal2.used)
	{
		player_struct* pl=getPlayer();

		int32 dist=distance(pl->object->position,p->position);
		if(dist<inttof32(1)/6)
		{
			freePolygon(&p->polygon);
			p->polygon=createEllipse(p->position, vectDivInt(p->plane[0],PORTALFRACTIONX), vectDivInt(p->plane[1],PORTALFRACTIONY), 32);
			projectPolygon(NULL, &p->polygon);
		}
	}

	const u16 range=10;
	s8 x=abs((((p->animCNT)/2)%(range*2))-range)-range/2;

	p->innerOutlineColor=(p->color==RGB15(31,31,0))?(RGB15(27,27,0)):(RGB15(0,27,27));
	p->outlineColor=(p->color==RGB15(31,31,0))?(RGB15(31,16+x,0)):(RGB15(0,16+x,31));

	p->animCNT++;
}

void updatePortals(void)
{
	if(portal1.used&&portal2.used)
	{
		/*
		 * Take the reading once for the frame, then let either portal set it.
		 *
		 * Both calls below used to copy inPortal into oldInPortal themselves
		 * and then overwrite inPortal, so the second one read the first one's
		 * answer as though it were last frame's. isPointInPortal() tests only
		 * the two in-plane axes, which makes it true for the whole column
		 * through a portal rather than just the mouth of it - so standing in
		 * one portal while the other faces it, which is the arrangement the
		 * game is built around, left the pair disagreeing every single frame.
		 * The edge test in updatePlayer() then fired the enter or exit sound
		 * on every frame for as long as the player stood there.
		 */
		player_struct* pl=getPlayer();
		pl->oldInPortal=pl->inPortal;
		pl->inPortal=false;

		checkPortalPlayerWarp(&portal1);
		checkPortalPlayerWarp(&portal2);
	}
	updatePortal(&portal1);
	updatePortal(&portal2);
}

void updatePortalCamera(portal_struct* p, camera_struct* c)
{
	if(!p||!p->targetPortal)return;
	if(!c)c=getPlayerCamera();

	portal_struct* p2=p->targetPortal;

	p->camera.position=addVect(p2->position,warpVector(p, vectDifference(c->position, p->position)));
	p->camera.viewPosition=addVect(p2->position,warpVector(p, vectDifference(c->viewPosition, p->position)));

	memcpy(p->camera.transformationMatrix,c->transformationMatrix,9*sizeof(int32));

	computePortalPlane(p);

	vect3D x=vect(p->camera.transformationMatrix[0],p->camera.transformationMatrix[3],p->camera.transformationMatrix[6]);
	vect3D y=vect(p->camera.transformationMatrix[1],p->camera.transformationMatrix[4],p->camera.transformationMatrix[7]);
	vect3D z=vect(p->camera.transformationMatrix[2],p->camera.transformationMatrix[5],p->camera.transformationMatrix[8]);

	x=vect(dotProduct(x,p->plane[0]),dotProduct(x,p->plane[1]),dotProduct(x,p->normal));
	y=vect(dotProduct(y,p->plane[0]),dotProduct(y,p->plane[1]),dotProduct(y,p->normal));
	z=vect(dotProduct(z,p->plane[0]),dotProduct(z,p->plane[1]),dotProduct(z,p->normal));

	computePortalPlane(p2);

	vect3D x2=addVect(vectMult(p2->plane[0],-x.x),addVect(vectMult(p2->plane[1],x.y),vectMult(p2->normal,-x.z)));
	vect3D y2=addVect(vectMult(p2->plane[0],-y.x),addVect(vectMult(p2->plane[1],y.y),vectMult(p2->normal,-y.z)));
	vect3D z2=addVect(vectMult(p2->plane[0],-z.x),addVect(vectMult(p2->plane[1],z.y),vectMult(p2->normal,-z.z)));

	p->camera.transformationMatrix[0]=x2.x;p->camera.transformationMatrix[3]=x2.y;p->camera.transformationMatrix[6]=x2.z;
	p->camera.transformationMatrix[1]=y2.x;p->camera.transformationMatrix[4]=y2.y;p->camera.transformationMatrix[7]=y2.z;
	p->camera.transformationMatrix[2]=z2.x;p->camera.transformationMatrix[5]=z2.y;p->camera.transformationMatrix[8]=z2.z;
}

void drawPortalRoom(portal_struct* p)
{
	room_struct* r=getPlayer()->currentRoom;
	if(!r || !p)return;

	glPushMatrix();
		glTranslate3f32(TILESIZE*2*r->position.x, 0, TILESIZE*2*r->position.y);
		glTranslate3f32(-TILESIZE,0,-TILESIZE);
		glScalef32((TILESIZE*2)<<7,(HEIGHTUNIT)<<7,(TILESIZE*2)<<7);
		if(currentPortal->targetPortal && currentPortal->targetPortal->displayList)glCallList(currentPortal->targetPortal->displayList);
	glPopMatrix(1);
}

static const u8 segmentList[4][2]={{0,1},{1,2},{2,3},{3,0}};
static const u8 segmentNormal[4]={0,1,0,1};

bool doSegmentsIntersect(vect3D s1, vect3D s2, vect3D sn, vect3D p1, vect3D p2, vect3D pn, bool *vv1, bool *vv2)
{
	bool v1=dotProduct(vectDifference(p1,s1),pn)>0;
	bool v2=dotProduct(vectDifference(p1,s2),pn)>0;
	bool v3=dotProduct(vectDifference(s1,p1),sn)>0;
	bool v4=dotProduct(vectDifference(s1,p2),sn)>0;
	*vv1=v1;*vv2=v2;
	return (v1!=v2)&&(v3!=v4);
}

bool portalRectangleIntersection(room_struct* r, portal_struct* p, rectangle_struct* rec, vect3D origpos, bool fix)
{
	if(!p || !rec)return false;
	if((rec->normal.x&&p->normal.x)||(rec->normal.z&&p->normal.z)||(rec->normal.y&&p->normal.y))return false;
	vect3D pr=addVect(convertVect(vect(r->position.x,0,r->position.y)),vect(rec->position.x*TILESIZE*2,rec->position.y*HEIGHTUNIT,rec->position.z*TILESIZE*2));
	vect3D s=convertSize(rec->size);

	//only need to do this once for all rectangles (so optimize it out)
	const vect3D v[]={(vectDivInt(p->plane[0],PORTALFRACTIONX)), (vectDivInt(p->plane[1],PORTALFRACTIONY))};
	const vect3D points[]={addVect(addVect(p->position,v[0]),v[1]),vectDifference(addVect(p->position,v[0]),v[1]),vectDifference(vectDifference(p->position,v[0]),v[1]),addVect(vectDifference(p->position,v[0]),v[1])};

	//projection = more elegant, less efficient ? (rectangles are axis aligned biatch)
	if(p->normal.x)
	{
		if(!((pr.x-PORTALMARGIN<=p->position.x&&pr.x+s.x+PORTALMARGIN>=p->position.x)||(pr.x+PORTALMARGIN>=p->position.x&&pr.x+s.x-PORTALMARGIN<=p->position.x)))return false;
	}else if(p->normal.y)
	{
		if(!((pr.y-PORTALMARGIN<=p->position.y&&pr.y+s.y+PORTALMARGIN>=p->position.y)||(pr.y+PORTALMARGIN>=p->position.y&&pr.y+s.y-PORTALMARGIN<=p->position.y)))return false;
	}else{
		if(!((pr.z-PORTALMARGIN<=p->position.z&&pr.z+s.z+PORTALMARGIN>=p->position.z)||(pr.z+PORTALMARGIN>=p->position.z&&pr.z+s.z-PORTALMARGIN<=p->position.z)))return false;
	}

	vect3D p1=pr, p2=addVect(pr,s);
	vect3D pn=rec->normal;

	int i;
	for(i=0;i<4;i++)
	{
		const vect3D s1=points[segmentList[i][0]], s2=points[segmentList[i][1]]; //segment
		bool v1, v2;
		if(doSegmentsIntersect(s1,s2,v[segmentNormal[i]],p1,p2,pn,&v1,&v2))
		{
			if(fix)
			{
				bool v3=dotProduct(vectDifference(p1,origpos),pn)>0;
				if(v3==v1)
				{
					int32 ln=dotProduct(pn,vectDifference(p1,s2));
					ln+=(ln>0)?(PORTALMARGIN):(-PORTALMARGIN);
					p->position=addVect(p->position,vectMult(pn,ln));
				}else{
					int32 ln=dotProduct(pn,vectDifference(p1,s1));
					ln+=(ln>0)?(PORTALMARGIN):(-PORTALMARGIN);
					p->position=addVect(p->position,vectMult(pn,ln));
				}
			//if put back, add updating of points
			// }else{
				// return true;
			}
			return true;
		}
	}
	/*
	 * No edge of the rectangle crosses the outline, so the remaining way it
	 * can block the portal is by sitting entirely inside it - a perpendicular
	 * surface poking through where the portal would go. That is the "case
	 * where both points in portal" of 2406a6e, and it has never actually run:
	 * the bounds below are missing their minus signs, so each condition reads
	 * "not exactly on the boundary" and returns false for every real input.
	 * The tail is therefore equivalent to an unconditional return false.
	 *
	 * Left as it is on purpose. Correcting it to
	 *
	 *     if(p1.x<-PORTALSIZEX||p1.x>PORTALSIZEX||...)
	 *
	 * enables a placement rule that has been dormant since the day it was
	 * written, and the one other time a dormant placement rule was switched on
	 * here it had to be switched off again - see portalToPortalIntersection
	 * below. Whether portals should refuse these spots is a level design
	 * question, and answering it needs the shipped chambers, not a unit test.
	 */
	p1=vectDifference(p1,p->position);
	p1=vect(dotProduct(p1,p->plane[0]),dotProduct(p1,p->plane[1]),0);
	if(p1.x<PORTALSIZEX||p1.x>PORTALSIZEX||p1.y<PORTALSIZEY||p1.y>PORTALSIZEY)return false;
	p2=vectDifference(p2,p->position);
	p2=vect(dotProduct(p2,p->plane[0]),dotProduct(p2,p->plane[1]),0);
	if(p2.x<PORTALSIZEX||p2.x>PORTALSIZEX||p2.y<PORTALSIZEY||p2.y>PORTALSIZEY)return false;

	return true;
}

/**
 * @brief Decides whether a candidate portal is clear of the other one.
 *
 * Named for the geometry it tests, but the sense is the caller's: true means
 * the placement is allowed. See the contract in portals.h.
 *
 * Both portals sit on axis aligned walls, so two of them can only overlap if
 * they lie in the same plane - parallel normals, and no separation along that
 * normal. Given that, it reduces to a rectangle overlap in the plane, which is
 * two separating axis checks against @p p's own tangents; @p p2's are the same
 * pair, possibly swapped, so its extents are projected onto them rather than
 * tested separately.
 *
 * @par Why this is not the check that used to be here
 * The previous version was switched off in e3fc39c ("provisional fix for level
 * 5 portals not appearing") and it deserved to be. It compared against 782*2-100
 * and 374*2 where the portal's actual half extents are @ref PORTALSIZEY (682)
 * and @ref PORTALSIZEX (341), so it rejected placements up to a tenth of a
 * portal too far apart; and its axis branches keyed off @c normal.x being equal,
 * which is also true of two portals that both face along z - and then compared
 * the wrong pair of axes for them. It handled no ceiling case at all.
 *
 * @par Conservative by a corner
 * The portals are drawn as ellipses and tested here as their bounding
 * rectangles, so two placed corner to corner are refused although the ellipses
 * would not quite have met. That is the safe direction to be wrong in for a
 * rule whose job is to stop them sharing a spot, and it is at most a corner's
 * worth.
 *
 * @param p  the portal being placed.
 * @param p2 the other portal.
 * @return true if @p p may be placed, false if it would overlap @p p2.
 */
bool portalToPortalIntersection(const portal_struct* p, const portal_struct* p2)
{
	if(!p || !p2)return true;

	//nothing to conflict with until the other portal has been shot
	if(!p2->used)return true;

	//different walls cannot overlap. Parallel normals give a zero cross
	//product, and anti-parallel ones do too, which is the point: two portals
	//facing opposite ways in one plane still share the spot.
	const vect3D c=vectProduct(p->normal,p2->normal);
	if(!equals(c.x,0) || !equals(c.y,0) || !equals(c.z,0))return true;

	const vect3D d=vectDifference(p2->position,p->position);

	//parallel but on different planes: one wall in front of another
	if(abs(dotProduct(d,p->normal))>PORTALPLANEEPSILON)return true;

	//p2's half extents measured along p's tangents. The dot products are
	//4096 or 0 for grid aligned portals, so this is a swap rather than a
	//rotation, but it costs nothing to write it generally.
	const int32 e0=abs(mulf32(PORTALSIZEX,dotProduct(p2->plane[0],p->plane[0])))
	              +abs(mulf32(PORTALSIZEY,dotProduct(p2->plane[1],p->plane[0])));
	const int32 e1=abs(mulf32(PORTALSIZEX,dotProduct(p2->plane[0],p->plane[1])))
	              +abs(mulf32(PORTALSIZEY,dotProduct(p2->plane[1],p->plane[1])));

	return abs(dotProduct(d,p->plane[0]))>=PORTALSIZEX+e0
	    || abs(dotProduct(d,p->plane[1]))>=PORTALSIZEY+e1;
}

bool isPortalOnWall(room_struct* r, portal_struct* p, bool fix)
{
	listCell_struct *lc=r->rectangles.first;
	vect3D origpos=p->position;
	while(lc)
	{
		if(portalRectangleIntersection(r,p,&lc->data,origpos,fix)&&!fix)return false;
		lc=lc->next;
	}
	return true;
}
