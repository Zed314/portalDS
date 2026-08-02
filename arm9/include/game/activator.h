/**
 * @file activator.h
 * @brief The wiring between things that trigger and things that get triggered.
 *
 * Test chamber logic is entirely "this button opens that door". Rather than
 * every trigger knowing about every kind of target, a trigger embeds an
 * @ref activator_struct - a short list of targets - and simply calls
 * @ref useActivator when it fires and @ref unuseActivator when it stops.
 *
 * Targets are stored as a @c void* plus an @ref activatorTarget_type saying
 * what to cast it back to. That is why adding a new triggerable entity means
 * adding a case to the switch in activator.c: the type tag is the whole of the
 * dispatch mechanism.
 *
 * Triggers are @ref bigbutton.h, @ref timedbutton.h and the switches the gun
 * can shoot; targets are cube dispensers, platforms, doors and wall doors.
 */

#ifndef ACTIVATOR_H
#define ACTIVATOR_H

#define NUMACTIVATORSLOTS (4) /**< Maximum number of things one trigger can drive. */

/** @brief What kind of entity an activator slot points at. */
typedef enum
{
	DISPENSER_TARGET, /**< A cubeDispenser_struct. */
	PLATFORM_TARGET,  /**< A platform_struct. */
	DOOR_TARGET,      /**< A door_struct. */
	WALLDOOR_TARGET,  /**< A wallDoor_struct. */
	NOT_TARGET        /**< Empty slot. */
}activatorTarget_type;

/** @brief One target: an untyped pointer plus the tag saying how to read it. */
typedef struct
{
	void* target;              /**< The target entity. */
	activatorTarget_type type; /**< What @c target actually points at. */
}activatorSlot_struct;

/**
 * @brief A list of things to trigger.
 *
 * Embedded by value in every entity that can trigger something.
 */
typedef struct
{
	activatorSlot_struct slot[NUMACTIVATORSLOTS]; /**< The targets. */
	u8 numSlots;                                  /**< How many slots are in use. */
}activator_struct;

/** @brief Clears an activator's target list. */
void initActivator(activator_struct* a);

/** @brief Fires every target: opens doors, starts platforms, dispenses cubes. */
void useActivator(activator_struct* a);

/** @brief Releases every target: closes doors, stops platforms. */
void unuseActivator(activator_struct* a);

/**
 * @brief Adds a target to an activator.
 * @param a      activator to extend.
 * @param target entity to drive.
 * @param type   what @p target is, so it can be cast back correctly.
 */
void addActivatorTarget(activator_struct* a, void* target, activatorTarget_type type);

#endif
