#ifndef _EMU_EMU_8086_APP_SEARCH_BAR_C
#define _EMU_EMU_8086_APP_SEARCH_BAR_C

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EMU_8086_APP_SEARCH_BAR_TYPE (emu_8086_app_search_bar_get_type())

#define EMU_8086_APP_SEARCH_BAR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), EMU_8086_APP_SEARCH_BAR_TYPE, Emu8086AppSearchBar))
#define EMU_8086_APP_SEARCH_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), EMU_8086_APP_SEARCH_BAR_TYPE, Emu8086AppSearchBarClass))
#define EMU_8086_IS_APP_EMU_8086_APP_SEARCH_BAR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), EMU_8086_APP_SEARCH_BAR_TYPE))
#define EMU_8086_IS_APP_EMU_8086_APP_SEARCH_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EMU_8086_APP_SEARCH_BAR_TYPE))
#define EMU_8086_APP_SEARCH_BAR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EMU_8086_APP_SEARCH_BAR_TYPE, Emu8086AppSearchBarClass))
#define PRIV_EMU_8086_APP_SEARCH_BAR Emu8086AppSearchBarPrivate *priv = search_bar->priv

typedef struct _Emu8086AppSearchBar Emu8086AppSearchBar;
typedef struct _Emu8086AppSearchBarClass Emu8086AppSearchBarClass;
typedef struct _Emu8086AppSearchBarPrivate Emu8086AppSearchBarPrivate;

struct _Emu8086AppSearchBar
{
    GtkGrid parent;

    Emu8086AppSearchBarPrivate *priv;
};

struct _Emu8086AppSearchBarClass
{
    GtkGridClass parent;
    void (*search_forward)(Emu8086AppSearchBar *search_bar);
    void (*search_backward)(Emu8086AppSearchBar *search_bar);
};

GType emu_8086_app_search_bar_get_type(void) G_GNUC_CONST;
Emu8086AppSearchBar *emu_8086_app_search_bar_create(GtkTextView *view);
gboolean find(Emu8086AppSearchBar *search_bar, const gchar *text);
void emu_8086_app_search_bar_clear_out(Emu8086AppSearchBar *search_bar);
G_END_DECLS

#endif