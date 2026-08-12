/**
 * @file particles.h
 * @brief World-space particle effects.
 *
 * The system was once cut for performance: with the room already being
 * rendered up to three times a frame for the portal views, there was no
 * polygon budget left for hundreds of sprites. It is now enabled again, but
 * idle - update and draw bail out immediately while no particle is alive, so
 * it costs nothing until an effect (emancipation flash, energy ball impact)
 * actually spawns some.
 *
 * Not to be confused with the gun's muzzle sparks in player.c, which are a
 * separate deliberately-tiny screen-space system.
 */

#ifndef __PARTICLES9__
#define __PARTICLES9__

#define NUMPARTICLES 256

typedef struct
{
	vect3D position, speed;
	u16 life, timer, color;
	u8 alpha;
	bool used;
}particle_struct;

void initParticles(void);
void drawParticles(void);
void updateParticles(void);
void particleExplosion(vect3D p, int number, u16 color);
void particleExplosionDir(vect3D p, vect3D dir, int number, u16 color);
void createParticles(vect3D position, vect3D speed, u16 life, u16 color);

#endif
