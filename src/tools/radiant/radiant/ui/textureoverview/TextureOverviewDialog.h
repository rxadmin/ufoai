/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef TEXTUREOVERVIEWDIALOG_H_
#define TEXTUREOVERVIEWDIALOG_H_

#include <gtk/gtkwidget.h>
#include <gtk/gtkliststore.h>
#include "gtkutil/window/BlockingTransientWindow.h"

namespace ui
{
	class TextureOverviewDialog: public gtkutil::BlockingTransientWindow
	{

	public:
		TextureOverviewDialog ();
		~TextureOverviewDialog ();

	private:

		// Main container widget
		GtkWidget* _widget;

		// Info table list store
		GtkListStore* _infoStore;

		// This is called to initialise the dialog window / create the widgets
		void populateWindow ();

		// Helper method to create the close button
		GtkWidget* createButtons ();

		// The callback for the buttons
		static void onClose (GtkWidget* widget, TextureOverviewDialog* self);
	};
}

#endif
