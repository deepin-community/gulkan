#include "common.h"

GdkPixbuf *
gdk_load_pixbuf_from_uri (const gchar *uri)
{
  GError    *error = NULL;
  GdkPixbuf *pixbuf_no_alpha = gdk_pixbuf_new_from_resource (uri, &error);

  if (error != NULL)
    {
      g_printerr ("Unable to read file: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }
  else
    {
      GdkPixbuf *pixbuf = gdk_pixbuf_add_alpha (pixbuf_no_alpha, FALSE, 0, 0,
                                                0);
      g_object_unref (pixbuf_no_alpha);
      return pixbuf;
    }
}
