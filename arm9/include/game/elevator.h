/**
 * @file elevator.h
 * @brief The glass lifts that carry the player between chambers.
 *
 * An elevator is the transition between test chambers - it arrives, opens, lets
 * the player in, closes and leaves, at which point the next level loads. That
 * sequence is elevatorState_type, and it runs strictly in order.
 *
 * Elevators are not a pool. There are exactly two per chamber, one at each end,
 * and each is embedded by value in the wall door it belongs to (see
 * @ref walldoor.h) rather than allocated separately.
 *
 * The interior is handled specially by the player collision code in physics.c -
 * search for the @c ELEVATOR_ constants there - because a cylinder that the
 * player must be kept inside rather than outside is the opposite of every other
 * collision case in the game.
 */

#ifndef ELEVATOR_H
#define ELEVATOR_H

#define ELEVATOR_UPDOWNBIT (4)          /**< Bit of the direction field selecting up versus down travel. */
#define ELEVATOR_SIZE (TILESIZE*3)      /**< Radius of the elevator shaft, in f32. */

/** @brief The stages of an elevator's sequence, which run strictly in order. */
typedef enum
{
	ELEVATOR_ARRIVING, /**< Travelling into position. */
	ELEVATOR_OPENING,  /**< Doors opening. */
	ELEVATOR_OPEN,     /**< Waiting for the player. */
	ELEVATOR_CLOSING,  /**< Doors closing behind them. */
	ELEVATOR_LEAVING   /**< Travelling away; the level ends when this finishes. */
}elevatorState_type;

/** @brief An elevator. Embedded by value in a @ref wallDoor_struct. */
typedef struct
{
	vect3D position;     /**< Nominal position in tile coordinates. */
	vect3D realPosition; /**< Actual world-space position, which moves during arrival and departure. */
	rectangle_struct* doorSurface; /**< The door face, toggled as the doors open and close. */
	int32 progress;      /**< Progress through the current stage. */
	u8 direction;        /**< Facing, plus @ref ELEVATOR_UPDOWNBIT for travel direction. */
	modelInstance_struct modelInstance; /**< Model and its door animation. */
	elevatorState_type state; /**< Current stage of the sequence. */
	rectangle_struct* floor;     /**< The elevator's own floor face, which travels with it. */
}elevator_struct;

/** @brief Loads the elevator model. */
void initElevators(void);

/** @brief Releases the elevator model. */
void freeElevators(void);

/**
 * @brief Sets up an elevator.
 * @param ev       elevator to initialise.
 * @param r        room it belongs to.
 * @param position position in tile coordinates.
 * @param direction facing.
 * @param up       true if it arrives from below.
 */
void initElevator(elevator_struct* ev, room_struct* r, vect3D position, u8 direction, bool up);

/**
 * @brief Starts an elevator's arrival from a distance away.
 * @param ev       elevator to start.
 * @param distance how far out to start it, which sets how long the arrival takes.
 */
void setElevatorArriving(elevator_struct* ev, int32 distance);

/** @brief Closes the doors and begins the departure - this is what ends a level. */
void closeElevator(elevator_struct* ev);

/** @brief Advances an elevator through its sequence by one frame. */
void updateElevator(elevator_struct* ev);

/** @brief Draws an elevator. */
void drawElevator(elevator_struct* ev);

#endif
