/**
 * @file AAR.c
 * @brief Static collision geometry: axis aligned rectangles and their broadphase grid.
 *
 * Implements @ref AAR.h. Three things live here:
 *
 *  - the rectangle pool and the accessors the FIFO commands drive
 *    (@ref createAAR, @ref updateAAR, @ref toggleAAR);
 *  - the broadphase, @ref generateGrid, which bins rectangles into XZ cells so
 *    a body only tests against nearby geometry, and shrinks its resolution
 *    until the node array fits in the ARM7's small heap;
 *  - contact generation, @ref AAROBBContacts and its helper OBBAARContacts,
 *    which clip a box against a rectangle and emit @ref AARCOLLISION contacts.
 *
 * Contacts that fall inside an open portal's outline are suppressed
 * (@c pointInPortal), which is what lets a cube fly through a wall, and the
 * funnel of guide rectangles built by @ref generateGuidAAR stops it catching
 * on the rim on the way.
 *
 * @see OBB.c for the solver that consumes these contacts.
 */

#include "stdafx.h"
#define ALLOCATORSIZE (8*1024) /**< Heap budget for the broadphase node array. */ //may cause problems !

//static u8 allocatorPool[ALLOCATORSIZE];
static u16 allocatorCounter=0;

static AAR_struct aaRectangles[NUMAARS];
static grid_struct AARgrid;

static const u8 AARSegments[NUMAARSEGMENTS][2]={{0,1},{1,2},{3,2},{0,3}};
static const u8 AARSegmentsPD[NUMAARSEGMENTS][2]={{0,0},{1,1},{3,0},{0,1}};

//extern portal_struct portal[2];

u32 nodeSize=ORIGNODESIZE;

void freeGrid(grid_struct* g); // defined below, used by initAARs

/*void initAllocator(void)
{
    allocatorCounter=0;
}*/

void* allocateData(u16 size)
{
    /*
    if(allocatorCounter+size>ALLOCATORSIZE){return NULL;}
    allocatorCounter+=size;
    // fifoSendValue32(FIFO_USER_08,allocatorCounter);
    return &allocatorPool[allocatorCounter-size];*/
    void * p=malloc(size);
    if (!p)
        return p;

    allocatorCounter+=size;
    return p;
}

void initAARs(void)
{
    int i;
    for(i=0;i<NUMAARS;i++)
    {
        aaRectangles[i].used=false;
    }
    // Must release the grid, not just forget it: this runs on every level
    // load, so dropping the pointer leaked the whole broadphase each time.
    freeGrid(&AARgrid);
}

void freeGrid(grid_struct* g)
{
/*
    if(!g || !g->nodes)return;
    g->nodes=NULL;


*/
    if (!g)
        return;
    // Each non-empty cell owns the index array allocateData handed it, so they
    // have to go before the cell array itself. Freeing only g->nodes leaked one
    // allocation per occupied cell every time the grid was rebuilt.
    if(g->nodes)
    {
        const int count=(int)g->width*g->height;
        for(int i=0;i<count;i++)
            free(g->nodes[i].data);
    }
    free(g->nodes);
    g->nodes=NULL;
    g->width=g->height=0;
    //initAllocator();
    allocatorCounter=0;
}

void generateGrid(grid_struct* g)
{
    if(!g)g=&AARgrid;

    freeGrid(g);


    bool b=false;
    vect3D m, M;
    m=vect((1<<29),(1<<29),(1<<29));
    M=vect(-(1<<29),-(1<<29),-(1<<29));
    for(int i=0;i<NUMAARS;i++)
    {
        if(aaRectangles[i].used)
        {
            if(aaRectangles[i].position.x<m.x)m.x=aaRectangles[i].position.x;
            if(aaRectangles[i].position.z<m.z)m.z=aaRectangles[i].position.z;

            if(aaRectangles[i].position.x+aaRectangles[i].size.x>M.x)M.x=aaRectangles[i].position.x+aaRectangles[i].size.x;
            if(aaRectangles[i].position.z+aaRectangles[i].size.z>M.z)M.z=aaRectangles[i].position.z+aaRectangles[i].size.z;
            b=true;
        }
    }
    if(!b)return;

    nodeSize=ORIGNODESIZE;

    g->width=(M.x-m.x)/NODESIZE+1;
    g->height=(M.z-m.z)/NODESIZE+1;

    while((sizeof(node_struct)*g->width*g->height)>(ALLOCATORSIZE/3))
    {
        nodeSize*=2;
        g->width=(M.x-m.x)/NODESIZE+1;
        g->height=(M.z-m.z)/NODESIZE+1;
    }
    g->nodes=allocateData(sizeof(node_struct)*g->width*g->height);

    g->m=m;
    g->M=M;

    fifoSendValue32(FIFO_USER_08,allocatorCounter);

    static u16 temp[NUMAARS];

    for(int i=0;i<g->width;i++)
    {
        for(int j=0;j<g->height;j++)
        {
            node_struct* n=&g->nodes[i+j*g->width];
            n->length=0;
            for(int k=0;k<NUMAARS;k++)
            {
                if(aaRectangles[k].used)
                {
                    const int32 mX=g->m.x+i*NODESIZE, MX=g->m.x+(i+1)*NODESIZE;
                    const int32 mZ=g->m.z+j*NODESIZE, MZ=g->m.z+(j+1)*NODESIZE;
                    if(!((aaRectangles[k].position.x<mX && aaRectangles[k].position.x+aaRectangles[k].size.x<mX)
                    ||   (aaRectangles[k].position.x>MX && aaRectangles[k].position.x+aaRectangles[k].size.x>MX)
                    ||   (aaRectangles[k].position.z<mZ && aaRectangles[k].position.z+aaRectangles[k].size.z<mZ)
                    ||   (aaRectangles[k].position.z>MZ && aaRectangles[k].position.z+aaRectangles[k].size.z>MZ)))
                    {
                        temp[n->length]=k;
                        n->length++;
                    }
                }
            }
            if(n->length)
            {
                n->data=allocateData(sizeof(u16)*n->length);
                memcpy(n->data,temp,n->length*sizeof(u16));
            }
            else n->data=NULL;
        }
    }
    fifoSendValue32(FIFO_USER_08,allocatorCounter);
}

// Converts an offset from the grid origin into a cell index, clamped to the
// grid. The clamp is load bearing: a body can end up outside the room - shoved
// through a wall, or falling out of the world - and then this offset is
// negative or past the far edge. Stored straight into a u16 a negative index
// wraps to ~65535, and the caller walks off the end of the node array.
static u16 clampNodeIndex(int32 offset, u16 count)
{
    if(!count)
        return 0;
    if(offset<0)
        return 0;
    const int32 n=offset/NODESIZE;
    return (n>=count)?(count-1):n;
}

void getOBBNodes(grid_struct* g, OBB_struct* o, u16* x, u16* X, u16* z, u16* Z)
{
    if(!o)return;
    if(!g)g=&AARgrid;

    const vect3D m=vectDifference(o->AABBo,g->m);
    const vect3D M=addVect(m,o->AABBs);

    *x=clampNodeIndex(m.x,g->width);*z=clampNodeIndex(m.z,g->height);
    *X=clampNodeIndex(M.x,g->width);*Z=clampNodeIndex(M.z,g->height);
}

AAR_struct* createAAR(u16 id, vect3D position, vect3D size, vect3D normal)
{
    int i=id;
    // for(i=0;i<NUMAARS;i++)
    // {
        if(!aaRectangles[i].used)
        {
            aaRectangles[i].used=true;
            aaRectangles[i].position=position;
            aaRectangles[i].size=size;
            aaRectangles[i].normal=normal;
            // fifoSendValue32(FIFO_USER_08,i);
            return &aaRectangles[i];
        }
    // }
    return NULL;
}

void updateAAR(u16 id, vect3D position)
{
    int i=id;
    // for(i=0;i<NUMAARS;i++)
    // {
        if(aaRectangles[i].used)
        {
            aaRectangles[i].position=position;
        }
    // }
}

void toggleAAR(u16 id)
{
    if(id>=NUMAARS)
        return;
    aaRectangles[id].used^=1;
}

ARM_CODE bool pointInPortal(portal_struct* p, vect3D pos) //assuming correct normal
{
    if(!p)
        return false;
    const vect3D v2=vectDifference(pos, p->position); //then, project onto portal base
    vect3D v=vect(dotProduct(p->plane[0],v2),dotProduct(p->plane[1],v2),dotProduct(p->normal,v2));
    return (abs(v.z)<16 && v.y>-PORTALSIZEY*4 && v.y<PORTALSIZEY*4 && v.x>-PORTALSIZEX*4 && v.x<PORTALSIZEX*4);
}

ARM_CODE void OBBAARContacts(AAR_struct* a, OBB_struct* o, bool port)
{
    if(!a || !o || !o->used || !a->used)
        return;

    vect3D u[3];
        u[0]=vect(o->transformationMatrix[0],o->transformationMatrix[3],o->transformationMatrix[6]);
        u[1]=vect(o->transformationMatrix[1],o->transformationMatrix[4],o->transformationMatrix[7]);
        u[2]=vect(o->transformationMatrix[2],o->transformationMatrix[5],o->transformationMatrix[8]);

    vect3D v[4], vv[4];
    vect3D uu[2], uuu[2];
        v[0]=vectDifference(a->position,o->position);
        v[2]=addVect(v[0],a->size);
        if(a->normal.x)
        {
            uu[0]=vect(0,inttof32(1),0);
            uu[1]=vect(0,0,inttof32(1));

            v[1]=addVect(v[0],vect(0,a->size.y,0));
            v[3]=addVect(v[0],vect(0,0,a->size.z));
        }else if(a->normal.y)
        {
            uu[0]=vect(inttof32(1),0,0);
            uu[1]=vect(0,0,inttof32(1));

            v[1]=addVect(v[0],vect(a->size.x,0,0));
            v[3]=addVect(v[0],vect(0,0,a->size.z));
        }else{
            uu[0]=vect(inttof32(1),0,0);
            uu[1]=vect(0,inttof32(1),0);

            v[1]=addVect(v[0],vect(a->size.x,0,0));
            v[3]=addVect(v[0],vect(0,a->size.y,0));
        }
    int i;
    for(i=0;i<4;i++) //optimizeable
    {
        vv[i]=vect(dotProduct(v[i],u[0]),dotProduct(v[i],u[1]),dotProduct(v[i],u[2]));
        v[i]=addVect(v[i],o->position);
    }
    uuu[0]=vect(dotProduct(uu[0],u[0]),dotProduct(uu[0],u[1]),dotProduct(uu[0],u[2]));
    uuu[1]=vect(dotProduct(uu[1],u[0]),dotProduct(uu[1],u[1]),dotProduct(uu[1],u[2]));

    int32 ss[3];
        ss[0]=o->size.x;//+MAXPENETRATIONBOX;
        ss[1]=o->size.y;//+MAXPENETRATIONBOX;
        ss[2]=o->size.z;//+MAXPENETRATIONBOX;

    for(i=0;i<NUMAARSEGMENTS;i++)
    {
        vect3D p1=v[AARSegments[i][0]];
        vect3D p2=v[AARSegments[i][1]];
        vect3D uu1=vv[AARSegments[i][0]];
        vect3D uu2=vv[AARSegments[i][1]];
        /* All six of these used to be uninitialised. b1 and b2 are what
         * decide whether a contact is emitted at all, and clipSegmentOBB
         * assigns them only on some of its paths - so which contacts a
         * body got against the static world depended on whatever was left
         * on the stack. collideOBBs sets its copies up correctly; this,
         * the path every body takes against every wall and floor, did not.
         *
         * k1 and k2 are read by clipSegmentOBB (it maxes and mins against
         * them) and are otherwise dead - the lines that consumed them are
         * commented out in both callers. They are initialised here so the
         * reads are defined; anyone reviving them needs to work out the
         * right starting values rather than trust these. */
        vect3D n1=vect(0,0,0), n2=vect(0,0,0);
        int32 k1=0, k2=0;
        bool b1=false, b2=false;
        if(clipSegmentOBB(ss, u, &p1, &p2, uu[AARSegmentsPD[i][1]], &uu1, &uu2, uuu[AARSegmentsPD[i][1]], &n1, &n2, &b1, &b2, &k1, &k2))
        {
            if(b1)
            {
                bool b=false;
                if(port)
                {
                    if((portal[0].normal.x&&a->normal.x)||(portal[0].normal.y&&a->normal.y)||(portal[0].normal.z&&a->normal.z))b=pointInPortal(&portal[0],p1);
                    if(!b&&((portal[1].normal.x&&a->normal.x)||(portal[1].normal.y&&a->normal.y)||(portal[1].normal.z&&a->normal.z)))b=pointInPortal(&portal[1],p1);
                }
                if(!b)
                {
                    //p1=addVect(p1,vectMult(vv,k1));
                    contactPoint_struct* cp=nextContactPoint(o);
                    if(cp)
                    {
                        cp->point=p1;
                        cp->type=PLANECOLLISION;
                        cp->normal=a->normal;
                        cp->penetration=0;
                        cp->target=NULL;
                    }
                }
            }
            if(b2)
            {
                bool b=false;
                if(port)
                {
                    if((portal[0].normal.x&&a->normal.x)||(portal[0].normal.y&&a->normal.y)||(portal[0].normal.z&&a->normal.z))b=pointInPortal(&portal[0],p2);
                    if(!b&&((portal[1].normal.x&&a->normal.x)||(portal[1].normal.y&&a->normal.y)||(portal[1].normal.z&&a->normal.z)))b=pointInPortal(&portal[1],p2);
                }
                if(!b)
                {
                    //p2=addVect(p2,vectMult(vv,k2));
                    contactPoint_struct* cp=nextContactPoint(o);
                    if(cp)
                    {
                        cp->point=p2;
                        cp->type=PLANECOLLISION;
                        cp->normal=a->normal;
                        cp->penetration=0;
                        cp->target=NULL;
                    }
                }
            }
        }
    }
}

ARM_CODE bool __attribute__((noinline)) AAROBBContacts(AAR_struct* a, OBB_struct* o, vect3D* v, bool port)
{
    if(!a || !o )return false;

    if(!o->used || !a->used)return false;



    u16 oldnum=o->numContactPoints;

    vect3D u[3];
        u[0]=vect(o->transformationMatrix[0],o->transformationMatrix[3],o->transformationMatrix[6]);
        u[1]=vect(o->transformationMatrix[1],o->transformationMatrix[4],o->transformationMatrix[7]);
        u[2]=vect(o->transformationMatrix[2],o->transformationMatrix[5],o->transformationMatrix[8]);

    if(a->normal.x)
    {
        if(a->position.x>o->AABBo.x+o->AABBs.x || a->position.x<o->AABBo.x)return false;
        int i;
        bool vb[8];
        for(i=0;i<8;i++)vb[i]=v[i].x>a->position.x;
        for(i=0;i<NUMOBBSEGMENTS;i++) //possible to only check half !
        {
            if(vb[OBBSegments[i][0]]!=vb[OBBSegments[i][1]])
            {
                const vect3D uu=v[OBBSegmentsPD[i][0]];
                const vect3D vv=u[OBBSegmentsPD[i][1]];
                const int32 k=divv16(abs(uu.x-a->position.x),abs(vv.x));
                const vect3D p=addVect(uu,vectMult(vv,k));
                if(p.y > a->position.y 
                    && p.y < a->position.y + a->size.y 
                    && p.z > a->position.z 
                    && p.z<a->position.z+a->size.z)
                {
                    bool b=false;
                    if(port)
                    {
                        if(portal[0].normal.x)b=pointInPortal(&portal[0],p);
                        if(!b&&portal[1].normal.x)b=pointInPortal(&portal[1],p);
                    }
                    if(!b)
                    {
                        contactPoint_struct* cp=nextContactPoint(o);
                        if(cp)
                        {
                            cp->point=p;
                            cp->type=AARCOLLISION;
                            cp->normal=a->normal;
                            cp->penetration=0;
                            cp->target=NULL;
                        }
                    }
                }
            }
        }
    }else if(a->normal.y)
    {
        if(a->position.y>o->AABBo.y+o->AABBs.y || a->position.y<o->AABBo.y)return false;
        int i;
        bool vb[8];
        for(i=0;i<8;i++)vb[i]=v[i].y>a->position.y;
        for(i=0;i<NUMOBBSEGMENTS;i++)
        {
            if(vb[OBBSegments[i][0]]!=vb[OBBSegments[i][1]])
            {
                const vect3D uu=v[OBBSegmentsPD[i][0]];
                const vect3D vv=u[OBBSegmentsPD[i][1]];
                const int32 k=divv16(abs(uu.y-a->position.y),abs(vv.y));
                const vect3D p=addVect(uu,vectMult(vv,k));
                if(p.x>a->position.x && p.x<a->position.x+a->size.x && p.z>a->position.z && p.z<a->position.z+a->size.z)
                {
                    bool b=false;
                    if(port)
                    {
                        if(portal[0].normal.y)b=pointInPortal(&portal[0],p);
                        if(!b&&portal[1].normal.y)b=pointInPortal(&portal[1],p);
                    }
                    if(!b)
                    {
                        contactPoint_struct* cp=nextContactPoint(o);
                        if(cp)
                        {
                            cp->point=p;
                            cp->type=AARCOLLISION;
                            cp->normal=a->normal;
                            cp->penetration=0;
                            cp->target=NULL;
                        }
                    }
                }
            }
        }
    }else{
        if(a->position.z>o->AABBo.z+o->AABBs.z || a->position.z<o->AABBo.z)return false;
        int i;
        bool vb[8];
        for(i=0;i<8;i++)vb[i]=v[i].z>a->position.z;
        for(i=0;i<NUMOBBSEGMENTS;i++)
        {
            if(vb[OBBSegments[i][0]]!=vb[OBBSegments[i][1]])
            {
                const vect3D uu=v[OBBSegmentsPD[i][0]];
                const vect3D vv=u[OBBSegmentsPD[i][1]];
                const int32 k=divv16(abs(uu.z-a->position.z),abs(vv.z));
                const vect3D p=addVect(uu,vectMult(vv,k));
                if(p.x>a->position.x && p.x<a->position.x+a->size.x && p.y>a->position.y && p.y<a->position.y+a->size.y)
                {
                    bool b=false;
                    if(port)
                    {
                        if(portal[0].normal.z)b=pointInPortal(&portal[0],p);
                        if(!b&&portal[1].normal.z)b=pointInPortal(&portal[1],p);
                    }
                    if(!b)
                    {
                        contactPoint_struct* cp=nextContactPoint(o);
                        if(cp)
                        {
                            cp->point=p;
                            cp->type=AARCOLLISION;
                            cp->normal=a->normal;
                            cp->penetration=0;
                            cp->target=NULL;
                        }
                    }
                }
            }
        }
    }

    OBBAARContacts(a, o, port);

    return o->numContactPoints>oldnum;
}

void AARsOBBContacts(OBB_struct* o, bool sleep)
{
    if (!o)
        return;
    bool port=portal[0].used&&portal[1].used;
    vect3D v[8];
    getOBBVertices(o,v);
    if(!sleep)
    {
        u16 x=0;u16 X=0;u16 z=0;u16 Z=0;
        getOBBNodes(NULL, o, &x, &X, &z, &Z);
        int8_t lalala[NUMAARS];
        for(int i=0;i<NUMAARS;i++)lalala[i]=0;
        o->groundID=-1;
        for(int i=x;i<=X;i++)
        {
            for(int j=z;j<=Z;j++)
            {
                if (!AARgrid.nodes)continue;
                int idx=i+j*AARgrid.width;
                //if (idx>= )continue;
                node_struct* n=&AARgrid.nodes[idx];
                if (!n) continue;
                // A cell can report a length with no array behind it, because
                // allocateData returns NULL when the broadphase arena is full.
                // The guard used to cover only the contact call, and the ground
                // check and the visited mark below it dereferenced n->data
                // anyway.
                if (!n->data)
                    continue;

                for(int k=0;k<n->length;k++)
                {
                    const u16 old=o->numContactPoints;
                    const u16 rect=n->data[k];

                    if(!lalala[rect])
                    {
                        AAROBBContacts(&aaRectangles[rect], o, v, port);
                    }
                    if(o->groundID<0 
                    && o->numContactPoints>old 
                    && aaRectangles[rect].normal.y>0)
                    {
                        o->groundID=rect;
                    }
                    lalala[rect]=1;
                }
            }
        }
        if(port)
        {
            AAROBBContacts(&portal[0].guideAAR[0], o, v, false);
            AAROBBContacts(&portal[0].guideAAR[1], o, v, false);
            AAROBBContacts(&portal[0].guideAAR[2], o, v, false);
            AAROBBContacts(&portal[0].guideAAR[3], o, v, false);

            AAROBBContacts(&portal[1].guideAAR[0], o, v, false);
            AAROBBContacts(&portal[1].guideAAR[1], o, v, false);
            AAROBBContacts(&portal[1].guideAAR[2], o, v, false);
            AAROBBContacts(&portal[1].guideAAR[3], o, v, false);
        }
    }
    collideOBBPlatforms(o, v);
}

void fixAAR(AAR_struct* a)
{
    if(!a)return;

    if(a->size.x<0){a->position.x+=a->size.x;a->size.x=-a->size.x;}
    if(a->size.y<0){a->position.y+=a->size.y;a->size.y=-a->size.y;}
    if(a->size.z<0){a->position.z+=a->size.z;a->size.z=-a->size.z;}
}

ARM_CODE void generateGuidAAR(portal_struct* p)
{
    if(!p)return;

    p->guideAAR[0].used=true;
    p->guideAAR[0].position=vectDifference(p->position,vectMultInt(addVect(vectDivInt(p->plane[0],PORTALFRACTIONX),vectDivInt(p->plane[1],PORTALFRACTIONY)),4));
    p->guideAAR[0].size=vectMultInt(addVect(vectDivInt(vectMultInt(p->plane[0],2),PORTALFRACTIONX),vectDivInt(p->normal,-8)),4);
    p->guideAAR[0].normal=p->plane[1];
    fixAAR(&p->guideAAR[0]);

    p->guideAAR[1].used=true;
    p->guideAAR[1].position=vectDifference(p->position,vectMultInt(addVect(vectDivInt(p->plane[0],PORTALFRACTIONX),vectDivInt(p->plane[1],PORTALFRACTIONY)),4));
    p->guideAAR[1].size=vectMultInt(addVect(vectDivInt(vectMultInt(p->plane[1],2),PORTALFRACTIONX),vectDivInt(p->normal,-8)),4);
    p->guideAAR[1].normal=p->plane[0];
    fixAAR(&p->guideAAR[1]);

    p->guideAAR[2].used=true;
    p->guideAAR[2].position=addVect(p->position,vectMultInt(addVect(vectDivInt(p->plane[0],PORTALFRACTIONX),vectDivInt(p->plane[1],PORTALFRACTIONY)),4));
    p->guideAAR[2].size=vectMultInt(addVect(vectDivInt(vectMultInt(p->plane[0],2),PORTALFRACTIONX),vectDivInt(p->normal,-8)),4);
    p->guideAAR[2].normal=vectMultInt(p->plane[1],-1);
    fixAAR(&p->guideAAR[2]);

    p->guideAAR[3].used=true;
    p->guideAAR[3].position=vectDifference(p->position,vectMultInt(addVect(vectDivInt(p->plane[0],PORTALFRACTIONX),vectDivInt(p->plane[1],PORTALFRACTIONY)),4));
    p->guideAAR[3].size=vectMultInt(addVect(vectDivInt(vectMultInt(p->plane[1],2),PORTALFRACTIONX),vectDivInt(p->normal,-8)),4);
    p->guideAAR[3].normal=vectMultInt(p->plane[0],-1);
    fixAAR(&p->guideAAR[3]);
}
