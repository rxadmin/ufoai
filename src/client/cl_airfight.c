/**
 * @file cl_airfight.c
 * @brief Airfight related stuff.
 * @todo Somehow i need to know which alien race was in the ufo we shoot down
 * I need this info for spawning the crash site (maybe even how many aliens)
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "cl_global.h"

/**
 * @brief Run bullets on geoscape.
 * @param[in] projectile Pointer to the projectile corresponding to this bullet.
 * @param[in] ortogonalVector vector perpendicular to the movement of the projectile.
 * @param[in] movement how much each bullet should move toward its target.
 */
static void AIRFIGHT_RunBullets (aircraftProjectile_t *projectile, vec3_t ortogonalVector, float movement)
{
	int i;
	vec3_t startPoint, finalPoint;

	assert(projectile->bullets);

	for (i = 0; i < BULLETS_PER_SHOT; i++) {
		PolarToVec(projectile->bulletPos[i], startPoint);
		RotatePointAroundVector(finalPoint, ortogonalVector, startPoint, movement);
		VecToPolar(finalPoint, projectile->bulletPos[i]);
	}
}

/**
 * @brief Remove a projectile from gd.projectiles
 * @param[in] idx of the projectile to remove in array gd.projectiles[]
 * @sa AIRFIGHT_AddProjectile
 */
static qboolean AIRFIGHT_RemoveProjectile (aircraftProjectile_t *projectile)
{
	int i;
	int idx = projectile->idx;

	gd.numProjectiles--;
	assert(gd.numProjectiles >= idx);
	memmove(projectile, projectile + 1, (gd.numProjectiles - idx) * sizeof(*projectile));
	/* wipe the now vacant last slot */
	memset(&gd.projectiles[gd.numProjectiles], 0, sizeof(gd.projectiles[gd.numProjectiles]));

	for (i = idx; i < gd.numProjectiles; i++)
		gd.projectiles[i].idx--;

	return qtrue;
}

/**
 * @brief Add a projectile in gd.projectiles
 * @param[in] attackingBase the attacking base in gd.bases[]. NULL is the attacker is an aircraft.
 * @param[in] attacker Pointer to the attacking aircraft
 * @param[in] aimedBase the aimed base (NULL if the target is an aircraft)
 * @param[in] target Pointer to the target aircraft
 * @param[in] weaponSlot Pointer to the weapon slot that fires the projectile.
 * @note we already checked in AIRFIGHT_ChooseWeapon that the weapon has still ammo
 * @sa AIRFIGHT_RemoveProjectile
 * @sa AII_ReloadWeapon for the aircraft item reload code
 */
static qboolean AIRFIGHT_AddProjectile (const base_t* attackingBase, aircraft_t *attacker, base_t* aimedBase, aircraft_t *target, aircraftSlot_t *weaponSlot)
{
	aircraftProjectile_t *projectile;
	int i;

	if (gd.numProjectiles >= MAX_PROJECTILESONGEOSCAPE) {
		Com_DPrintf(DEBUG_CLIENT, "Too many projectiles on map\n");
		return qfalse;
	}

	projectile = &gd.projectiles[gd.numProjectiles];

	if (weaponSlot->ammoIdx == NONE) {
		Com_Printf("AIRFIGHT_AddProjectile: Error - no ammo assigned\n");
		return qfalse;
	}

	assert(weaponSlot->itemIdx != NONE);

	projectile->idx = gd.numProjectiles;
	projectile->aircraftItemsIdx = weaponSlot->ammoIdx;
	if (!attackingBase) {
		assert(attacker);
		projectile->attackingAircraft = attacker;
		VectorSet(projectile->pos, attacker->pos[0], attacker->pos[1], 0);
	} else {
		projectile->attackingAircraft = NULL;
		VectorSet(projectile->pos, attackingBase->pos[0], attackingBase->pos[1], 0);
	}
	/* if we are not aiming to a base - we are aiming towards an aircraft */
	if (!aimedBase) {
		assert(target);
		projectile->aimedAircraft = target;
		projectile->aimedBase = NULL;
		VectorSet(projectile->idleTarget, 0, 0, 0);
	} else {
		projectile->aimedAircraft = NULL;
		projectile->aimedBase = aimedBase;
		VectorSet(projectile->idleTarget, aimedBase->pos[0], aimedBase->pos[1], 0);
	}
	projectile->time = 0;
	projectile->angle = 0.0f;
	projectile->bullets = qfalse;

	if (csi.ods[weaponSlot->itemIdx].craftitem.bullets) {
		projectile->bullets = qtrue;

		for (i = 0; i < BULLETS_PER_SHOT; i++) {
			projectile->bulletPos[i][0] = projectile->pos[0] + (frand() - 0.5f) * 2;
			projectile->bulletPos[i][1] = projectile->pos[1] + (frand() - 0.5f) * 2;
		}
	}

	weaponSlot->ammoLeft -= 1;

	gd.numProjectiles++;

	return qtrue;
}

/**
 * @brief Change destination of projectile to an idle point of the map, close to its former target.
 * @param[in] idx idx of the projectile to update in gd.projectiles[].
 */
static void AIRFIGHT_MissTarget (aircraftProjectile_t *projectile, qboolean returnToBase)
{
	aircraft_t *oldTarget;
	assert(projectile);

	oldTarget = projectile->aimedAircraft;
	if (oldTarget) {
		VectorSet(projectile->idleTarget, oldTarget->pos[0] + 10 * frand() - 5, oldTarget->pos[1] + 10 * frand() - 5, 0);
		projectile->aimedAircraft = NULL;
	} else {
		VectorSet(projectile->idleTarget, projectile->idleTarget[0] + 10 * frand() - 5, projectile->idleTarget[1] + 10 * frand() - 5, 0);
		projectile->aimedBase = NULL;
	}
	if (returnToBase && projectile->attackingAircraft) {
		if (projectile->attackingAircraft->homebase) {
			assert(projectile->attackingAircraft->ufotype == UFO_MAX);
			AIR_AircraftReturnToBase(projectile->attackingAircraft);
		}
	}
}

/**
 * @brief Check if the selected weapon can shoot.
 * @param[in] slot Pointer to the weapon slot to shoot with.
 * @param[in] distance distance between the weapon and the target.
 * @return 0 AIRFIGHT_WEAPON_CAN_SHOOT if the weapon can shoot,
 * -1 AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT if it can't shoot atm,
 * -2 AIRFIGHT_WEAPON_CAN_NEVER_SHOOT if it will never be able to shoot.
 */
static int AIRFIGHT_CheckWeapon (const aircraftSlot_t *slot, float distance)
{
	int ammoIdx;

	assert(slot);

	/* check if there is a functional weapon in this slot */
	if (slot->itemIdx == NONE || slot->installationTime != 0)
		return AIRFIGHT_WEAPON_CAN_NEVER_SHOOT;

		/* check if there is still ammo in this weapon */
	if (slot->ammoIdx == NONE || slot->ammoLeft <= 0)
		return AIRFIGHT_WEAPON_CAN_NEVER_SHOOT;

	ammoIdx = slot->ammoIdx;
	/* check if the target is within range of this weapon */
	if (distance > csi.ods[ammoIdx].craftitem.stats[AIR_STATS_WRANGE])
		return AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT;

	/* check if weapon is reloaded */
	if (slot->delayNextShot > 0)
		return AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT;

	return AIRFIGHT_WEAPON_CAN_SHOOT;
}

/**
 * @brief Choose the weapon an attacking aircraft will use to fire on a target.
 * @param[in] slot Pointer to the first weapon slot of attacking base or aircraft.
 * @param[in] maxSlot maximum number of weapon slots in attacking base or aircraft.
 * @param[in] pos position of attacking base or aircraft.
 * @param[in] targetpos Pointer to the aimed aircraft.
 * @return indice of the slot to use (in array weapons[]),
 * -1 AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT no weapon to use atm,
 * -2 AIRFIGHT_WEAPON_CAN_NEVER_SHOOT if no weapon to use at all.
 * @sa AIRFIGHT_CheckWeapon
 */
int AIRFIGHT_ChooseWeapon (aircraftSlot_t *slot, int maxSlot, vec3_t pos, vec3_t targetPos)
{
	int slotIdx = AIRFIGHT_WEAPON_CAN_NEVER_SHOOT;
	int i, weapon_status;
	float distance0 = 99999.9f;
	float distance = MAP_GetDistance(pos, targetPos);

	/* We choose the usable weapon with the smallest range */
	for (i = 0; i < maxSlot; i++) {
		assert(slot);
		weapon_status = AIRFIGHT_CheckWeapon(slot + i, distance);

		/* set slotIdx to AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT if needed */
		/* this will only happen if weapon_state is AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT
 		 * and no weapon has been found that can shoot. */
		if (weapon_status > slotIdx)
			slotIdx = AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT;

		/* select this weapon if this is the one with the shortest range */
		if (weapon_status >= AIRFIGHT_WEAPON_CAN_SHOOT && distance < distance0) {
			slotIdx = i;
			distance0 = distance;
		}
	}
	return slotIdx;
}

/**
 * @brief Calculate the probability to hit the enemy.
 * @param[in] shooter Pointer to the attacking aircraft (may be NULL if a base fires the projectile).
 * @param[in] target Pointer to the aimed aircraft (may be NULL if a target is a base).
 * @param[in] slot Slot containing the weapon firing.
 * @return Probability to hit the target (0 when you don't have a chance, 1 (or more) when you're sure to hit).
 * @note that modifiers due to electronics, weapons, and shield are already taken into account in AII_UpdateAircraftStats
 * @sa AII_UpdateAircraftStats
 * @sa AIRFIGHT_ExecuteActions
 * @sa AIRFIGHT_ChooseWeapon
 * @pre slotIdx must have a weapon installed, with ammo available (see AIRFIGHT_ChooseWeapon)
 * @todo This probability should also depend on the pilot skills, when they will be implemented.
 */
static float AIRFIGHT_ProbabilityToHit (const aircraft_t *shooter, const aircraft_t *target, const aircraftSlot_t *slot)
{
	int idx;
	float probability = 0.0f;

	idx = slot->itemIdx;
	if (idx == NONE) {
		Com_Printf("AIRFIGHT_ProbabilityToHit: no weapon assigned to attacking aircraft\n");
		return probability;
	}

	idx = slot->ammoIdx;
	if (idx == NONE) {
		Com_Printf("AIRFIGHT_ProbabilityToHit: no ammo in weapon of attacking aircraft\n");
		return probability;
	}

	/* Take Base probability from the ammo of the attacking aircraft */
	probability = csi.ods[idx].craftitem.stats[AIR_STATS_ACCURACY];
	Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ProbabilityToHit: Base probablity: %f\n", probability);

	/* Modify this probability by items of the attacking aircraft (stats is in percent) */
	if (shooter)
		probability *= shooter->stats[AIR_STATS_ACCURACY] / 100.0f;

	/* Modify this probability by items of the aimed aircraft (stats is in percent) */
	if (target)
		probability /= target->stats[AIR_STATS_ECM] / 100.0f;

	Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ProbabilityToHit: Probability to hit: %f\n", probability);
	return probability;
}

/**
 * @brief Decide what an attacking aircraft can do.
 * @param[in] shooter The aircraft we attack with.
 * @param[in] target The ufo we are going to attack.
 * @todo Implement me and display an attack popup.
 */
void AIRFIGHT_ExecuteActions (aircraft_t* shooter, aircraft_t* target)
{
	int ammoIdx, slotIdx;
	float probability;

	/* some asserts */
	assert(shooter);

	if (!shooter->baseTarget) {
		assert(target);

		/* Check if the attacking aircraft can shoot */
		slotIdx = AIRFIGHT_ChooseWeapon(shooter->weapons, shooter->maxWeapons, shooter->pos, target->pos);
	} else
		slotIdx = AIRFIGHT_ChooseWeapon(shooter->weapons, shooter->maxWeapons, shooter->pos, shooter->baseTarget->pos);

	/* if weapon found that can shoot */
	if (slotIdx >= AIRFIGHT_WEAPON_CAN_SHOOT) {
		ammoIdx = shooter->weapons[slotIdx].ammoIdx;

		/* shoot */
		if (AIRFIGHT_AddProjectile(NULL, shooter, shooter->baseTarget, target, shooter->weapons + slotIdx)) {
			shooter->weapons[slotIdx].delayNextShot = csi.ods[ammoIdx].craftitem.weaponDelay;
			/* will we miss the target ? */
			probability = frand();
			if (probability > AIRFIGHT_ProbabilityToHit(shooter, target, shooter->weapons + slotIdx))
				AIRFIGHT_MissTarget(&gd.projectiles[gd.numProjectiles - 1], qfalse);
		}
	} else if (slotIdx == AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT) {
		/* no ammo to fire atm (too far or reloading), pursue target */
		if (shooter->type == AIRCRAFT_UFO) {
			if (!shooter->baseTarget)
				AIR_SendUfoPurchasingAircraft(shooter, target);
			else
				/* ufo is attacking a base */
				return;
		} else
			AIR_SendAircraftPurchasingUfo(shooter, target);
	} else {
		/* no ammo left, or no weapon, you should flee ! */
		if (shooter->type == AIRCRAFT_UFO) {
			if (!shooter->baseTarget)
				UFO_FleePhalanxAircraft(shooter, target->pos);
			else
				UFO_FleePhalanxAircraft(shooter, shooter->baseTarget->pos);
		} else {
			MN_AddNewMessage(_("Notice"), _("Our aircraft has no more ammo left - returning to home base now."), qfalse, MSG_STANDARD, NULL);
			AIR_AircraftReturnToBase(shooter);
		}
	}
}

/**
 * @brief Set all projectile aiming a given aircraft to an idle destination.
 * @param[in] aircraft Pointer to the aimed aircraft.
 * @note This function is called when @c aircraft is destroyed.
 * @sa AIRFIGHT_ActionsAfterAirfight
 */
static void AIRFIGHT_RemoveProjectileAimingAircraft (aircraft_t * aircraft)
{
	aircraftProjectile_t *projectile;
	int idx = 0;

	if (!aircraft)
		return;

	for (projectile = gd.projectiles; idx < gd.numProjectiles; projectile++, idx++) {
		if (projectile->aimedAircraft == aircraft)
			AIRFIGHT_MissTarget(projectile, qtrue);
	}
}

/**
 * @brief Set all projectile attackingAircraft pointers to NULL
 * @param[in] aircraft Pointer to the destroyed aircraft.
 * @note This function is called when @c aircraft is destroyed.
 */
static void AIRFIGHT_UpdateProjectileForDestroyedAircraft (const aircraft_t * aircraft)
{
	aircraftProjectile_t *projectile;
	aircraft_t *attacker;
	int idx;

	for (idx = 0, projectile = gd.projectiles; idx < gd.numProjectiles; projectile++, idx++) {
		attacker = projectile->attackingAircraft;

		if (attacker == aircraft)
			projectile->attackingAircraft = NULL;
	}
}

/**
 * @brief Actions to execute when a fight is done.
 * @param[in] shooter Pointer to the aircraft that fired the projectile.
 * @param[in] aircraft Pointer to the aircraft which was destroyed (alien or phalanx).
 * @param[in] battleStatus qtrue if PHALANX won, qfalse if UFO won.
 * @note Some of these mission values are redone (and not reloaded) in CP_Load
 * @note shooter may be NULL
 * @sa UFO_DestroyAllUFOsOnGeoscape_f
 * @sa CP_Load
 * @sa CP_SpawnCrashSiteMission
 */
void AIRFIGHT_ActionsAfterAirfight (aircraft_t *shooter, aircraft_t* aircraft, qboolean phalanxWon)
{
	byte *color;
	int i, j;
	base_t* base;

	if (phalanxWon) {
		assert(aircraft);
		/* get the color value of the map at the crash position */
		color = MAP_GetColor(aircraft->pos, MAPTYPE_TERRAIN);
		/* if this color value is not the value for water ...
		 * and we hit the probability to spawn a crashsite mission */
		if (!MapIsWater(color)) {
			CP_SpawnCrashSiteMission(aircraft);
		} else {
			Com_DPrintf(DEBUG_CLIENT, "AIRFIGHT_ActionsAfterAirfight: zone: %s (%i:%i:%i)\n", MAP_GetTerrainType(color), color[0], color[1], color[2]);
			MN_AddNewMessage(_("Interception"), _("UFO interception successful -- UFO lost to sea."), qfalse, MSG_STANDARD, NULL);
		}
		/* change destination of other projectiles aiming aircraft */
		AIRFIGHT_RemoveProjectileAimingAircraft(aircraft);
		/* now update the projectile for the destroyed aircraft, too */
		AIRFIGHT_UpdateProjectileForDestroyedAircraft(aircraft);

		/* let all the other aircraft return to base that also targetted this
		 * ufo on the geoscape */
		for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
			if (!base->founded)
				continue;
			for (j = 0; j < base->numAircraftInBase; j++) {
				if (base->aircraft[j].status == AIR_UFO
				 && base->aircraft[j].aircraftTarget == aircraft) {
					AIR_AircraftReturnToBase(&base->aircraft[j]);
				}
			}
		}

		/* now remove the ufo from geoscape - the aircraft pointer is no longer
		 * valid after this call */
		UFO_RemoveUfoFromGeoscape(aircraft);
		aircraft = NULL;
	} else {
		/* change destination of other projectiles aiming aircraft */
		AIRFIGHT_RemoveProjectileAimingAircraft(aircraft);

		/* and now update the projectile pointers (there still might be some in the air
		 * of the current destroyed aircraft) - this is needed not send the aircraft
		 * back to base as soon as the projectiles will hit their target */
		AIRFIGHT_UpdateProjectileForDestroyedAircraft(aircraft);

		/* Destroy the aircraft and everything onboard - the aircraft pointer
		 * is no longer valid after this point */
		AIR_DestroyAircraft(aircraft);

		MN_AddNewMessage(_("Interception"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);
	}
}

/**
 * @brief Check if some projectiles on geoscape reached their destination.
 * @note Destination is not necessarily an aircraft, in case the projectile missed its initial target.
 * @param[in] projectile Pointer to the projectile
 * @param[in] movement distance that the projectile will do up to next draw of geoscape
 * @sa AIRFIGHT_CampaignRunProjectiles
 */
static qboolean AIRFIGHT_ProjectileReachedTarget (const aircraftProjectile_t *projectile, float movement)
{
	float distance;

	if (!projectile->aimedAircraft)
		/* the target is idle, its position is in idleTarget*/
		distance = MAP_GetDistance(projectile->idleTarget, projectile->pos);
	else {
		/* the target is moving, pointer to the other aircraft is aimedAircraft */
		distance = MAP_GetDistance(projectile->aimedAircraft->pos, projectile->pos);
	}

	/* projectile reaches its target */
	if (distance < movement) {
		return qtrue;
	}

	/* check if the projectile went farther than it's range */
	distance = (float) projectile->time * csi.ods[projectile->aircraftItemsIdx].craftitem.weaponSpeed / 3600.0f;
	if (distance > csi.ods[projectile->aircraftItemsIdx].craftitem.stats[AIR_STATS_WRANGE]) {
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Calculates the damage value for the airfight
 * @param[in] od The ammo object definition of the craft item
 * @sa AII_UpdateAircraftStats
 * @note ECM is handled in AIRFIGHT_ProbabilityToHit
 */
static int AIRFIGHT_GetDamage (const objDef_t *od, const aircraft_t* target)
{
	int damage;

	assert(od);

	/* already destroyed - do nothing */
	if (target->stats[AIR_STATS_DAMAGE] <= 0)
		return 0;

	/* base damage is given by the ammo */
	damage = od->craftitem.weaponDamage;

	/* reduce damages with shield target */
	damage -= target->stats[AIR_STATS_SHIELD];

	return damage;
}

/**
 * @brief Solve the result of one projectile hitting an aircraft.
 * @param[in] projectile Pointer to the projectile.
 * @note the target loose (base damage - shield of target) hit points
 */
static void AIRFIGHT_ProjectileHits (aircraftProjectile_t *projectile)
{
	aircraft_t *target;
	int damage = 0;

	assert(projectile);
	target = projectile->aimedAircraft;
	assert(target);

	/* if the aircraft is not on geoscape anymore, do nothing (returned to base) */
	if (AIR_IsAircraftInBase(target))
		return;

	damage = AIRFIGHT_GetDamage(&csi.ods[projectile->aircraftItemsIdx], target);

	/* apply resulting damages - but only if damage > 0 - because the target might
	 * already be destroyed, and we don't want to execute the actions after airfight
	 * for every projectile */
	if (damage > 0) {
		assert(target->stats[AIR_STATS_DAMAGE] > 0);
		target->stats[AIR_STATS_DAMAGE] -= damage;
		AIRFIGHT_ActionsAfterAirfight(projectile->attackingAircraft, target, target->type == AIRCRAFT_UFO);
	}
}

/**
 * @brief Solve the result of one projectile hitting a base.
 * @param[in] projectile Pointer to the projectile.
 * @sa B_UpdateBaseData
 * @note Not possible without UFO_ATTACK_BASES set
 */
static void AIRFIGHT_ProjectileHitsBase (aircraftProjectile_t *projectile)
{
	base_t *base;
	int damage = 0;
	int i, rnd;
	qboolean baseAttack = qfalse;

	assert(projectile);
	base = projectile->aimedBase;
	assert(base);

	/* base damage is given by the ammo */
	damage = csi.ods[projectile->aircraftItemsIdx].craftitem.weaponDamage;

	/* damages are divided between defense system and base. */
	damage = frand() * damage;
	base->batteryDamage -= damage;
	base->baseDamage -= csi.ods[projectile->aircraftItemsIdx].craftitem.weaponDamage - damage;

	if (base->batteryDamage <= 0) {
		/* projectile destroyed a base defense system */
		base->batteryDamage = MAX_BATTERY_DAMAGE;
		rnd = rand() % 2;
		if (base->maxBatteries + base->maxLasers <= 0)
			rnd = -1;
		else if (rnd == 0 && base->maxBatteries <= 0)
			rnd = 1;
		else if (rnd == 1 && base->maxLasers <= 0)
			rnd = 0;

		if (rnd == 0) {
			/* Add message to message-system. */
			MN_AddNewMessage(_("Base facility destroyed"), _("You've lost a missile battery system."), qfalse, MSG_CRASHSITE, NULL);
			for (i = 0; i < gd.numBuildings[base->idx]; i++) {
				if (gd.buildings[base->idx][i].buildingType == B_DEFENSE_MISSILE) {
					/* FIXME: Destroy a random one - otherwise the player might 'cheat' with this
					 * e.g. just building an empty defense station (a lot cheaper) */
					B_BuildingDestroy(base, &gd.buildings[base->idx][i]);
					baseAttack = qtrue;
					break;
				}
			}
		} else if (rnd == 1) {
			/* Add message to message-system. */
			MN_AddNewMessage(_("Base facility destroyed"), _("You've lost a laser battery system."), qfalse, MSG_CRASHSITE, NULL);
			for (i = 0; i < gd.numBuildings[base->idx]; i++) {
				if (gd.buildings[base->idx][i].buildingType == B_DEFENSE_LASER) {
					/* FIXME: Destroy a random one - otherwise the player might 'cheat' with this
					 * e.g. just building an empty defense station (a lot cheaper) */
					B_BuildingDestroy(base, &gd.buildings[base->idx][i]);
					baseAttack = qtrue;
					break;
				}
			}
		}
	}

	if (base->baseDamage <= 0) {
		/* projectile destroyed a building */
		base->baseDamage = MAX_BASE_DAMAGE;
		rnd = frand() * gd.numBuildings[base->idx];
		/* Add message to message-system. */
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("You've lost a base facility (%s)."), _(gd.buildings[base->idx][rnd].name));
		MN_AddNewMessage(_("Base facility destroyed"), messageBuffer, qfalse, MSG_BASEATTACK, NULL);
		B_BuildingDestroy(base, &gd.buildings[base->idx][rnd]);
		baseAttack = qtrue;
	}

	/* set the base under attack */
	if (baseAttack) {
		int idx;
		/* if we shoot the attacking craft down, we won't set the base under attack */
		if (projectile->attackingAircraft)
			B_BaseAttack(base);

		/* FIXME: Not the correct place - should be done after you freed your base from the invasion and
		 * return back to geoscape to prevent another base attack just after you've secured it */
		for (idx = gd.numProjectiles - 1; idx >= 0; idx--)
			if (gd.projectiles[idx].aimedBase == base)
				AIRFIGHT_RemoveProjectile(&gd.projectiles[idx]);
	}
}

/**
 * @brief Update values of projectiles.
 * @param[in] dt Time elapsed since last call of this function.
 */
void AIRFIGHT_CampaignRunProjectiles (int dt)
{
	aircraftProjectile_t *projectile;
	int idx;
	float angle;
	float movement;
	vec3_t ortogonalVector, finalPoint, startPoint;

	/* gd.numProjectiles is changed in AIRFIGHT_RemoveProjectile */
	for (idx = gd.numProjectiles - 1; idx >= 0; idx--) {
		projectile = &gd.projectiles[idx];
		projectile->time += dt;
		movement = (float) dt * csi.ods[projectile->aircraftItemsIdx].craftitem.weaponSpeed / 3600.0f;
		/* Check if the projectile reached its destination (aircraft or idle point) */
		if (AIRFIGHT_ProjectileReachedTarget(projectile, movement)) {
			/* check if it got the ennemy */
			if (projectile->aimedBase)
				AIRFIGHT_ProjectileHitsBase(projectile);
			else if (projectile->aimedAircraft)
				AIRFIGHT_ProjectileHits(projectile);

			/* remove the missile from gd.projectiles[] */
			AIRFIGHT_RemoveProjectile(projectile);
		} else {
			/* missile is moving towards its target */
			if (projectile->aimedAircraft)
				angle = MAP_AngleOfPath(projectile->pos, projectile->aimedAircraft->pos, NULL, ortogonalVector);
			else
				angle = MAP_AngleOfPath(projectile->pos, projectile->idleTarget, NULL, ortogonalVector);
			/* udpate angle of the projectile */
			projectile->angle = angle;
			/* update position of the projectile */
			PolarToVec(projectile->pos, startPoint);
			RotatePointAroundVector(finalPoint, ortogonalVector, startPoint, movement);
			VecToPolar(finalPoint, projectile->pos);
			/* Update position of bullets if needed */
			if (projectile->bullets)
				AIRFIGHT_RunBullets(projectile, ortogonalVector, movement);
		}
	}
}

/**
 * @brief Check if one type of battery (missile or laser) can shoot now.
 * @param[in] base Pointer to the firing base.
 * @param[in] slot Pointer to the first weapon slot of the base to use.
 * @param[in] maxSlot number of slot of this type in base.
 * @param[in] targetIdx Pointer to the array of target idx of this defense system.
 */
static void AIRFIGHT_BaseShoot (const base_t *base, aircraftSlot_t *slot, int maxSlot, int *targetIdx)
{
	int i, test;
	float distance;

	for (i = 0; i < maxSlot; i++) {
		/* if no target, can't shoot */
		if (targetIdx[i] == AIRFIGHT_NO_TARGET)
			continue;

		/* if the weapon is not ready in base, can't shoot */
		if (slot[i].installationTime > 0)
			continue;

		/* if weapon is reloading, can't shoot */
		if (slot[i].delayNextShot > 0)
			continue;

		/* check that the ufo is still visible */
		if (!gd.ufos[targetIdx[i]].visible) {
			targetIdx[i] = AIRFIGHT_NO_TARGET;
			continue;
		}

		/* check if we can still fire on this target */
		distance = MAP_GetDistance(base->pos, gd.ufos[targetIdx[i]].pos);
		test = AIRFIGHT_CheckWeapon(slot + i, distance);
		/* weapon unable to shoot, reset target */
		if (test == AIRFIGHT_WEAPON_CAN_NEVER_SHOOT) {
			targetIdx[i] = AIRFIGHT_NO_TARGET;
			continue;
		}
		/* we can't shoot with this weapon atm, wait to see if UFO comes closer */
		else if (test == AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT)
			continue;
		/* target is too far, wait to see if UFO comes closer */
		else if (distance > csi.ods[slot[i].ammoIdx].craftitem.stats[AIR_STATS_WRANGE])
			continue;

		/* shoot */
		if (AIRFIGHT_AddProjectile(base, NULL, NULL, gd.ufos + targetIdx[i], slot + i)) {
			slot[i].delayNextShot = csi.ods[slot[i].ammoIdx].craftitem.weaponDelay;
			/* will we miss the target ? */
			if (frand() > AIRFIGHT_ProbabilityToHit(NULL, gd.ufos + targetIdx[i], slot + i))
				AIRFIGHT_MissTarget(&gd.projectiles[gd.numProjectiles - 1], qfalse);

			/* Check if UFO flees */
			if (!UFO_UFOCanShoot(gd.ufos + targetIdx[i]))
				UFO_FleePhalanxAircraft(gd.ufos + targetIdx[i], base->pos);
		}
	}
}

/**
 * @brief Run base defenses.
 * @param[in] dt Time elapsed since last call of this function.
 */
void AIRFIGHT_CampaignRunBaseDefense (int dt)
{
	base_t* base;
	int idx;

	for (base = gd.bases; base < (gd.bases + gd.numBases); base++) {
		if (!base->founded)
			continue;
		if (base->baseStatus == BASE_UNDER_ATTACK)
			continue;
		for (idx = 0; idx < base->maxBatteries; idx++) {
			if (base->batteries[idx].delayNextShot > 0)
				base->batteries[idx].delayNextShot -= dt;
		}

		for (idx = 0; idx < base->maxLasers; idx++) {
			if (base->lasers[idx].delayNextShot > 0)
				base->lasers[idx].delayNextShot -= dt;
		}

		if (AII_BaseCanShoot(base)) {
			if (base->hasMissile)
				AIRFIGHT_BaseShoot(base, base->batteries, base->maxBatteries, base->targetMissileIdx);
			if (base->hasLaser)
				AIRFIGHT_BaseShoot(base, base->lasers, base->maxLasers, base->targetLaserIdx);
		}
	}
}
