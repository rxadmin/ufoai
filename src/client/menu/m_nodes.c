/**
 * @file m_nodes.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "m_main.h"
#include "m_internal.h"
#include "m_nodes.h"
#include "m_parse.h"
#include "m_input.h"

#include "node/m_node_abstractnode.h"
#include "node/m_node_abstractscrollbar.h"
#include "node/m_node_abstractvalue.h"
#include "node/m_node_bar.h"
#include "node/m_node_base.h"
#include "node/m_node_button.h"
#include "node/m_node_checkbox.h"
#include "node/m_node_controls.h"
#include "node/m_node_cinematic.h"
#include "node/m_node_container.h"
#include "node/m_node_custombutton.h"
#include "node/m_node_editor.h"
#include "node/m_node_image.h"
#include "node/m_node_item.h"
#include "node/m_node_linechart.h"
#include "node/m_node_map.h"
#include "node/m_node_airfightmap.h"
#include "node/m_node_model.h"
#include "node/m_node_optionlist.h"
#include "node/m_node_panel.h"
#include "node/m_node_radar.h"
#include "node/m_node_radiobutton.h"
#include "node/m_node_rows.h"
#include "node/m_node_selectbox.h"
#include "node/m_node_string.h"
#include "node/m_node_special.h"
#include "node/m_node_spinner.h"
#include "node/m_node_tab.h"
#include "node/m_node_tbar.h"
#include "node/m_node_text.h"
#include "node/m_node_textentry.h"
#include "node/m_node_todo.h"
#include "node/m_node_vscrollbar.h"
#include "node/m_node_zone.h"

typedef void (*registerFunction_t)(nodeBehaviour_t *node);

/**
 * @brief List of functions to register nodes
 * @note Functions must be sorted by node name
 */
const static registerFunction_t registerFunctions[] = {
	MN_RegisterNullNode,
	MN_RegisterAbstractBaseNode,
	MN_RegisterAbstractNode,
	MN_RegisterAbstractOptionNode,
	MN_RegisterAbstractScrollbarNode,
	MN_RegisterAbstractValueNode,
	MN_RegisterAirfightMapNode,
	MN_RegisterBarNode,
	MN_RegisterBaseLayoutNode,
	MN_RegisterBaseMapNode,
	MN_RegisterButtonNode,
	MN_RegisterCheckBoxNode,
	MN_RegisterCinematicNode,
	MN_RegisterConFuncNode,
	MN_RegisterContainerNode,
	MN_RegisterControlsNode,
	MN_RegisterCustomButtonNode,
	MN_RegisterCvarFuncNode,
	MN_RegisterEditorNode,
	MN_RegisterFuncNode,
	MN_RegisterItemNode,
	MN_RegisterLineChartNode,
	MN_RegisterMapNode,
	MN_RegisterWindowNode,	/* menu */
	MN_RegisterModelNode,
	MN_RegisterOptionListNode,
	MN_RegisterPanelNode,
	MN_RegisterImageNode,	/* pic */
	MN_RegisterRadarNode,
	MN_RegisterRadioButtonNode,
	MN_RegisterRowsNode,
	MN_RegisterSelectBoxNode,
	MN_RegisterSpinnerNode,
	MN_RegisterStringNode,
	MN_RegisterTabNode,
	MN_RegisterTBarNode,
	MN_RegisterTextNode,
	MN_RegisterTextEntryNode,
	MN_RegisterTodoNode,
	MN_RegisterVScrollbarNode,
	MN_RegisterZoneNode
};
#define NUMBER_OF_BEHAVIOURS lengthof(registerFunctions)

/**
 * @brief List of all node behaviours, indexes by nodetype num.
 */
static nodeBehaviour_t nodeBehaviourList[NUMBER_OF_BEHAVIOURS];

/**
 * @brief Get a property from a behaviour or his inheritance
 * @param[in] node Requested node
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
const value_t *MN_GetPropertyFromBehaviour (const nodeBehaviour_t *behaviour, const char* name)
{
	for (; behaviour; behaviour = behaviour->super) {
		const value_t *result;
		if (behaviour->properties == NULL)
			continue;
		result = MN_FindPropertyByName(behaviour->properties, name);
		if (result)
			return result;
	}
	return NULL;
}

/**
 * @brief Check the if conditions for a given node
 * @sa V_SPECIAL_IF
 * @returns qfalse if the node is not drawn due to not meet if conditions
 */
qboolean MN_CheckVisibility (menuNode_t *node)
{
	if (!node->visibilityCondition)
		return qtrue;
	return MN_CheckCondition(node->visibilityCondition);
}

/**
 * @brief Return a path from a menu to a node
 * @return A path "menuname.nodename.nodename.givennodename"
 * @note Use a static buffer for the result
 */
const char* MN_GetPath (const menuNode_t* node)
{
	static char result[MAX_VAR];
	const menuNode_t* nodes[8];
	int i = 0;

	while (node) {
		assert(i < 8);
		nodes[i] = node;
		node = node->parent;
		i++;
	}

	/** @todo we can use something faster than cat */
	result[0] = '\0';
	while (i) {
		i--;
		Q_strcat(result, nodes[i]->name, sizeof(result));
		if (i > 0)
			Q_strcat(result, ".", sizeof(result));
	}

	return result;
}

/**
 * @brief Return a node by a path name (names with dot separation)
 */
menuNode_t* MN_GetNodeByPath (const char* path)
{
	char name[MAX_VAR];
	menuNode_t* node = NULL;
	const char* nextName;

	nextName = path;
	while (nextName && nextName[0] != '\0') {
		const char* begin = nextName;
		nextName = strstr(begin, ".");
		if (!nextName) {
			Q_strncpyz(name, begin, sizeof(name));
		} else {
			assert(nextName - begin + 1 <= sizeof(name));
			Q_strncpyz(name, begin, nextName - begin + 1);
			nextName++;
		}

		if (node == NULL)
			node = MN_GetMenu(name);
		else
			node = MN_GetNode(node, name);

		if (!node)
			return NULL;
	}

	return node;
}

/**
 * @brief Allocate a node into the menu memory
 * @note Its not a dynamic memory allocation. Please only use it at the loading time
 * @todo Assert out when we are not in parsing/loading stage
 */
menuNode_t* MN_AllocNode (const char* type)
{
	menuNode_t* node = &mn.menuNodes[mn.numNodes++];
	if (mn.numNodes >= MAX_MENUNODES)
		Sys_Error("MN_AllocNode: MAX_MENUNODES hit");
	memset(node, 0, sizeof(*node));
	node->behaviour = MN_GetNodeBehaviour(type);
	return node;
}

/**
 * @brief Return the first visible node at a position
 * @param[in] node Node where we must search
 * @param[in] rx Relative x position to the parent of the node
 * @param[in] ry Relative y position to the parent of the node
 * @return The first visible node at position, else NULL
 */
static menuNode_t *MN_GetNodeInTreeAtPosition (menuNode_t *node, int rx, int ry)
{
	menuNode_t *find;
	menuNode_t *child;
	int i;

	if (node->invis || node->behaviour->isVirtual || !MN_CheckVisibility(node))
		return NULL;

	/* relative to the node */
	rx -= node->pos[0];
	ry -= node->pos[1];

	/* check bounding box */
	if (rx < 0 || ry < 0 || rx >= node->size[0] || ry >= node->size[1])
		return NULL;

	/** @todo we should improve the loop (last-to-first) */
	find = NULL;
	for (child = node->firstChild; child; child = child->next) {
		menuNode_t *tmp;
		tmp = MN_GetNodeInTreeAtPosition(child, rx, ry);
		if (tmp)
			find = tmp;
	}
	if (find)
		return find;

	/* is the node tangible */
	if (node->ghost)
		return NULL;

	/* check excluded box */
	for (i = 0; i < node->excludeRectNum; i++) {
		if (rx >= node->excludeRect[i].pos[0]
		 && rx < node->excludeRect[i].pos[0] + node->excludeRect[i].size[0]
		 && ry >= node->excludeRect[i].pos[1]
		 && ry < node->excludeRect[i].pos[1] + node->excludeRect[i].size[1])
			return NULL;
	}

	/* we are over the node */
	return node;
}

/**
 * @brief Return the first visible node at a position
 */
menuNode_t *MN_GetNodeAtPosition (int x, int y)
{
	int pos;

	/* find the first menu under the mouse */
	for (pos = mn.menuStackPos - 1; pos >= 0; pos--) {
		menuNode_t *menu = mn.menuStack[pos];
		menuNode_t *find;

		/* update the layout */
		menu->behaviour->doLayout(menu);

		find = MN_GetNodeInTreeAtPosition(menu, x, y);
		if (find)
			return find;

		/* we must not search anymore */
		if (menu->u.window.dropdown)
			break;
		if (menu->u.window.modal)
			break;
	}

	return NULL;
}

/**
 * @brief Return a node behaviour by name
 * @note Use a dichotomic search. nodeBehaviourList must be sorted by name.
 */
nodeBehaviour_t* MN_GetNodeBehaviour (const char* name)
{
	unsigned char min = 0;
	unsigned char max = NUMBER_OF_BEHAVIOURS;

	while (min != max) {
		const int mid = (min + max) >> 1;
		const char diff = Q_strcmp(nodeBehaviourList[mid].name, name);
		assert(mid < max);
		assert(mid >= min);

		if (diff == 0)
			return &nodeBehaviourList[mid];

		if (diff > 0)
			max = mid;
		else
			min = mid + 1;
	}

	Sys_Error("Node behaviour '%s' doesn't exist", name);
}

/**
 * @brief Clone a node
 * @param[in] node to clone
 * @param[in] recursive True if we also must clone subnodes
 * @param[in] newMenu Menu where the nodes must be add (this function only link node into menu, note menu into the new node)
 * @todo Properties like CVAR_OR_FLOAT that are using a value don't embed the value, but point to a value.
 * As a result, the new node will share the value with the "base" node.
 * We can embed this values into node.
 * @todo exclude rect is not safe cloned.
 */
menuNode_t* MN_CloneNode (const menuNode_t* node, menuNode_t *newMenu, qboolean recursive)
{
	menuNode_t* newNode = MN_AllocNode(node->behaviour->name);

	/* clone all data */
	*newNode = *node;

	/* clean up node navigation */
	newNode->menu = newMenu;
	newNode->parent = NULL;
	newNode->firstChild = NULL;
	newNode->lastChild = NULL;
	newNode->next = NULL;

	/* clone child */
	if (recursive) {
		menuNode_t* childNode;
		for (childNode = node->firstChild; childNode; childNode = childNode->next) {
			menuNode_t* newChildNode = MN_CloneNode(childNode, newMenu, recursive);
			MN_AppendNode(newNode, newChildNode);
		}
	}
	return newNode;
}

/** @brief position of virtual function into node behaviour */
static const size_t virtualFunctions[] = {
	offsetof(nodeBehaviour_t, draw),
	offsetof(nodeBehaviour_t, drawTooltip),
	offsetof(nodeBehaviour_t, leftClick),
	offsetof(nodeBehaviour_t, rightClick),
	offsetof(nodeBehaviour_t, middleClick),
	offsetof(nodeBehaviour_t, mouseWheel),
	offsetof(nodeBehaviour_t, mouseMove),
	offsetof(nodeBehaviour_t, mouseDown),
	offsetof(nodeBehaviour_t, mouseUp),
	offsetof(nodeBehaviour_t, capturedMouseMove),
	offsetof(nodeBehaviour_t, loading),
	offsetof(nodeBehaviour_t, loaded),
	offsetof(nodeBehaviour_t, doLayout),
	offsetof(nodeBehaviour_t, dndEnter),
	offsetof(nodeBehaviour_t, dndMove),
	offsetof(nodeBehaviour_t, dndLeave),
	offsetof(nodeBehaviour_t, dndDrop),
	offsetof(nodeBehaviour_t, dndFinished),
	offsetof(nodeBehaviour_t, focusGained),
	offsetof(nodeBehaviour_t, focusLost),
	0
};

static void MN_InitializeNodeBehaviour (nodeBehaviour_t* behaviour)
{
	if (behaviour->isInitialized)
		return;

	/** @todo check (when its possible) properties are ordered by name */
	/* check and update properties data */
	if (behaviour->properties) {
		int num = 0;
		const value_t* current = behaviour->properties;
		while (current->string != NULL) {
			num++;
			current++;
		}
		behaviour->propertyCount = num;
	}

	/* everything inherits 'abstractnode' */
	if (behaviour->extends == NULL && Q_strcmp(behaviour->name, "abstractnode") != 0) {
		behaviour->extends = "abstractnode";
	}

	if (behaviour->extends) {
		int i = 0;
		behaviour->super = MN_GetNodeBehaviour(behaviour->extends);
		MN_InitializeNodeBehaviour(behaviour->super);

		while (qtrue) {
			const size_t pos = virtualFunctions[i];
			size_t superFunc;
			size_t func;
			if (pos == 0)
				break;

			/* cache super function if we don't overwrite it */
			superFunc = *(size_t*)((char*)behaviour->super + pos);
			func = *(size_t*)((char*)behaviour + pos);
			if (func == 0 && superFunc != 0)
				*(size_t*)((char*)behaviour + pos) = superFunc;

			i++;
		}
	}

	behaviour->isInitialized = qtrue;
}

void MN_InitNodes (void)
{
	int i = 0;
	nodeBehaviour_t *current = nodeBehaviourList;

	/* compute list of node behaviours */
	for (i = 0; i < NUMBER_OF_BEHAVIOURS; i++) {
		registerFunctions[i](current);
		current++;
	}

	/* check for safe data: list must be sorted by alphabet */
	current = nodeBehaviourList;
	assert(current);
	for (i = 0; i < NUMBER_OF_BEHAVIOURS - 1; i++) {
		const nodeBehaviour_t *a = current;
		const nodeBehaviour_t *b = current + 1;
		assert(b);
		if (Q_strcmp(a->name, b->name) >= 0) {
#ifdef DEBUG
			Sys_Error("MN_InitNodes: '%s' is before '%s'. Please order node behaviour registrations by name\n", a->name, b->name);
#else
			Sys_Error("MN_InitNodes: Error: '%s' is before '%s'\n", a->name, b->name);
#endif
		}
		current++;
	}

	/* finalize node behaviour initialization */
	current = nodeBehaviourList;
	for (i = 0; i < NUMBER_OF_BEHAVIOURS; i++) {
		MN_InitializeNodeBehaviour(current);
		current++;
	}
}
