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

#ifndef _littleksin_private_h_
#define _littleksin_private_h_

#include <glib.h>

typedef struct {
	char *name;
	int begin, end;
} LittleSkinImage;

#define LITTLE_SKIN_IMAGE(s) ((LittleSkinImage*)(s))
LittleSkinImage* little_skin_image_new ();
void little_skin_image_destroy (LittleSkinImage *image);

typedef struct {
	char *name;
	GList *images; /* <LittleSkinImage*> */
	int range;
	gboolean animation;
} LittleSkinSegment;

struct _LittleSkinPrivate {
	GList *segments; /* <LittleSkinSegment*> */
	char *dir;

	/* GHashTable<GList<char*>>, so you lookup segment name, index to
	   the n'th element of the list, and you have the filename to show */
	GHashTable *table; 

  GList *loads;
  
  /* animation info */
  guint anim_id;
  char *anim_current;
  int anim_image_num;

  GdkPixbuf *current_pixbuf;
  
  /* alpha stuff */
  gboolean alpha;
  int alpha_level;
  GdkPixbuf *alpha_pixbuf;
  GdkColor *alpha_color;
};

#define LITTLE_SKIN_SEGMENT(s) ((LittleSkinSegment*)(s))
LittleSkinSegment* little_skin_segment_new ();
void little_skin_segment_destroy (LittleSkinSegment *segment);

#endif /* _littleksin_h_ */
