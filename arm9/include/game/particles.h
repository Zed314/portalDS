/**
 * @file particles.h
 * @brief Particle effects. Entirely disabled.
 *
 * The whole system is commented out. It was cut for performance: with the room
 * already being rendered up to three times a frame for the portal views, there
 * was no polygon budget left for hundreds of sprites. The calls to
 * initParticles(), updateParticles() and drawParticles() are still present in
 * game.c, commented out alongside their declarations here.
 *
 * Kept because the effects it was written for - the emancipation flash, energy
 * ball impacts - are still described in the code that would have called it.
 */

#ifndef __PARTICLES9__
#define __PARTICLES9__

/* Not used currently */
/*
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
*/
#endif
