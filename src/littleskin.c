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

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>

#include <gnome.h>
#include <panel-applet.h>
#include <libgnomevfs/gnome-vfs.h>
#include <eel/eel.h>

#include "littleskin.h"
#include "littleskin-private.h"

//#define debug(...)
#define debug(foo...) g_message(foo)

/* What extensions do we try to load */
char *pixmap_extensions[] = 
{
	".png",
	".jpg",
	".xpm",
	NULL
};

static void
fuck_off_and_die (char *arse, GdkPixbuf *pixbuf) {
  GtkWidget *window, *hbox;
  int h, w;
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), 
		      GTK_WIDGET (gtk_image_new_from_pixbuf (pixbuf)),
		      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), 
		      gtk_label_new (arse),
		      FALSE, FALSE, 0);

  /*
  gtk_box_pack_start (GTK_BOX (hbox), 
		      GTK_WIDGET (gtk_image_new_from_pixmap (pixmap, NULL)),
		      FALSE, FALSE, 0);
  */

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  if (pixbuf == NULL) {
    gtk_window_set_title (GTK_WINDOW (window), "ARSE");
  } else {
    gtk_window_set_title (GTK_WINDOW (window), "fuck off and die");
  }
  gtk_container_add (GTK_CONTAINER (window), hbox);
  gtk_widget_show_all (window);
}


/******************************************************************************/

LittleSkinImage*
little_skin_image_new () {
	LittleSkinImage *result =  g_new0 (LittleSkinImage, 1);
	result->begin = -1;
	result->end = -1;
	return result;
}

void
little_skin_image_destroy (LittleSkinImage *image) {
	g_free (image->name);
	g_free (image);
}

/******************************************************************************/

LittleSkinSegment*
little_skin_segment_new () {
	return g_new0 (LittleSkinSegment, 1);
}

void
little_skin_segment_destroy (LittleSkinSegment *segment) {
	g_list_foreach (segment->images, (GFunc)little_skin_image_destroy, NULL);
	g_list_free (segment->images);
	//g_free (segment->dir);
	g_free (segment);
}

/******************************************************************************/

static void
ls_alpha_redraw (LittleSkin *skin) {
  int w, h;
  GdkPixbuf *tmp = NULL;
  
  w = gdk_pixbuf_get_width (skin->priv->current_pixbuf);
  h = gdk_pixbuf_get_height (skin->priv->current_pixbuf);

  if (skin->priv->alpha_pixbuf) {
    tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
			  TRUE, 
			  gdk_pixbuf_get_bits_per_sample (skin->priv->current_pixbuf), 
			  w, h);
    gdk_pixbuf_copy_area (skin->priv->alpha_pixbuf, 0, 0, w, h,
			  tmp, 0, 0);      

    gdk_pixbuf_composite (skin->priv->current_pixbuf,
			  tmp,
			  0, 0, w, h,
			  0.0, 0.0, 1.0, 1.0,			      
			  GDK_INTERP_HYPER,
			  skin->priv->alpha_level);
  } else {
    /* must be color */
    if (!gdk_colormap_alloc_color (gdk_colormap_get_system (), 
				   skin->priv->alpha_color, FALSE, FALSE)) {
      fuck_off_and_die ("NOGO", NULL);
      return;
    }
    
    tmp = gdk_pixbuf_composite_color_simple (skin->priv->current_pixbuf,
					     w, h,
					     GDK_INTERP_HYPER,
					     255, 
					     8, 
					     skin->priv->alpha_color->pixel,
					     skin->priv->alpha_color->pixel);
    
    gdk_colormap_free_colors (gdk_colormap_get_system (),
			      &skin->priv->alpha_color->pixel,
			      1);
  }

  gtk_image_set_from_pixbuf (GTK_IMAGE (skin->pixbuf), tmp);
  gdk_pixbuf_unref (G_OBJECT (tmp));
}

static gboolean
ls_free_table_list (char *key, GList *list, gpointer unused) {
	g_free (key);
	g_list_free (list);
	return TRUE;
}

static void
ls_free_some_structs (LittleSkin *skin) {
	if (skin->priv->table) {
		g_hash_table_foreach_remove (skin->priv->table, 
					     (GHRFunc)ls_free_table_list,
					     NULL);
		g_hash_table_destroy (skin->priv->table);
		skin->priv->table = NULL;
	}
	if (skin->priv->segments) {
		g_list_foreach (skin->priv->segments, (GFunc)little_skin_segment_destroy, NULL);
		g_list_free (skin->priv->segments);
		skin->priv->segments = NULL;
	}
}

static gint
ls_compare_image (LittleSkinImage *a, LittleSkinImage *b) {
	if (a->end < b->begin) return -1;
	if (a->begin > b->end) return 1;
	return 0;
}

static void
ls_load_theme_image (LittleSkin *skin,
		     LittleSkinLoad *load,
		     const char *themedir,
		     GnomeVFSFileInfo *dirent,
		     LittleSkinSegment *segment)
{
	int j;
	char *dot_pos = strrchr (dirent->name, '.');

	/* This could be moved up to calling function for slight efficiency */
	char *scanf_match, *match;	
	int match_len;
	match = g_strdup_printf ("%s-", load->filename);
	if (load->range_size) {				
		scanf_match = g_strdup_printf ("%s-%%d-%%d.", load->filename);
	} else {
		scanf_match = g_strdup_printf ("%s-%%d.", load->filename);
	}
	match_len = strlen (match);

	/* Check the allowed extensions */
	for (j = 0; pixmap_extensions[j]; j++) {
		if (strcasecmp (dot_pos, pixmap_extensions[j])==0) { 
			int i;
			int pixmap_offset_begin = 0;
			int pixmap_offset_end = 0;
			char *dupe;		       			

			if (strncmp (dirent->name, match, match_len) == 0) {
				LittleSkinImage *image = little_skin_image_new ();
				sscanf (dirent->name, scanf_match, &(image->begin), &(image->end));
				image->name = g_strdup (dirent->name);
				if (image->end < 0) {
					debug ("ls_load_theme_image : adding animation step %d %s ", 
					       image->begin,
					       image->name);
				} else {
					debug ("ls_load_theme_image : adding range %d - %d %s ", 
					       image->begin,
					       image->end,
					       image->name);
				}
				segment->images = g_list_insert_sorted (segment->images, 
									image,
									(GCompareFunc)ls_compare_image);
			}
		}
	}	
	g_free (match);
	g_free (scanf_match);
}

static LittleSkinSegment*
ls_load_segment (LittleSkin *skin, 
		 const char *themedir, 
		 GnomeVFSDirectoryHandle *dir, 
		 LittleSkinLoad *load) {
	LittleSkinSegment *result = g_new0 (LittleSkinSegment, 1);
	GnomeVFSFileInfo *dirent;
	GnomeVFSResult vfsresult;
	int nlen = strlen (load->filename);

	debug ("ls_load_segment : %s, %s %d", 
	       load->filename, 
	       load->range_size>=0 ? "range size" : "animation",
	       load->range_size);

	load->loaded = 0;
	result->animation = load->range_size > 0 ? FALSE : TRUE;
	result->name = g_strdup (load->filename);

	dirent = gnome_vfs_file_info_new ();
	vfsresult = gnome_vfs_directory_read_next (dir, dirent);
	while (vfsresult == GNOME_VFS_OK) {
		debug ("ls_load_segment : %s", gnome_vfs_result_to_string (vfsresult));
		if (dirent->type == GNOME_VFS_FILE_TYPE_REGULAR) {
			if (strrchr (dirent->name, '.')!=NULL && 
			    strncmp (dirent->name, load->filename, nlen)==0) {
				debug ("ls_load_segment : checking %s", dirent->name);
				ls_load_theme_image (skin,
						     load,
						     themedir,
						     dirent,
						     result);
			}
		}		
		gnome_vfs_file_info_clear (dirent);
		vfsresult = gnome_vfs_directory_read_next (dir, dirent);
	}
	debug ("ls_load_segment : %s", gnome_vfs_result_to_string (vfsresult));
	gnome_vfs_file_info_unref (dirent);

	if (result->images) { 
		load->loaded = 1; 
		result->range = load->range_size;
	};

	return result;
}

static void
ls_debug_segments (LittleSkin *skin) {
	GList *seg_ptr = skin->priv->segments;
	int seg_cnt = 0;
	while (seg_ptr) {
		GList *img_ptr;
		int img_cnt = 0;
		debug ("Segment %d : %s (%s)", seg_cnt, 
		       LITTLE_SKIN_SEGMENT (seg_ptr->data)->name,
		       LITTLE_SKIN_SEGMENT (seg_ptr->data)->animation ? "anim" : "range");
		
		img_ptr = LITTLE_SKIN_SEGMENT (seg_ptr->data)->images;

		while (img_ptr) {
			debug ("\t%s (%d-%d)", 
			       LITTLE_SKIN_IMAGE (img_ptr->data)->name,
			       LITTLE_SKIN_IMAGE (img_ptr->data)->begin,
			       LITTLE_SKIN_IMAGE (img_ptr->data)->end);
			img_ptr = img_ptr->next;
			img_cnt++;
		}
		
		seg_ptr = seg_ptr->next;
		seg_cnt ++;
	}
}

static void
ls_build_table (LittleSkin *skin) {
	GList *seg_ptr = skin->priv->segments;

	g_assert (skin->priv->table == NULL);
	skin->priv->table = g_hash_table_new (g_str_hash, g_str_equal);

	while (seg_ptr) {
		GList *img_ptr;
		GList *images = NULL;
		int img_cnt = 0;
		
		img_ptr = LITTLE_SKIN_SEGMENT (seg_ptr->data)->images;

		while (img_ptr) {
			if (LITTLE_SKIN_SEGMENT (seg_ptr->data)->animation) {
				images = g_list_append (images, LITTLE_SKIN_IMAGE (img_ptr->data)->name);
				debug ("ls_build_table : %s/%s/%d = %s",
				       skin->theme,
				       LITTLE_SKIN_SEGMENT (seg_ptr->data)->name,
				       img_cnt,
				       LITTLE_SKIN_IMAGE (img_ptr->data)->name);
				img_cnt++;
			} else 
				while (img_cnt <= LITTLE_SKIN_IMAGE (img_ptr->data)->end) {
					debug ("ls_build_table : %s/%s/%d = %s",
					       skin->theme,
					       LITTLE_SKIN_SEGMENT (seg_ptr->data)->name,
					       img_cnt,
					       LITTLE_SKIN_IMAGE (img_ptr->data)->name);
					images = g_list_append (images, LITTLE_SKIN_IMAGE (img_ptr->data)->name);
					img_cnt++;
				}
			img_ptr = img_ptr->next;
		}		
		debug ("ls_build_table : inserting %d images in table under %s",
		       g_list_length (images),
		       LITTLE_SKIN_SEGMENT (seg_ptr->data)->name);
		g_hash_table_insert (skin->priv->table, 
				     g_strdup (LITTLE_SKIN_SEGMENT (seg_ptr->data)->name), 
				     images);
		seg_ptr = seg_ptr->next;
	}	
}

static void
ls_load_theme (LittleSkin *skin,
	       const char *theme_name,
	       GList *load) {
	int cnt = 0;
	GnomeVFSDirectoryHandle *dir;
	char *themedir;
	GList *images;
	int total_images_to_be_read;

	debug ("ls_load_theme (%p, %s, %s)", skin, skin->priv->dir, theme_name);

	ls_free_some_structs (skin);

	themedir = g_strdup_printf ("%s/%s", skin->priv->dir, theme_name);
	if (gnome_vfs_directory_open (&dir, themedir, GNOME_VFS_FILE_INFO_DEFAULT|GNOME_VFS_FILE_INFO_FOLLOW_LINKS) == GNOME_VFS_OK) {
		GList *ptr = load;
		while (ptr) {
			LittleSkinSegment *segment = ls_load_segment (skin, 
								      themedir, 
								      dir, 
								      LITTLE_SKIN_LOAD (ptr->data));
			if (segment->images) {
				skin->priv->segments = g_list_append (skin->priv->segments, segment);
			} else {
				little_skin_segment_destroy (segment);
			}
			
			/* bah, no rewind ?! */
			gnome_vfs_directory_close (dir);
			gnome_vfs_directory_open (&dir, themedir, GNOME_VFS_FILE_INFO_DEFAULT|GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
			ptr = ptr->next;
		}
	} else  {
		skin->error = LITTLE_SKIN_NO_THEMES;
	} 
	gnome_vfs_directory_close (dir);
	g_free (themedir);

	ls_debug_segments (skin);
	ls_build_table (skin);
	little_skin_set_image (skin, LITTLE_SKIN_LOAD (load->data)->filename, 0);
}

static void
ls_read_themes (LittleSkin *skin) {
	GList *list;
	struct dirent *dirent;
	char *themedir;
	int vfs_res;

	g_assert (skin->priv->dir);
	debug ("ls_read_themes (%p, %s)", skin, skin->priv->dir);
	if ((vfs_res = gnome_vfs_directory_list_load (&list, skin->priv->dir, 
						      GNOME_VFS_FILE_INFO_GET_MIME_TYPE
						      | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE
						      | GNOME_VFS_FILE_INFO_FOLLOW_LINKS)) != GNOME_VFS_OK) {		
		skin->error = LITTLE_SKIN_NO_THEMES;
		debug ("ls_read_themes : gnome_vfs_directory_list_load error: %s",
		       gnome_vfs_result_to_string (vfs_res));
		return;
	} else {
		GList *ptr = list;
		while (ptr) {
			GnomeVFSFileInfo *info = ptr->data;
			debug ("ls_read_themes : checking %s", info->name);
			if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				if (strrchr (info->name, '.')==NULL) {
					debug ("ls_read_themes : found %s", info->name);
					skin->themes = g_list_append (skin->themes, 
								      g_strdup (info->name));
				}
			}
			ptr = ptr->next;
		}
	}
	gnome_vfs_file_info_list_free (list);
	skin->themes = g_list_sort (skin->themes, (GCompareFunc)g_strcasecmp);
}

gboolean
ls_has_theme (LittleSkin *skin,
	      const char *theme_name) {
	GList *ptr = skin->themes;
	while (ptr) {
		if (strcmp (ptr->data, theme_name)==0) return TRUE;
		ptr = ptr->next;
	}
	return FALSE;
}

gboolean
ls_animate (LittleSkin *skin) {
	GList *images = g_hash_table_lookup (skin->priv->table, skin->priv->anim_current);
	skin->priv->anim_image_num ++;
	if (skin->priv->anim_image_num > g_list_length (images)) {
		skin->priv->anim_image_num = 0;
	}
	little_skin_set_image (skin, skin->priv->anim_current, skin->priv->anim_image_num);
	return TRUE;
}

/******************************************************************************/

LittleSkinLoad* 
little_skin_load_new (const char *name, 
		      int range_size) {
	LittleSkinLoad *load = g_new0 (LittleSkinLoad, 1);
	load->filename = g_strdup (name);
	load->range_size = range_size;
	return load;
}

void little_skin_load_destroy (LittleSkinLoad *load) {
	g_free (load->filename);
	g_free (load);
}

/******************************************************************************/

LittleSkin* 
little_skin_new (const char *directory,
		 const char *theme_name,
		 GList *load) {
	LittleSkin *result = g_new0 (LittleSkin, 1);

	if (!gnome_vfs_initialized ()) {
		gnome_vfs_init ();
	}

	result->error = LITTLE_SKIN_PEACHY;
	result->priv = g_new0 (LittleSkinPrivate, 1);
	result->priv->dir = g_strdup (directory);
	result->priv->loads = load;
	result->priv->anim_id = 0;
	result->priv->anim_current = 0;
	result->priv->anim_image_num = 0;
	result->priv->current_pixbuf = NULL;
	result->priv->alpha = FALSE;
	result->priv->alpha_level = 255;
	result->priv->alpha_pixbuf = NULL;
	result->pixbuf = gtk_image_new ();

	ls_read_themes (result);
	if (result->error != LITTLE_SKIN_PEACHY) goto out;
	if (ls_has_theme (result, theme_name)) {		
		result->theme = g_strdup (theme_name);
		ls_load_theme (result, theme_name, load);
	} else {
		result->error = LITTLE_SKIN_NO_SUCH_THEME;
		result->theme = NULL;
		if (ls_has_theme (result, "default")) {
			result->theme = g_strdup ("default");
			ls_load_theme (result, "default", load);
		}
	}
	
 out:
	return result;
}

void 
little_skin_destroy (LittleSkin *skin) {
        /* blow away the segment tables */
	ls_free_some_structs (skin);
	/* blow away the alpha pixbufs */
	little_skin_set_alpha (skin, FALSE); 
	g_free (skin->priv->dir);
	g_free (skin->priv);	
	
	g_list_foreach (skin->themes, (GFunc)g_free, NULL);
	g_free (skin);
}

void 
little_skin_set_image_widget (LittleSkin *skin, GtkWidget *pixbuf) {
	if (skin->pixbuf) gtk_object_unref (GTK_OBJECT (skin->pixbuf));
	gtk_object_ref (GTK_OBJECT (pixbuf));
	skin->pixbuf = pixbuf;
}

void 
little_skin_redraw (LittleSkin *skin) {
  //fuck_off_and_die (skin->priv->alpha ? "alpha draw" : "no alpha_draw", NULL);
  if (skin->priv->alpha) {
    ls_alpha_redraw (skin);
  } else {
    gtk_image_set_from_pixbuf (GTK_IMAGE (skin->pixbuf), 
			       skin->priv->current_pixbuf);
  }
}

void 
little_skin_set_theme (LittleSkin *skin, const char *theme_name) {
	debug ("little_skin_set_theme : %s (%s)", 
	       theme_name, 
	       ls_has_theme (skin, theme_name) ? "OK" : "NOEXIST");
	if (ls_has_theme (skin, theme_name)) {
		g_free (skin->theme);
		skin->theme = g_strdup (theme_name);
		ls_load_theme (skin, skin->theme, skin->priv->loads);
	} else {
		skin->error = LITTLE_SKIN_NO_SUCH_THEME;
	}
}

void 
little_skin_set_image (LittleSkin *skin, const char *name, int image_number) {
	GList *index;
	GList *image;

	index = g_hash_table_lookup (skin->priv->table, name);
	if (index) {
		image = g_list_nth (index, image_number);
		if (image) {
			char *filename = g_strdup_printf ("%s/%s/%s", 
							  skin->priv->dir,
							  skin->theme,
							  image->data);
			debug ("little_skin_set_image : %s:%d = %s", name, image_number, filename);

			if (skin->priv->current_pixbuf) {
			  g_object_unref (G_OBJECT (skin->priv->current_pixbuf));
			}
			skin->priv->current_pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
			g_free (filename);
		} else {
			debug ("no such image (%d)", image_number);
		}
	} else {
		debug ("no such segment (%s)", name);
	}
}

void 
little_skin_start_animation (LittleSkin *skin, const char *anim_name, int interval_ms) {
	if (skin->priv->anim_id != 0) {
		little_skin_stop_animation (skin);
	}
	skin->priv->anim_current = g_strdup (anim_name);
	skin->priv->anim_id = gtk_timeout_add (interval_ms, 
					       (GtkFunction)ls_animate, 
					       skin);
}

void 
little_skin_stop_animation (LittleSkin *skin) {
	if (skin->priv->anim_id != 0) {
		gtk_timeout_remove (skin->priv->anim_id);
		skin->priv->anim_id = 0;
		g_free (skin->priv->anim_current);
		skin->priv->anim_current = NULL;
	}
}

GtkWidget* 
little_skin_get_image_widget (LittleSkin *skin) {
	return skin->pixbuf;
}

gboolean 
little_skin_has_segment (LittleSkin *skin, const char *segment) {
	if (g_hash_table_lookup (skin->priv->table, segment)) return TRUE;
	return FALSE;
}

GList* 
little_skin_get_segment_names (LittleSkin *skin) {
	GList *segments = NULL;
	GList *ptr = skin->priv->segments;
	while (ptr) {
		debug ("little_skin_get_segment_names : adding %s", LITTLE_SKIN_SEGMENT (ptr->data)->name);
		segments = g_list_append (segments, LITTLE_SKIN_SEGMENT (ptr->data)->name);
		ptr = ptr->next;
	}
	return segments;
}

const GList* 
little_skin_get_theme_names (LittleSkin *skin) {
	return skin->themes;
}

int little_skin_get_segment_range (LittleSkin *skin, const char *segment) {
	GList *ptr = skin->priv->segments;
	while (ptr) {
		if (g_strcasecmp (LITTLE_SKIN_SEGMENT (ptr->data)->name, segment) == 0) {
			debug ("little_skin_get_segment_range : %d", LITTLE_SKIN_SEGMENT (ptr->data)->range);
			return LITTLE_SKIN_SEGMENT (ptr->data)->range;
		}
		ptr = ptr->next;
	}
	debug ("little_skin_get_segment_range : -1");
	return -1;
}
gboolean 
little_skin_is_segment_animation (LittleSkin *skin, const char *segment) {
	GList *ptr = skin->priv->segments;
	while (ptr) {
		if (g_strcasecmp (LITTLE_SKIN_SEGMENT (ptr->data)->name, segment) == 0 &&
		    LITTLE_SKIN_SEGMENT (ptr->data)->animation) 
			return TRUE;
		ptr = ptr->next;
	}
	return FALSE;
}

gboolean 
little_skin_is_animating (LittleSkin *skin) {
  return (skin->priv->anim_id != 0);
}

void 
little_skin_set_alpha (LittleSkin *skin, gboolean alpha) {
  skin->priv->alpha = alpha;
  if (!alpha) {
    if (skin->priv->alpha_pixbuf) {
      g_object_unref (skin->priv->alpha_pixbuf);
      skin->priv->alpha_pixbuf = NULL;
    }
  }
  //fuck_off_and_die (skin->priv->alpha ? "ALPHA" : "NO ALPHA", NULL);
}

void 
little_skin_set_alpha_level (LittleSkin *skin, int alpha_level) {
  skin->priv->alpha_level = alpha_level;
}

void 
little_skin_set_alpha_pixbuf (LittleSkin *skin, GdkPixbuf *pixbuf) {
  if (skin->priv->alpha_pixbuf) {
    g_object_unref (skin->priv->alpha_pixbuf);
    skin->priv->alpha_pixbuf = NULL;
  }
  if (pixbuf) {
    g_object_ref (G_OBJECT (pixbuf));
    skin->priv->alpha_pixbuf = pixbuf;
  }
}

GdkPixbuf*
little_skin_get_alpha_pixbuf (LittleSkin *skin) {
  return skin->priv->alpha_pixbuf;
}

void 
little_skin_set_alpha_color (LittleSkin *skin, GdkColor *color) {
  if (skin->priv->alpha_pixbuf) {
    g_object_unref (skin->priv->alpha_pixbuf);
    skin->priv->alpha_pixbuf = NULL;
  }
  if (skin->priv->alpha_color) gdk_color_free (skin->priv->alpha_color);
  skin->priv->alpha_color = gdk_color_copy (color);
}
