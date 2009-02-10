/**
 * @file cl_mapfightequip.c
 * @brief contains everything related to equiping slots of aircraft or base
 * @note Base defence functions prefix: BDEF_
 * @note Aircraft items slots functions prefix: AIM_
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

#include "../client.h"
#include "../cl_global.h"
#include "cl_mapfightequip.h"
#include "../menu/node/m_node_text.h"
#include "cl_fightequip_callbacks.h"

/**
 * @brief Returns craftitem weight based on size.
 * @param[in] od Pointer to objDef_t object being craftitem.
 * @return itemWeight_t
 * @sa AII_WeightToName
 */
itemWeight_t AII_GetItemWeightBySize (const objDef_t *od)
{
	assert(od);
	assert(od->craftitem.type >= 0);

	if (od->size < 50)
		return ITEM_LIGHT;
	else if (od->size < 100)
		return ITEM_MEDIUM;
	else
		return ITEM_HEAVY;
}

/**
 * @brief Check if an aircraft item should or should not be displayed in airequip menu
 * @param[in] base Pointer to a base.
 * @param[in] installation Pointer to a installation.
 * @param[in] aircraft Pointer to a aircraft.
 * @param[in] tech Pointer to the technology to test
 * @return qtrue if the aircraft item should be displayed, qfalse else
 */
qboolean AIM_SelectableAircraftItem (base_t* base, installation_t* installation, aircraft_t *aircraft, const technology_t *tech, const int airequipID)
{
	objDef_t *item;
	aircraftSlot_t *slot;

	if (aircraft)
		slot = AII_SelectAircraftSlot(aircraft, airequipID);
	else if (base)
		slot = BDEF_SelectBaseSlot(base, airequipID);
	else if (installation)
		slot = BDEF_SelectInstallationSlot(installation, airequipID);
	else {
		Com_Printf("AIM_SelectableAircraftItem: no aircraft, no base and no installation given\n");
		return qfalse;
	}

	/* no slot somehow */
	if (!slot)
		return qfalse;

	/* item is researched? */
	if (!RS_IsResearched_ptr(tech))
		return qfalse;

	item = AII_GetAircraftItemByID(tech->provides);
	if (!item)
		return qfalse;

	/* you can choose an ammo only if it fits the weapons installed in this slot */
	if (airequipID >= AC_ITEM_AMMO) {
		/** @todo This only works for ammo that is useable in exactly one weapon
		 * check the weap_idx array and not only the first value */
		if (!slot->nextItem && item->weapons[0] != slot->item)
			return qfalse;

		/* are we trying to change ammos for nextItem? */
		if (slot->nextItem && item->weapons[0] != slot->nextItem)
			return qfalse;
	}

	/* you can install an item only if its weight is small enough for the slot */
	if (AII_GetItemWeightBySize(item) > slot->size)
		return qfalse;

	/* you can't install an item that you don't possess
	 * unlimited ammo don't need to be possessed */
	if (aircraft) {
		if (aircraft->homebase->storage.num[item->idx] <= 0 && !item->notOnMarket  && !item->craftitem.unlimitedAmmo)
			return qfalse;
	} else if (base) {
		if (base->storage.num[item->idx] <= 0 && !item->notOnMarket && !item->craftitem.unlimitedAmmo)
			return qfalse;
	} else if (installation) {
		if (installation->storage.num[item->idx] <= 0 && !item->notOnMarket && !item->craftitem.unlimitedAmmo)
			return qfalse;
	}


	/* you can't install an item that does not have an installation time (alien item)
	 * except for ammo which does not have installation time */
	if (item->craftitem.installationTime == -1 && airequipID < AC_ITEM_AMMO)
		return qfalse;

	return qtrue;
}

/**
 * @brief Checks to see if the pilot is in any aircraft at this base.
 * @param[in] base Which base has the aircraft to search for the employee in.
 * @param[in] type Which employee to search for.
 * @return qtrue or qfalse depending on if the employee was found on the base aircraft.
 */
qboolean AIM_PilotAssignedAircraft (const base_t* base, const employee_t* pilot)
{
	int i;
	qboolean found = qfalse;

	for (i = 0; i < base->numAircraftInBase; i++) {
		const aircraft_t *aircraft = &base->aircraft[i];
		if (aircraft->pilot == pilot) {
			found = qtrue;
			break;
		}
	}

	return found;
}

/**
 * @brief Adds a defence system to base.
 * @param[in] basedefType Base defence type (see basedefenceType_t)
 * @param[in] base Pointer to the base in which the battery will be added
 */
static void BDEF_AddBattery (basedefenceType_t basedefType, base_t* base)
{
	switch (basedefType) {
	case BASEDEF_MISSILE:
		if (base->numBatteries >= MAX_BASE_SLOT) {
			Com_Printf("BDEF_AddBattery: too many missile batteries in base\n");
			return;
		}

		base->numBatteries++;
		break;
	case BASEDEF_LASER:
		if (base->numLasers >= MAX_BASE_SLOT) {
			Com_Printf("BDEF_AddBattery: too many laser batteries in base\n");
			return;
		}
		/* slots has a lot of ammo for now */
		/** @todo it should be unlimited, no ? check that when we'll know how laser battery work */
		base->lasers[base->numLasers].slot.ammoLeft = 9999;

		base->numLasers++;
		break;
	default:
		Com_Printf("BDEF_AddBattery: unknown type of base defence system.\n");
	}
}

/**
 * @brief Reload the battery of every bases
 * @todo we should define the number of ammo to reload and the period of reloading in the .ufo file
 */
void BDEF_ReloadBattery (void)
{
	int i, j;

	/* Reload all ammos of aircraft */
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		for (j = 0; j < base->numBatteries; j++) {
			if (base->batteries[j].slot.ammoLeft >= 0 && base->batteries[j].slot.ammoLeft < 20)
				base->batteries[j].slot.ammoLeft++;
		}
	}
}

/**
 * @brief Adds a defence system to base.
 */
void BDEF_AddBattery_f (void)
{
	int basedefType, baseIdx;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <basedefType> <baseIdx>", Cmd_Argv(0));
		return;
	} else {
		basedefType = atoi(Cmd_Argv(1));
		baseIdx = atoi(Cmd_Argv(2));
	}

	/* Check that the baseIdx exists */
	if (baseIdx < 0 || baseIdx >= ccs.numBases) {
		Com_Printf("BDEF_AddBattery_f: baseIdx %i doesn't exists: there is only %i bases in game.\n", baseIdx, ccs.numBases);
		return;
	}

	/* Check that the basedefType exists */
	if (basedefType != BASEDEF_MISSILE && basedefType != BASEDEF_LASER) {
		Com_Printf("BDEF_AddBattery_f: base defence type %i doesn't exists.\n", basedefType);
		return;
	}

	BDEF_AddBattery(basedefType, B_GetBaseByIDX(baseIdx));
}

/**
 * @brief Remove a base defence sytem from base.
 * @param[in] basedefType (see basedefenceType_t)
 * @param[in] idx idx of the battery to destroy (-1 if this is random)
 */
void BDEF_RemoveBattery (base_t *base, basedefenceType_t basedefType, int idx)
{
	assert(base);

	/* Select the type of base defence system to destroy */
	switch (basedefType) {
	case BASEDEF_MISSILE: /* this is a missile battery */
		/* we must have at least one missile battery to remove it */
		assert(base->numBatteries > 0);
		if (idx < 0)
			idx = rand() % base->numBatteries;
		REMOVE_ELEM(base->batteries, idx, base->numBatteries);
		/* just for security */
		AII_InitialiseSlot(&base->batteries[base->numBatteries].slot, NULL, base, NULL, AC_ITEM_BASE_MISSILE);
		break;
	case BASEDEF_LASER: /* this is a laser battery */
		/* we must have at least one laser battery to remove it */
		assert(base->numLasers > 0);
		if (idx < 0)
			idx = rand() % base->numLasers;
		REMOVE_ELEM(base->lasers, idx, base->numLasers);
		/* just for security */
		AII_InitialiseSlot(&base->lasers[base->numLasers].slot, NULL, base, NULL, AC_ITEM_BASE_LASER);
		break;
	default:
		Com_Printf("BDEF_RemoveBattery_f: unknown type of base defence system.\n");
	}
}

/**
 * @brief Remove a defence system from base.
 * @note 1st argument is the basedefence system type to destroy (sa basedefenceType_t).
 * @note 2nd argument is the idx of the base in which you want the battery to be destroyed.
 * @note if the first argument is BASEDEF_RANDOM, the type of the battery to destroy is randomly selected
 * @note the building must already be removed from gd.buildings[baseIdx][]
 */
void BDEF_RemoveBattery_f (void)
{
	int basedefType, baseIdx;
	base_t *base;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <basedefType> <baseIdx>", Cmd_Argv(0));
		return;
	} else {
		basedefType = atoi(Cmd_Argv(1));
		baseIdx = atoi(Cmd_Argv(2));
	}

	/* Check that the baseIdx exists */
	if (baseIdx < 0 || baseIdx >= ccs.numBases) {
		Com_Printf("BDEF_RemoveBattery_f: baseIdx %i doesn't exists: there is only %i bases in game.\n", baseIdx, ccs.numBases);
		return;
	}

	base = B_GetFoundedBaseByIDX(baseIdx);
	if (!base) {
		Com_Printf("BDEF_RemoveBattery_f: baseIdx %i is not founded.\n", baseIdx);
		return;
	}

	if (basedefType == BASEDEF_RANDOM) {
		/* Type of base defence to destroy is randomly selected */
		if (base->numBatteries <= 0 && base->numLasers <= 0) {
			Com_Printf("No base defence to destroy\n");
			return;
		} else if (base->numBatteries <= 0) {
			/* only laser battery is possible */
			basedefType = BASEDEF_LASER;
		} else if (base->numLasers <= 0) {
			/* only missile battery is possible */
			basedefType = BASEDEF_MISSILE;
		} else {
			/* both type are possible, choose one randomly */
			basedefType = rand() % 2 + BASEDEF_MISSILE;
		}
	} else {
		/* Check if the removed building was under construction */
		int i, type, max;
		int workingNum = 0;

		switch (basedefType) {
		case BASEDEF_MISSILE:
			type = B_DEFENCE_MISSILE;
			max = base->numBatteries;
			break;
		case BASEDEF_LASER:
			type = B_DEFENCE_MISSILE;
			max = base->numLasers;
			break;
		default:
			Com_Printf("BDEF_RemoveBattery_f: base defence type %i doesn't exists.\n", basedefType);
			return;
		}

		for (i = 0; i < gd.numBuildings[baseIdx]; i++) {
			if (gd.buildings[baseIdx][i].buildingType == type
				&& gd.buildings[baseIdx][i].buildingStatus == B_STATUS_WORKING)
				workingNum++;
		}

		if (workingNum == max) {
			/* Removed building was under construction, do nothing */
			return;
		} else if ((workingNum != max - 1)) {
			/* Should never happen, we only remove building one by one */
			Com_Printf("BDEF_RemoveBattery_f: Error while checking number of batteries (%i instead of %i) in base '%s'.\n",
				workingNum, max, base->name);
			return;
		}

		/* If we reached this point, that means we are removing a working building: continue */
	}

	BDEF_RemoveBattery(base, basedefType, -1);
}

/**
 * @brief Initialise all values of base slot defence.
 * @param[in] base Pointer to the base which needs initalisation of its slots.
 */
void BDEF_InitialiseBaseSlots (base_t *base)
{
	int i;

	for (i = 0; i < MAX_BASE_SLOT; i++) {
		AII_InitialiseSlot(&base->batteries[i].slot, NULL, base, NULL, AC_ITEM_BASE_MISSILE);
		AII_InitialiseSlot(&base->lasers[i].slot, NULL, base, NULL, AC_ITEM_BASE_LASER);
		base->batteries[i].target = NULL;
		base->lasers[i].target = NULL;
	}
}

/**
 * @brief Initialise all values of installation slot defence.
 * @param[in] Pointer to the installation which needs initalisation of its slots.
 */
void BDEF_InitialiseInstallationSlots (installation_t *installation)
{
	int i;

	for (i = 0; i < installation->installationTemplate->maxBatteries; i++) {
		AII_InitialiseSlot(&installation->batteries[i].slot, NULL, NULL, installation, AC_ITEM_BASE_MISSILE);
		installation->batteries[i].target = NULL;
	}
}


/**
 * @brief Update the installation delay of one slot.
 * @param[in] base Pointer to the base to update the storage and capacity for
 * @param[in] aircraft Pointer to the aircraft (NULL if a base is updated)
 * @param[in] slot Pointer to the slot to update
 * @sa AII_AddItemToSlot
 */
static void AII_UpdateOneInstallationDelay (base_t* base, installation_t* installation, aircraft_t *aircraft, aircraftSlot_t *slot)
{
	assert(base || installation);

	/* if the item is already installed, nothing to do */
	if (slot->installationTime == 0)
		return;
	else if (slot->installationTime > 0) {
		/* the item is being installed */
		slot->installationTime--;
		/* check if installation is over */
		if (slot->installationTime <= 0) {
			/* Update stats values */
			if (aircraft) {
				AII_UpdateAircraftStats(aircraft);
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
						_("%s was successfully installed into aircraft %s at base %s."),
						_(slot->item->name), _(aircraft->name), aircraft->homebase->name);
				MSO_CheckAddNewMessage(NT_INSTALLATION_INSTALLED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
			} else if (installation) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s was successfully installed at installation %s."),
						_(slot->item->name), installation->name);
				MSO_CheckAddNewMessage(NT_INSTALLATION_INSTALLED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
			} else {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s was successfully installed at base %s."),
						_(slot->item->name), base->name);
				MSO_CheckAddNewMessage(NT_INSTALLATION_INSTALLED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
			}
		}
	} else if (slot->installationTime < 0) {
		const objDef_t *olditem;

		/* the item is being removed */
		slot->installationTime++;
		if (slot->installationTime >= 0) {
#ifdef DEBUG
			if (aircraft && aircraft->homebase != base)
				Sys_Error("AII_UpdateOneInstallationDelay: aircraft->homebase and base pointers are out of sync\n");
#endif
			olditem = slot->item;
			AII_RemoveItemFromSlot(base, slot, qfalse);
			if (aircraft) {
				AII_UpdateAircraftStats(aircraft);
				/* Only stop time and post a notice, if no new item to install is assigned */
				if (!slot->item) {
					Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
							_("%s was successfully removed from aircraft %s at base %s."),
							_(olditem->name), _(aircraft->name), base->name);
					MSO_CheckAddNewMessage(NT_INSTALLATION_REMOVED, _("Notice"), cp_messageBuffer, qfalse,
							MSG_STANDARD, NULL);
				} else {
					Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
							_ ("%s was successfully removed, starting installation of %s into aircraft %s at base %s"),
							_(olditem->name), _(slot->item->name), _(aircraft->name), base->name);
					MSO_CheckAddNewMessage(NT_INSTALLATION_REPLACE, _("Notice"), cp_messageBuffer, qfalse,
							MSG_STANDARD, NULL);
				}
			} else if (!slot->item) {
				if (installation) {
					Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
							_("%s was successfully removed from installation %s."),
							_(olditem->name), installation->name);
					MSO_CheckAddNewMessage(NT_INSTALLATION_REMOVED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD,
							NULL);
				} else {
					Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s was successfully removed from base %s."),
							_(olditem->name), base->name);
					MSO_CheckAddNewMessage(NT_INSTALLATION_REMOVED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD,
							NULL);
				}
			}
		}
	}
}

/**
 * @brief Update the installation delay of all slots of a given aircraft.
 * @note hourly called
 * @sa CL_CampaignRun
 * @sa AII_UpdateOneInstallationDelay
 */
void AII_UpdateInstallationDelay (void)
{
	int i, j, k;

	for (j = 0; j < MAX_INSTALLATIONS; j++) {
		installation_t *installation = INS_GetFoundedInstallationByIDX(j);
		if (!installation)
			continue;

		/* Update base */
		for (k = 0; k < installation->installationTemplate->maxBatteries; k++)
			AII_UpdateOneInstallationDelay(NULL, installation, NULL, &installation->batteries[k].slot);
	}

	for (j = 0; j < MAX_BASES; j++) {
		aircraft_t *aircraft;
		base_t *base = B_GetFoundedBaseByIDX(j);
		if (!base)
			continue;

		/* Update base */
		for (k = 0; k < base->numBatteries; k++)
			AII_UpdateOneInstallationDelay(base, NULL, NULL, &base->batteries[k].slot);
		for (k = 0; k < base->numLasers; k++)
			AII_UpdateOneInstallationDelay(base, NULL, NULL, &base->lasers[k].slot);

		/* Update each aircraft */
		for (i = 0, aircraft = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, aircraft++)
			if (aircraft->homebase) {
				assert(aircraft->homebase == base);
				if (AIR_IsAircraftInBase(aircraft)) {
					/* Update electronics delay */
					for (k = 0; k < aircraft->maxElectronics; k++)
						AII_UpdateOneInstallationDelay(base, NULL, aircraft, aircraft->electronics + k);

					/* Update weapons delay */
					for (k = 0; k < aircraft->maxWeapons; k++)
						AII_UpdateOneInstallationDelay(base, NULL, aircraft, aircraft->weapons + k);

					/* Update shield delay */
					AII_UpdateOneInstallationDelay(base, NULL, aircraft, &aircraft->shield);
				}
			}
	}
}

/**
 * @brief Auto add ammo corresponding to weapon, if there is enough in storage.
 * @param[in] base Pointer to the base where you want to add/remove ammo (needed even for aicraft:
 * we need to know where to add / remove items. Maybe be NULL for installations or UFOs.
 * @param[in] installation Pointer to the installation where you want to add/remove ammo
 * @param[in] aircraft Pointer to the aircraft (NULL if we are changing base defence system)
 * @param[in] slot Pointer to the slot where you want to add ammo
 * @sa AIM_AircraftEquipAddItem_f
 * @sa AII_RemoveItemFromSlot
 */
void AIM_AutoAddAmmo (base_t *base, installation_t *installation, aircraft_t *aircraft, aircraftSlot_t *slot, const int airequipID)
{
	int k;
	const qboolean nextAmmo = (qboolean) slot->nextItem;	/**< do we search an ammo for next item? */
	const technology_t *ammo_tech;
	const objDef_t *ammo;
	const objDef_t *item;

	assert(slot);

	/* Get the weapon (either current weapon or weapon to install after this one is removed) */
	item = nextAmmo ? slot->nextItem : slot->item;

	if (!item)
		return;

	if (item->craftitem.type > AC_ITEM_WEAPON)
		return;

	/* don't try to add ammo to a slot that already has ammo */
	if (nextAmmo ? slot->nextAmmo : slot->ammo)
		return;

	/* Try every ammo usable with this weapon until we find one we have in storage */
	for (k = 0; k < item->numAmmos; k++) {
		ammo = item->ammos[k];
		if (ammo) {
			ammo_tech = ammo->tech;
			if (ammo_tech && AIM_SelectableAircraftItem(base, installation, aircraft, ammo_tech, airequipID)) {
				AII_AddAmmoToSlot((ammo->notOnMarket || ammo->craftitem.unlimitedAmmo) ? NULL : base, ammo_tech, slot);
				break;
			}
		}
	}
}

/**
 * @brief Move the item in the slot (or optionally its ammo only) to the base storage.
 * @note if there is another item to install after removal, begin this installation.
 * @param[in] base The base to add the item to (may be NULL if item shouldn't be removed from any base).
 * @param[in] slot The slot to remove the item from.
 * @param[in] ammo qtrue if we want to remove only ammo. qfalse if the whole item should be removed.
 * @sa AII_AddItemToSlot
 * @sa AII_AddAmmoToSlot
 */
void AII_RemoveItemFromSlot (base_t* base, aircraftSlot_t *slot, qboolean ammo)
{
	assert(slot);

	if (ammo) {
		/* only remove the ammo */
		if (slot->nextAmmo) {
			if (base)
				B_UpdateStorageAndCapacity(base, slot->nextAmmo, 1, qfalse, qfalse);
			slot->nextAmmo = NULL;
		} else if (slot->ammo) {
			if (base)
				B_UpdateStorageAndCapacity(base, slot->ammo, 1, qfalse, qfalse);
			slot->ammo = NULL;
		}
		return;
	} else if (slot->nextItem) {
		/* Remove nextItem */
		if (base)
			B_UpdateStorageAndCapacity(base, slot->nextItem, 1, qfalse, qfalse);
		slot->nextItem = NULL;
		/* also remove ammo if any */
		if (slot->nextAmmo)
			AII_RemoveItemFromSlot(base, slot, qtrue);
	} else if (slot->item) {
		if (base)
			B_UpdateStorageAndCapacity(base, slot->item, 1, qfalse, qfalse);
		/* the removal is over */
		if (slot->nextItem) {
			/* there is anoter item to install after this one */
			slot->item = slot->nextItem;
			/* we already removed nextItem from storage when it has been added to slot: don't use B_UpdateStorageAndCapacity */
			slot->item = slot->nextItem;
			slot->installationTime = slot->item->craftitem.installationTime;
			slot->nextItem = NULL;
		} else {
			slot->item = NULL;
			slot->installationTime = 0;
		}
		/* also remove ammo */
		AII_RemoveItemFromSlot(base, slot, qtrue);
	}
}

/**
 * @brief Add an ammo to an aircraft weapon slot
 * @note No check for the _type_ of item is done here, so it must be done before.
 * @param[in] base Pointer to the base which provides items (NULL if items shouldn't be removed of storage)
 * @param[in] tech Pointer to the tech to add to slot
 * @param[in] slot Pointer to the slot where you want to add ammos
 * @sa AII_AddItemToSlot
 */
qboolean AII_AddAmmoToSlot (base_t* base, const technology_t *tech, aircraftSlot_t *slot)
{
	objDef_t *ammo;

	assert(slot);
	assert(tech);

	ammo = AII_GetAircraftItemByID(tech->provides);
	if (!ammo) {
		Com_Printf("AII_AddAmmoToSlot: Could not add item (%s) to slot\n", tech->provides);
		return qfalse;
	}

	/* the base pointer can be null here - e.g. in case you are equipping a UFO
	 * and base ammo defence are not stored in storage */
	if (base && ammo->craftitem.type <= AC_ITEM_AMMO) {
		if (base->storage.num[ammo->idx] <= 0) {
			Com_Printf("AII_AddAmmoToSlot: No more ammo of this type to equip (%s)\n", ammo->id);
			return qfalse;
		}
	}

	/* remove any applied ammo in the current slot */
	if (slot->nextItem) {
		if (slot->nextAmmo)
		AII_RemoveItemFromSlot(base, slot, qtrue);
		slot->nextAmmo = ammo;
	} else {
		/* you shouldn't be able to have nextAmmo set if you don't have nextItem set */
		assert(!slot->nextAmmo);
		AII_RemoveItemFromSlot(base, slot, qtrue);
		slot->ammo = ammo;
	}

	/* the base pointer can be null here - e.g. in case you are equipping a UFO */
	if (base && !ammo->craftitem.unlimitedAmmo)
		B_UpdateStorageAndCapacity(base, ammo, -1, qfalse, qfalse);

	/* proceed only if we are changing ammo of current weapon */
	if (slot->nextItem)
		return qtrue;
	/* some weapons have unlimited ammo */
	if (ammo->craftitem.unlimitedAmmo) {
		slot->ammoLeft = AMMO_STATUS_UNLIMITED;
	} else if (slot->aircraft)
		AII_ReloadWeapon(slot->aircraft);

	return qtrue;
}

/**
 * @brief Add an item to an aircraft slot
 * @param[in] base Pointer to the base where item will be removed (NULL for ufos, unlimited ammos or while loading game)
 * @param[in] tech Pointer to the tech that will be added in this slot.
 * @param[in] slot Pointer to the aircraft, base, or installation slot.
 * @param[in] nextItem False if we are changing current item in slot, true if this is the item to install
 * after current removal is over.
 * @note No check for the _type_ of item is done here.
 * @sa AII_UpdateOneInstallationDelay
 * @sa AII_AddAmmoToSlot
 */
qboolean AII_AddItemToSlot (base_t* base, const technology_t *tech, aircraftSlot_t *slot, qboolean nextItem)
{
	objDef_t *item;

	assert(slot);
	assert(tech);

	item = AII_GetAircraftItemByID(tech->provides);
	if (!item) {
		Com_Printf("AII_AddItemToSlot: Could not add item (%s) to slot\n", tech->provides);
		return qfalse;
	}

	/* Sanity check : the type of the item should be the same than the slot type */
	if (slot->type != item->craftitem.type) {
		Com_Printf("AII_AddItemToSlot: Type of the item to install (%s -- %i) doesn't match type of the slot (%i)\n", item->id, item->craftitem.type, slot->type);
		return qfalse;
	}

#ifdef DEBUG
	/* Sanity check : the type of the item cannot be an ammo */
	/* note that this should never be reached because a slot type should never be an ammo
	 * , so the test just before should be wrong */
	if (item->craftitem.type >= AC_ITEM_AMMO) {
		Com_Printf("AII_AddItemToSlot: Type of the item to install (%s) should be a weapon, a shield, or electronics (no ammo)\n", item->id);
		return qfalse;
	}
#endif

	/* the base pointer can be null here - e.g. in case you are equipping a UFO */
	if (base) {
		if (base->storage.num[item->idx] <= 0) {
			Com_Printf("AII_AddItemToSlot: No more item of this type to equip (%s)\n", item->id);
			return qfalse;
		}
	}

	if (slot->size >= AII_GetItemWeightBySize(item)) {
		if (nextItem)
			slot->nextItem = item;
		else {
			slot->item = item;
			slot->installationTime = item->craftitem.installationTime;
		}
		/* the base pointer can be null here - e.g. in case you are equipping a UFO
		 * Remove item even for nextItem, this way we are sure we won't use the same item
		 * for another aircraft. */
		if (base)
			B_UpdateStorageAndCapacity(base, item, -1, qfalse, qfalse);
	} else {
		Com_Printf("AII_AddItemToSlot: Could not add item '%s' to slot %i (slot-size: %i - item-weight: %i)\n",
			item->id, slot->idx, slot->size, item->size);
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Auto Add weapon and ammo to an aircraft.
 * @param[in] aircraft Pointer to the aircraft
 * @note This is used to auto equip interceptor of first base.
 * @sa B_SetUpBase
 */
void AIM_AutoEquipAircraft (aircraft_t *aircraft)
{
	int i;
	aircraftSlot_t *slot;
	objDef_t *item;
	const technology_t *tech = RS_GetTechByID("rs_craft_weapon_sparrowhawk");

	if (!tech)
		Sys_Error("Could not get tech rs_craft_weapon_sparrowhawk");

	assert(aircraft);
	assert(aircraft->homebase);

	item = AII_GetAircraftItemByID(tech->provides);
	if (!item)
		return;

	for (i = 0; i < aircraft->maxWeapons; i++) {
		slot = &aircraft->weapons[i];
		if (slot->size < AII_GetItemWeightBySize(item))
			continue;
		if (aircraft->homebase->storage.num[item->idx] <= 0)
			continue;
		AII_AddItemToSlot(aircraft->homebase, tech, slot, qfalse);
		AIM_AutoAddAmmo(aircraft->homebase, NULL, aircraft, slot, AC_ITEM_WEAPON);
		slot->installationTime = 0;
	}

	/* Fill slots too small for sparrowhawk with shiva */
	tech = RS_GetTechByID("rs_craft_weapon_shiva");

	if (!tech)
		Sys_Error("Could not get tech rs_craft_weapon_shiva");

	item = AII_GetAircraftItemByID(tech->provides);

	if (!item)
		return;

	for (i = 0; i < aircraft->maxWeapons; i++) {
		slot = &aircraft->weapons[i];
		if (slot->size < AII_GetItemWeightBySize(item))
			continue;
		if (aircraft->homebase->storage.num[item->idx] <= 0)
			continue;
		if (slot->item)
			continue;
		AII_AddItemToSlot(aircraft->homebase, tech, slot, qfalse);
		AIM_AutoAddAmmo(aircraft->homebase, NULL, aircraft, slot, AC_ITEM_WEAPON);
		slot->installationTime = 0;
	}

	AII_UpdateAircraftStats(aircraft);
}

/**
 * @brief Initialise values of one slot of an aircraft or basedefence common to all types of items.
 * @param[in] slot	Pointer to the slot to initialize.
 * @param[in] aircraftTemplate	Pointer to aircraft template.
 * @param[in] base	Pointer to base.
 * @param[in] type
 */
void AII_InitialiseSlot (aircraftSlot_t *slot, aircraft_t *aircraft, base_t *base, installation_t *installation, aircraftItemType_t type)
{
	assert((!base && aircraft) || (base && !aircraft) || (installation && !aircraft));	/* Only one of them is allowed. */
	assert((!base && installation) || (base && !installation) || (!base && !installation)); /* Only one of them is allowed or neither. */

	memset(slot, 0, sizeof(slot)); /* all values to 0 */
	slot->aircraft = aircraft;
	slot->base = base;
	slot->installation = installation;
	slot->item = NULL;
	slot->ammo = NULL;
	slot->nextAmmo = NULL;
	slot->size = ITEM_HEAVY;
	slot->nextItem = NULL;
	slot->type = type;
	slot->ammoLeft = AMMO_STATUS_NOT_SET; /** sa BDEF_AddBattery: it needs to be AMMO_STATUS_NOT_SET and not 0 @sa B_SaveBaseSlots */
	slot->installationTime = 0;
}

/**
 * @brief Check if item in given slot should change one aircraft stat.
 * @param[in] slot Pointer to the slot containing the item
 * @param[in] stat the stat that should be checked
 * @return qtrue if the item should change the stat.
 */
static qboolean AII_CheckUpdateAircraftStats (const aircraftSlot_t *slot, int stat)
{
	const objDef_t *item;

	assert(slot);

	/* there's no item */
	if (!slot->item)
		return qfalse;

	/* you can not have advantages from items if it is being installed or removed, but only disavantages */
	if (slot->installationTime != 0) {
		item = slot->item;
		if (item->craftitem.stats[stat] > 1.0f) /* advantages for relative and absolute values */
			return qfalse;
	}

	return qtrue;
}

/**
 * @brief Get the maximum weapon range of aircraft.
 * @param[in] slot Pointer to the aircrafts weapon slot list.
 * @param[in] maxSlot maximum number of weapon slots in aircraft.
 * @return Maximum weapon range for this aircaft as an angle.
 */
float AIR_GetMaxAircraftWeaponRange (const aircraftSlot_t *slot, int maxSlot)
{
	int i;
	float range = 0.0f;

	assert(slot);

	/* We choose the usable weapon with the biggest range */
	for (i = 0; i < maxSlot; i++) {
		const aircraftSlot_t *weapon = slot + i;
		const objDef_t *ammo = weapon->ammo;

		if (!ammo)
			continue;

		/* make sure this item is useable */
		if (!AII_CheckUpdateAircraftStats(slot, AIR_STATS_WRANGE))
			continue;

		/* select this weapon if this is the one with the longest range */
		if (ammo->craftitem.stats[AIR_STATS_WRANGE] > range) {
			range = ammo->craftitem.stats[AIR_STATS_WRANGE];
		}
	}
	return range;
}

/**
 * @brief Repair aircraft.
 * @note Hourly called.
 */
void AII_RepairAircraft (void)
{
	int baseIDX, aircraftIDX;
	const int REPAIR_PER_HOUR = 1;	/**< Number of damage points repaired per hour */

	for (baseIDX = 0; baseIDX < MAX_BASES; baseIDX++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIDX);
		if (!base)
			continue;

		for (aircraftIDX = 0; aircraftIDX < base->numAircraftInBase; aircraftIDX++) {
			aircraft_t *aircraft = &base->aircraft[aircraftIDX];

			if (!AIR_IsAircraftInBase(aircraft))
				continue;
			aircraft->damage = min(aircraft->damage + REPAIR_PER_HOUR, aircraft->stats[AIR_STATS_DAMAGE]);
		}
	}
}

/**
 * @brief Update the value of stats array of an aircraft.
 * @param[in] aircraft Pointer to the aircraft
 * @note This should be called when an item starts to be added/removed and when addition/removal is over.
 */
void AII_UpdateAircraftStats (aircraft_t *aircraft)
{
	int i, currentStat;
	const aircraft_t *source;
	const objDef_t *item;

	assert(aircraft);

	source = aircraft->tpl;

	for (currentStat = 0; currentStat < AIR_STATS_MAX; currentStat++) {
		/* we scan all the stats except AIR_STATS_WRANGE (see below) */
		if (currentStat == AIR_STATS_WRANGE)
			continue;

		/* initialise value */
		aircraft->stats[currentStat] = source->stats[currentStat];

		/* modify by electronics (do nothing if the value of stat is 0) */
		for (i = 0; i < aircraft->maxElectronics; i++) {
			if (!AII_CheckUpdateAircraftStats (&aircraft->electronics[i], currentStat))
				continue;
			item = aircraft->electronics[i].item;
			if (fabs(item->craftitem.stats[currentStat]) > 2.0f)
				aircraft->stats[currentStat] += (int) item->craftitem.stats[currentStat];
			else if (!equal(item->craftitem.stats[currentStat], 0))
				aircraft->stats[currentStat] *= item->craftitem.stats[currentStat];
		}

		/* modify by weapons (do nothing if the value of stat is 0)
		 * note that stats are not modified by ammos */
		for (i = 0; i < aircraft->maxWeapons; i++) {
			if (!AII_CheckUpdateAircraftStats (&aircraft->weapons[i], currentStat))
				continue;
			item = aircraft->weapons[i].item;
			if (fabs(item->craftitem.stats[currentStat]) > 2.0f)
				aircraft->stats[currentStat] += item->craftitem.stats[currentStat];
			else if (!equal(item->craftitem.stats[currentStat], 0))
				aircraft->stats[currentStat] *= item->craftitem.stats[currentStat];
		}

		/* modify by shield (do nothing if the value of stat is 0) */
		if (AII_CheckUpdateAircraftStats (&aircraft->shield, currentStat)) {
			item = aircraft->shield.item;
			if (fabs(item->craftitem.stats[currentStat]) > 2.0f)
				aircraft->stats[currentStat] += item->craftitem.stats[currentStat];
			else if (!equal(item->craftitem.stats[currentStat], 0))
				aircraft->stats[currentStat] *= item->craftitem.stats[currentStat];
		}
	}

	/* now we update AIR_STATS_WRANGE (this one is the biggest range of every ammo) */
	aircraft->stats[AIR_STATS_WRANGE] = 1000.0f * AIR_GetMaxAircraftWeaponRange(aircraft->weapons, aircraft->maxWeapons);

	/* check that aircraft hasn't too much fuel (caused by removal of fuel pod) */
	if (aircraft->fuel > aircraft->stats[AIR_STATS_FUELSIZE])
		aircraft->fuel = aircraft->stats[AIR_STATS_FUELSIZE];

	/* check that aircraft hasn't too much HP (caused by removal of armour) */
	if (aircraft->damage > aircraft->stats[AIR_STATS_DAMAGE])
		aircraft->damage = aircraft->stats[AIR_STATS_DAMAGE];

	/* check that speed of the aircraft is positive */
	if (aircraft->stats[AIR_STATS_SPEED] < 1)
		aircraft->stats[AIR_STATS_SPEED] = 1;

	/* Update aircraft state if needed */
	if (aircraft->status == AIR_HOME && aircraft->fuel < aircraft->stats[AIR_STATS_FUELSIZE])
		aircraft->status = AIR_REFUEL;
}

/**
 * @brief Returns the amount of assigned items for a given slot of a given aircraft
 * @param[in] type This is the slot type to get the amount of assigned items for
 * @param[in] aircraft The aircraft to count the items for (may not be NULL)
 * @return The amount of assigned items for the given slot
 */
int AII_GetSlotItems (aircraftItemType_t type, const aircraft_t *aircraft)
{
	int i, max, cnt = 0;
	const aircraftSlot_t *slot;

	assert(aircraft);

	switch (type) {
	case AC_ITEM_SHIELD:
		if (aircraft->shield.item)
			return 1;
		else
			return 0;
		break;
	case AC_ITEM_WEAPON:
		slot = aircraft->weapons;
		max = MAX_AIRCRAFTSLOT;
		break;
	case AC_ITEM_ELECTRONICS:
		slot = aircraft->electronics;
		max = MAX_AIRCRAFTSLOT;
		break;
	default:
		Com_Printf("AIR_GetSlotItems: Unknow type of slot : %i", type);
		return 0;
	}

	for (i = 0; i < max; i++)
		if (slot[i].item)
			cnt++;

	return cnt;
}

/**
 * @brief Check if the aircraft has weapon and ammo
 * @param[in] aircraft The aircraft to count the items for (may not be NULL)
 * @return qtrue if the aircraft can fight, qfalse else
 * @sa AII_BaseCanShoot
 */
int AII_AircraftCanShoot (const aircraft_t *aircraft)
{
	int i;

	assert(aircraft);

	for (i = 0; i < aircraft->maxWeapons; i++)
		if (AIRFIGHT_CheckWeapon(&aircraft->weapons[i], 0) != AIRFIGHT_WEAPON_CAN_NEVER_SHOOT)
			return qtrue;

	return qfalse;
}

/**
 * @brief Check if base or installation weapon can shoot
 * @param[in] weapons Pointer to the weapon array of the base.
 * @param[in] numWeapons Pointer to the number of weapon in this base.
 * @return qtrue if the base can fight, qfalse else
 * @sa AII_BaseCanShoot
 */
static qboolean AII_WeaponsCanShoot (const baseWeapon_t *weapons, const int *numWeapons)
{
	int i;

	for (i = 0; i < *numWeapons; i++) {
		if (AIRFIGHT_CheckWeapon(&weapons[i].slot, 0) != AIRFIGHT_WEAPON_CAN_NEVER_SHOOT)
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Check if the base has weapon and ammo
 * @param[in] base Pointer to the base you want to check (may not be NULL)
 * @return qtrue if the base can shoot, qflase else
 * @sa AII_AircraftCanShoot
 */
int AII_BaseCanShoot (const base_t *base)
{
	assert(base);

	if (B_GetBuildingStatus(base, B_DEFENCE_MISSILE)) {
		/* base has missile battery and any needed building */
		return AII_WeaponsCanShoot(base->batteries, &base->numBatteries);
	}

	if (B_GetBuildingStatus(base, B_DEFENCE_LASER)) {
		/* base has laser battery and any needed building */
		return AII_WeaponsCanShoot(base->lasers, &base->numLasers);
	}

	return qfalse;
}

/**
 * @brief Check if the installation has a weapon and ammo
 * @param[in] installation Pointer to the installation you want to check (may not be NULL)
 * @return qtrue if the installation can shoot, qflase else
 * @sa AII_AircraftCanShoot
 */
qboolean AII_InstallationCanShoot (const installation_t *installation)
{
	assert(installation);

	if (installation->founded && installation->installationStatus == INSTALLATION_WORKING
	 && installation->installationTemplate->maxBatteries > 0) {
		/* installation is working and has battery */
		return AII_WeaponsCanShoot(installation->batteries, &installation->installationTemplate->maxBatteries);
	}

	return qfalse;
}

/**
 * @brief Translate a weight int to a translated string
 * @sa itemWeight_t
 * @sa AII_GetItemWeightBySize
 */
const char* AII_WeightToName (itemWeight_t weight)
{
	switch (weight) {
	case ITEM_LIGHT:
		return _("Light weight");
		break;
	case ITEM_MEDIUM:
		return _("Medium weight");
		break;
	case ITEM_HEAVY:
		return _("Heavy weight");
		break;
	default:
		return _("Unknown weight");
		break;
	}
}
