#include <stdlib.h>
#include <gtk/gtk.h>
#include "littleskin.h"
#include <assert.h>

GtkWidget*
test_window (const char *title, LittleSkin *skin) {
	GtkWidget *window;
	GtkWidget *hbox;

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), 
			    little_skin_get_image_widget (skin), 
			    FALSE, FALSE, 0);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	if (title) gtk_window_set_title (GTK_WINDOW (window), title);

	{
	  GtkWidget *image2;
	  image2 = gtk_image_new_from_file ("/usr/share/pixmaps/fish/fishanim.png");
	  little_skin_set_alpha (skin, TRUE);
	  little_skin_set_alpha_level (skin, 192);
	  little_skin_set_alpha_pixbuf (skin, gtk_image_get_pixbuf (GTK_IMAGE (image2)));
	}

	gtk_container_add (GTK_CONTAINER (window), hbox);

	return window;
}


gboolean
test_timeout (GtkWidget *window) {
	static int segment_num = 0;
	static int cnt = 0;
	GList *segments;
	const char *segment_name;

	LittleSkin *skin = gtk_object_get_data (GTK_OBJECT (window), "skin");

	segments = little_skin_get_segment_names (skin);
	segment_name = (char*)(g_list_nth (segments, segment_num)->data);
	if (little_skin_is_segment_animation (skin, segment_name)) {
		g_message ("%s is an anim", segment_name);
		if (cnt == 0) {
			little_skin_start_animation (skin, segment_name, 200);
		}
		cnt ++;
		if (cnt > 100) {
			little_skin_stop_animation (skin);
			segment_num++;
			if (segment_num >= g_list_length (segments)) {
				segment_num = 0;
			}			
		}
	} else {
		little_skin_set_image (skin, segment_name, cnt);
		little_skin_redraw (skin);
		cnt++;
		if (cnt >= little_skin_get_segment_range (skin, segment_name)) {
			cnt = 0;
			segment_num++;
			if (segment_num >= g_list_length (segments)) {
				segment_num = 0;
			}
		}
	}

	return TRUE;
}

void
test_delete_event (GtkWidget *widget,
		   GdkEvent *event,
		   gpointer crack) {
	guint id = GPOINTER_TO_INT (crack);
	gtk_timeout_remove (id);
	gtk_main_quit ();
}
		  
int
main (int argc, char *argv[]) {
	GtkWidget *window;
	GtkWidget *image;
	GList *loads = NULL;
	LittleSkin *skin;

	gtk_init (&argc, &argv);

	{
		LittleSkinLoad *load;
		load = little_skin_load_new ("charging", 101);
		loads = g_list_append (loads, load);
		load = little_skin_load_new ("battery", 101);
		loads = g_list_append (loads, load);
		load = little_skin_load_new ("low", 0);
		loads = g_list_append (loads, load);
	}
	skin = little_skin_new (argv[1], argv[2], loads);

	little_skin_set_alpha (skin, TRUE);

	window = test_window ("littleskin", skin);
	gtk_object_set_data (GTK_OBJECT (window), "skin", skin);
	g_signal_connect (window, "delete_event", 
			  G_CALLBACK (test_delete_event), 
			  GINT_TO_POINTER (gtk_timeout_add (10, (GtkFunction)test_timeout, window)));
	gtk_widget_show_all (window);
	gtk_main ();

	little_skin_destroy (skin);
	g_list_foreach (loads, (GFunc)little_skin_load_destroy, NULL);
	g_list_free (loads);
}
