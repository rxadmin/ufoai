/**
 * @file m_node_window.h
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

#ifndef CLIENT_MENU_M_NODE_WINDOW_H
#define CLIENT_MENU_M_NODE_WINDOW_H

#include "../../../shared/mathlib.h"

/* prototype */
struct menuNode_s;
struct menuAction_s;
struct nodeBehaviour_s;

/**
 * @brief extradata for the window node
 */
typedef struct {
	int eventTime;
	vec2_t noticePos; 		/**< the position where the cl.msgText messages are rendered */
	qboolean dragButton;
	qboolean closeButton;
	qboolean preventTypingEscape;
	qboolean modal;
	qboolean dropdown;		/**< very special property force the menu to close if we click outside */
	qboolean isFullScreen;

	struct menuNode_s *parent;	/**< to create child window */

	/** @todo we can remove it if we create a node for the battlescape */
	struct menuNode_s *renderNode;

	/** @todo think about converting it to action instead of node */
	struct menuNode_s *eventNode;	/**< single 'func' node, or NULL */
	struct menuAction_s *onInit; 	/**< Call when the menu is push */
	struct menuAction_s *onClose;	/**< Call when the menu is pop */
	struct menuAction_s *onTimeOut;	/**< Call when the own timer of the menu out */
	struct menuAction_s *onLeave;	/**< Call when mouse leave the window? call by cl_input */

} windowExtraData_t;

void MN_RegisterWindowNode(struct nodeBehaviour_s *behaviour);

qboolean MN_WindowIsFullScreen(struct menuNode_s* const menu);

#endif
