/* 
 * Copyright (C) 2002 Free Software Foundation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors : Eskil Heyn Olsen <eskil@eskil.dk>
 */

/*

This is a small theming library tailored for my applets.

 */

#ifndef _littleksin_h_
#define _littleksin_h_

#include <glib.h>
#include <gtk/gtkwidget.h>

typedef struct _LittleSkinPrivate LittleSkinPrivate;

typedef enum {
	LITTLE_SKIN_PEACHY,
	LITTLE_SKIN_NO_SUCH_THEME,
	LITTLE_SKIN_NO_THEMES,
	LITTLE_SKIN_GAP,
	LITTLE_SKIN_OVERLAP
} LittleSkinError;

typedef struct {
	LittleSkinPrivate *priv;

	/* Current theme */
	char *theme;
	/* Available themes */
	GList *themes;

	GtkWidget *pixbuf;

	LittleSkinError error;
} LittleSkin;

/*
  Structure that describes how to load images.
  Image filenames are constructed from "filename"-X[-Y]."ext",
  where [-Y] is only used if range_size is non-zero,
  in which case, the X and Y's must go from 0 to range_size, 
  and no overlap.
 */
typedef struct {
	char *filename;
	int range_size;
	int loaded;
} LittleSkinLoad;

#define LITTLE_SKIN_LOAD(s) ((LittleSkinLoad*)(s))
LittleSkinLoad* little_skin_load_new (const char *name, int range_size);
void little_skin_load_destroy (LittleSkinLoad *load);


#define LITTLE_SKIN(s) ((LittleSkin*)(s))
/*
  load is a GList<LittleSkinLoad*>, containing which images to load. Order is significant.
  Each structure will have "loaded" set to the actual number of images loaded.
  Unless everything is bad, it'll return non-null, and set error to some value.
  error == LITTLE_SKIN_PEACHY means all is well, OVERLAP or GAP means
  some filename sequence has a gap or overlap in the naming, but will still
  be usefull.

  If theme_name is NULL or doesn't exist, "default" is used.
  theme_name = "none" will just render an empty pixbuf.

 */
LittleSkin* little_skin_new (const char *directory,
			     const char *theme_name,
			     GList *load);

void little_skin_destroy (LittleSkin *skin);		

void little_skin_set_image_widget (LittleSkin *skin, GtkWidget *image);
GtkWidget* little_skin_get_image_widget (LittleSkin *skin);

void little_skin_set_theme (LittleSkin *skin, const char *theme_name);

void little_skin_redraw (LittleSkin *skin);

void little_skin_set_image (LittleSkin *skin, const char *name, int image_number);

void little_skin_start_animation (LittleSkin *skin, const char *anim_name, int interval_ms);
void little_skin_stop_animation (LittleSkin *skin);

gboolean little_skin_has_segment (LittleSkin *skin, const char *segment);

GList* little_skin_get_segment_names (LittleSkin *skin);
const GList* little_skin_get_theme_names (LittleSkin *skin);
int little_skin_get_segment_range (LittleSkin *skin, const char *segment);

gboolean little_skin_is_segment_animation (LittleSkin *skin, const char *segment);
gboolean little_skin_is_animating (LittleSkin *skin);
void little_skin_set_alpha (LittleSkin *skin, gboolean alpha);
void little_skin_set_alpha_level (LittleSkin *skin, int alpha_level);
void little_skin_set_alpha_pixbuf (LittleSkin *skin, GdkPixbuf *pixbuf);
GdkPixbuf *little_skin_get_alpha_pixbuf (LittleSkin *skin);
void little_skin_set_alpha_color (LittleSkin *skin, GdkColor *color);
#endif /* _littleksin_h_ */
