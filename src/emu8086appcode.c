/* gtkemu8086
 * Copyright 2020 KRC
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * code.c
 * Code class
 */
#include <emu8086appwin.h>
#include <emu8086appcode.h>
#include <emu8086appcodebuffer.h>
#include <pango_css.h>
#include <emu8086appcodegutter.h>
#include <emu8086stylescheme.h>

enum _Emu8086AppCodeSignals
{

    CODE_UNDO,
    CODE_REDO,
    MOVE_LINES,
    CODE_N_SIGNALS
};

static guint code_signals[CODE_N_SIGNALS] = {0};

enum _Emu8086AppCodeProperty
{
    PROP_0,
    PROP_FONT,
    PROP_CODE_THEME,
    PROP_AUTO_INDENT,
    PROP_HIGHLIGHT_LINE,
    PROP_CODE_TYPE
};

typedef enum _Emu8086AppCodeProperty Emu8086AppCodeProperty;

struct _Emu8086AppCodePrivate
{

    Emu8086AppStyleScheme *scheme;
    Emu8086AppWindow *win;
    gchar *hl;
    GtkStyleProvider *provider;
    Emu8086AppCodeBuffer *buffer;
    gint line;
    GSettings *settings;
    gint fontsize;
    GtkAccelGroup *ag;
    GtkTextMark *mark;
    Emu8086AppCodeGutter *gutter;
    int break_points[100];
    int break_points_len;
    gboolean auto_indent;
    gboolean highlight_line;
    gboolean show_line_numbers;
    Emu8086AppCodeType m_type;
};

G_DEFINE_TYPE_WITH_PRIVATE(Emu8086AppCode, emu8086_app_code, GTK_TYPE_TEXT_VIEW);

static void emu8086_app_code_remove_all_break_points(Emu8086AppCode *code);
static Emu8086AppCode *emu8086_app_code_new(Emu8086AppCodeType type);

static void menu_item_activate_indent_cb(GSimpleAction *action,
                                         GVariant *parameter,
                                         gpointer appe);
static void user_function(GtkTextBuffer *textbuffer,
                          GtkTextIter *location,
                          gchar *text,
                          gint len,
                          gpointer user_data);

static void user_function2(GtkTextBuffer *textbuffer,
                           GtkTextIter *start,
                           GtkTextIter *end,
                           gpointer user_data);
static void menu_item_activate_rem_bps_cb(GSimpleAction *action,
                                          GVariant *parameter,
                                          gpointer user_data);

static GActionEntry code_entries[] = {

    {"format", menu_item_activate_indent_cb, NULL, NULL, NULL},
    {"remove_bps", menu_item_activate_rem_bps_cb, NULL, NULL, NULL}

};

static void
menu_item_activate_indent_cb(GSimpleAction *action,
                             GVariant *parameter,
                             gpointer appe)
{
    GtkTextView *text_view;
    text_view = GTK_TEXT_VIEW(appe);
    GtkTextBuffer *buffer;
    buffer = gtk_text_view_get_buffer(text_view);
    if (!EMU8086_IS_APP_CODE_BUFFER(buffer))
    {
        return;
    }
    emu8086_app_code_buffer_indent(EMU8086_APP_CODE_BUFFER(buffer));
}

static void
menu_item_activate_rem_bps_cb(GSimpleAction *action,
                              GVariant *parameter,
                              gpointer user_data)
{
    Emu8086AppCode *code;
    code = EMU8086_APP_CODE(user_data);
    emu8086_app_code_remove_all_break_points(code);
}

static void emu8086_app_code_popup_menu(GtkTextView *text_view,
                                        GtkWidget *popup)
{
    Emu8086AppCode *code = EMU8086_APP_CODE(text_view);
    Emu8086AppCodeBuffer *buffer;
    GtkMenuShell *menu;
    GtkWidget *menu_item;
    buffer = code->priv->buffer;

    if (!EMU8086_IS_APP_CODE_BUFFER(buffer) || code->priv->m_type == MEM_VIEW)
    {
        return;
    }

    if (!GTK_IS_MENU_SHELL(popup))
    {
        return;
    }
    PRIV_CODE;
    menu = GTK_MENU_SHELL(popup);
    gtk_menu_set_accel_group(GTK_MENU(menu), priv->ag);
    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(menu, menu_item);
    gtk_widget_show(menu_item);

    menu_item = gtk_menu_item_new_with_mnemonic(("_Format Document"));
    gtk_menu_shell_append(menu, menu_item);
    gtk_menu_item_set_accel_path(GTK_MENU_ITEM(menu_item), "<Code-Widget>/Format");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(menu_item), "code.format");

    gtk_widget_show(menu_item);

    menu_item = gtk_menu_item_new_with_mnemonic(("_Remove All Breakpoints"));
    gtk_menu_shell_append(menu, menu_item);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(menu_item), "code.remove_bps");

    gtk_widget_show(menu_item);
    return;
};

static gchar *emu8086_app_code_change_color(Emu8086AppCode *code)
{

    PRIV_CODE;
    gchar *v = emu8086_app_style_scheme_get_color_by_index(priv->scheme, COLOR_TEXT);

    PangoFontDescription *desc = pango_font_description_from_string(g_strdup(code->font));

    gchar *m = emu8086_pango_font_description_to_css(desc, v);
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(priv->provider), m, -1, NULL);

    code->color = v;

    pango_font_description_free(desc);
    g_free(m);
}

static void
emu8086_app_code_set_property(GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    Emu8086AppCode *code = EMU8086_APP_CODE(object);
    PRIV_CODE;
    PangoFontDescription *desc;

    gchar *m;

    gchar *v;
    switch ((Emu8086AppCodeProperty)property_id)
    {
    case PROP_FONT:

        v = g_strdup(g_value_get_string(value));
        desc = pango_font_description_from_string(v);

        code->font = v;

        pango_font_description_free(desc);
        emu8086_app_code_change_color(code);

        break;

    case PROP_AUTO_INDENT:
        code->priv->auto_indent = g_value_get_boolean(value);
        break;

    case PROP_HIGHLIGHT_LINE:
        code->priv->highlight_line = g_value_get_boolean(value);
        break;

    case PROP_CODE_TYPE:
        code->priv->m_type = g_value_get_int(value);
        break;

    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}
static void
emu8086_app_code_get_property(GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    Emu8086AppCode *self = EMU8086_APP_CODE(object);
    switch ((Emu8086AppCodeProperty)property_id)
    {
    case PROP_FONT:
        g_value_set_string(value, self->font);
        break;

    case PROP_CODE_THEME:
        g_value_set_string(value, self->color);
        break;
    case PROP_AUTO_INDENT:
        g_value_set_boolean(value, self->priv->auto_indent);
        break;
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
emu8086_app_code_drag_data_received(GtkWidget *widget,
                                    GdkDragContext *context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *selection_data,
                                    guint info,
                                    guint timestamp)
{
    Emu8086AppCode *code;
    code = EMU8086_APP_CODE(widget);
    g_return_if_fail(EMU8086_IS_APP_CODE(code));

    PRIV_CODE;
    // ;
    emu8086_app_window_open_drag_data(priv->win, selection_data);
    gtk_drag_finish(context, TRUE, FALSE, timestamp);
}
static gboolean
emu8086_app_code_draw(GtkWidget *widget,
                      cairo_t *cr)
{
    Emu8086AppCode *code = EMU8086_APP_CODE(widget);
    PRIV_CODE;
    gboolean event_handled;
    event_handled = GTK_WIDGET_CLASS(emu8086_app_code_parent_class)->draw(widget, cr);

    if (priv->gutter != NULL && priv->show_line_numbers)
    {
        draw(priv->gutter, cr);
    }
    return event_handled;
}

static void emu8086_app_code_remove_all_break_points(Emu8086AppCode *code)
{
    PRIV_CODE;
    gint len = priv->break_points_len;
    for (gint i = 0; i < len; i++)
    {
        priv->break_points[i] = -1;
    }
}

gboolean check_for_break_points(Emu8086AppCode *code, gint line_num,
                                gboolean toggle)
{
    PRIV_CODE;
    gint len = priv->break_points_len;

    for (gint i = 0; i < len; i++)
    {
        int *l = priv->break_points + i;
        if (line_num == *l)
        {
            if (toggle)
                *l = -1;
            return TRUE;
        }
    }
    return FALSE;
};

static gboolean emu8086_app_code_button_press_event(GtkWidget *widget, GdkEventButton *event)
{
    Emu8086AppCode *code;
    code = EMU8086_APP_CODE(widget);
    PRIV_CODE;
    GdkWindow *window = event->window;
    GtkTextView *view = GTK_TEXT_VIEW(code);

    if (GTK_TEXT_WINDOW_LEFT == gtk_text_view_get_window_type(GTK_TEXT_VIEW(code), window) &&
        event->type == GDK_2BUTTON_PRESS)
    {
        gint first_y_buffer_coord;
        gint last_y_buffer_coord;
        GtkTextBuffer *buffer = GTK_TEXT_BUFFER(priv->buffer);
        GtkTextIter iter;
        gint line_num;
        gtk_text_view_window_to_buffer_coords(view,
                                              GTK_TEXT_WINDOW_LEFT, 0,
                                              event->y,
                                              NULL,
                                              &first_y_buffer_coord);
        gtk_text_view_get_line_at_y(view, &iter, first_y_buffer_coord, NULL);
        line_num = gtk_text_iter_get_line(&iter);

        // We only want to allow 100 breakpoints
        if (priv->break_points_len >= 99)
            return GTK_WIDGET_CLASS(emu8086_app_code_parent_class)->button_press_event(widget, event);

        if (!check_for_break_points(code, line_num, TRUE))

        {
            gint len = priv->break_points_len;
            gboolean has_set_bp = FALSE;

            for (gint i = 0; i < len; i++)
            {
                int *l = priv->break_points + i;
                if (*l == -1)
                {
                    *l = line_num;
                    has_set_bp = TRUE;
                }
            }

            if (!has_set_bp)
                priv->break_points[priv->break_points_len++] = line_num;
        }
    }

    return GTK_WIDGET_CLASS(emu8086_app_code_parent_class)->button_press_event(widget, event);
}

// Check if previous line is empty
static gboolean check_prev_line(GtkTextBuffer *buffer, gint line)
{
    if (line == 0)
        return FALSE;

    GtkTextIter iter;

    gtk_text_buffer_get_iter_at_line(buffer, &iter, line);
    if (gtk_text_iter_ends_line(&iter))
    {
        return TRUE;
    }
    while (gtk_text_iter_get_char(&iter) == ' ')
    {
        gtk_text_iter_forward_char(&iter);
    }
    if (gtk_text_iter_ends_line(&iter))
    {
        return TRUE;
    }
    while (gtk_text_iter_get_char(&iter) == '\t')
        gtk_text_iter_forward_char(&iter);
    while (gtk_text_iter_get_char(&iter) == ' ')
    {
        gtk_text_iter_forward_char(&iter);
    }
    if (gtk_text_iter_ends_line(&iter))
    {
        return TRUE;
    }
    return FALSE;
}

static gboolean
emu8086_app_code_key_press_event(GtkWidget *widget,
                                 GdkEventKey *event)
{
    Emu8086AppCode *code;
    GtkTextBuffer *buf;
    GtkTextIter cur;
    GtkTextMark *mark;
    guint modifiers;
    gint key;
    gboolean editable;
    gint line;
    code = EMU8086_APP_CODE(widget);
    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
    editable = gtk_text_view_get_editable(GTK_TEXT_VIEW(widget));
    key = event->keyval;
    mark = gtk_text_buffer_get_insert(buf);
    gtk_text_buffer_get_iter_at_mark(buf, &cur, mark);
    if (!code->priv->auto_indent)
        return GTK_WIDGET_CLASS(emu8086_app_code_parent_class)->key_press_event(widget, event);

    if ((key == GDK_KEY_Return || key == GDK_KEY_KP_Enter))
    {
        if (gtk_text_view_im_context_filter_keypress(GTK_TEXT_VIEW(widget), event))
        {
            return GDK_EVENT_STOP;
        }
        gtk_text_buffer_delete_selection(buf,
                                         TRUE,
                                         gtk_text_view_get_editable(GTK_TEXT_VIEW(widget)));
        gtk_text_buffer_get_iter_at_mark(buf, &cur, mark);
        line = gtk_text_iter_get_line(&cur);
        /* Insert new line and auto-indent. */
        gtk_text_buffer_begin_user_action(buf);
        gtk_text_buffer_insert(buf, &cur, "\n", 1);
        if (!check_prev_line(buf, line))
            gtk_text_buffer_insert(buf, &cur, "\t", 1);
        gtk_text_buffer_end_user_action(buf);
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(widget),
                                           mark);
        return GDK_EVENT_STOP;
    }
    return GTK_WIDGET_CLASS(emu8086_app_code_parent_class)->key_press_event(widget, event);
}

static void emu8086_app_code_paint_line_background(GtkTextView *text_view,
                                                   cairo_t *cr,
                                                   int y, /* in buffer coordinates */
                                                   int height,
                                                   const GdkRGBA *color)
{
    gdouble x1, y1, x2, y2;

    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);

    gdk_cairo_set_source_rgba(cr, (GdkRGBA *)color);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, x1 + .5, y + .5, x2 - x1 - 1, height - 1);

    cairo_stroke_preserve(cr);
    cairo_fill(cr);
    // cairo_stroke(cr)
}

static void emu8086_app_code_paint_current_line_highlight(GtkTextView *view,

                                                          cairo_t *cr)
{
    GtkTextBuffer *buffer;
    GtkTextIter cur;
    gint y;
    gint height;
    GdkRGBA color;

    gdk_rgba_parse(&color, EMU8086_APP_CODE(view)->priv->hl);

    buffer = gtk_text_view_get_buffer(view);
    gtk_text_buffer_get_iter_at_mark(buffer,
                                     &cur,
                                     gtk_text_buffer_get_insert(buffer));
    gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(view), &cur, &y, &height);
    emu8086_app_code_paint_line_background(view,
                                           cr,
                                           y, height,
                                           &color);
}

static void emu8086_app_code_draw_layer(GtkTextView *text_view,
                                        GtkTextViewLayer layer,
                                        cairo_t *cr)
{
    Emu8086AppCode *code;
    code = EMU8086_APP_CODE(text_view);
    g_return_if_fail(EMU8086_IS_APP_CODE(code));
    if (!code->priv->highlight_line)
        return;
    cairo_save(cr);
    if (gtk_widget_is_sensitive(GTK_WIDGET(text_view)) && layer == GTK_TEXT_VIEW_LAYER_BELOW_TEXT)
    {
        emu8086_app_code_paint_current_line_highlight(text_view, cr);
    }
    cairo_restore(cr);
}

void emu8086_app_code_undo(Emu8086AppCode *code)
{
    emu8086_app_code_buffer_undo(code->priv->buffer);
}
void emu8086_app_code_redo(Emu8086AppCode *code)
{
    emu8086_app_code_buffer_redo(code->priv->buffer);
}
static void emu8086_app_code_class_init(Emu8086AppCodeClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GtkTextViewClass *textview_class;
    GtkBindingSet *binding_set;
    textview_class = GTK_TEXT_VIEW_CLASS(klass);
    object_class->set_property = emu8086_app_code_set_property;
    object_class->get_property = emu8086_app_code_get_property;

    widget_class->draw = emu8086_app_code_draw;
    widget_class->drag_data_received = emu8086_app_code_drag_data_received;
    widget_class->button_press_event = emu8086_app_code_button_press_event;
    widget_class->key_press_event = emu8086_app_code_key_press_event;
    textview_class->populate_popup = emu8086_app_code_popup_menu;
    textview_class->draw_layer = emu8086_app_code_draw_layer;

    klass->undo = emu8086_app_code_undo;
    klass->redo = emu8086_app_code_redo;

    g_object_class_install_property(object_class, PROP_FONT,
                                    g_param_spec_string("font", "Font", "Editor Font", "Monospace Regular 16",
                                                        G_PARAM_READWRITE));
    g_object_class_install_property(object_class, PROP_CODE_THEME,
                                    g_param_spec_string("color", "Color", "Editor Color", "#C4C4C4",
                                                        G_PARAM_READWRITE));
    g_object_class_install_property(object_class, PROP_AUTO_INDENT,
                                    g_param_spec_boolean("auto_indent", "AutoIndent", "Editor auto indent", FALSE,
                                                         G_PARAM_READWRITE));
    g_object_class_install_property(object_class, PROP_HIGHLIGHT_LINE,
                                    g_param_spec_boolean("highlight_line", "HighlightLine", "Editor highlight line", TRUE,
                                                         G_PARAM_READWRITE));
    g_object_class_install_property(object_class, PROP_CODE_TYPE,
                                    g_param_spec_int("code_type", "CodeType", "The text view type", 0, 1, CODE_VIEW, G_PARAM_READWRITE));
    code_signals[CODE_UNDO] =
        g_signal_new("undo",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                     G_STRUCT_OFFSET(Emu8086AppCodeClass, undo),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
    g_signal_set_va_marshaller(code_signals[CODE_UNDO],
                               G_TYPE_FROM_CLASS(klass),
                               g_cclosure_marshal_VOID__VOIDv);
    code_signals[CODE_REDO] =
        g_signal_new("redo",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                     G_STRUCT_OFFSET(Emu8086AppCodeClass, redo),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
    g_signal_set_va_marshaller(code_signals[CODE_REDO],
                               G_TYPE_FROM_CLASS(klass),
                               g_cclosure_marshal_VOID__VOIDv);

    code_signals[MOVE_LINES] =
        g_signal_new("move-lines",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                     G_STRUCT_OFFSET(Emu8086AppCodeClass, move_lines),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__BOOLEAN,
                     G_TYPE_NONE, 1,
                     G_TYPE_BOOLEAN);
    g_signal_set_va_marshaller(code_signals[MOVE_LINES],
                               G_TYPE_FROM_CLASS(klass),
                               g_cclosure_marshal_VOID__BOOLEANv);
    binding_set = gtk_binding_set_by_class(klass);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_KEY_z,
                                 GDK_CONTROL_MASK,
                                 "undo", 0);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_KEY_z,
                                 GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                 "redo", 0);
}

static void _refreshLines(GtkTextView *text_view,
                          gpointer user_data)

{

    Emu8086AppCodeBuffer *b;
    b = EMU8086_APP_CODE_BUFFER(gtk_text_view_get_buffer(text_view));
    refreshLines(b);
}
static void _refresh_theme(Emu8086AppStyleScheme *scheme,
                           gpointer user_data)

{

    Emu8086AppCode *code;
    code = EMU8086_APP_CODE(user_data);
    gchar *hl;
    hl = emu8086_app_style_scheme_get_color_by_index(scheme, 7);
    code->priv->hl = hl;
    emu8086_app_code_change_color(code);
    emu8086_app_code_buffer_change_theme(code->priv->buffer);
    // exit(1);
}

static void emu8086_app_code_init(Emu8086AppCode *code)
{
    code->priv = emu8086_app_code_get_instance_private(code);
    PRIV_CODE;

    priv->settings = g_settings_new("com.krc.emu8086app");
    priv->provider = GTK_STYLE_PROVIDER(gtk_css_provider_new());
    priv->mark = NULL;
    priv->gutter = NULL;
    priv->show_line_numbers = TRUE;
    priv->scheme = emu8086_app_style_scheme_get_default();
    priv->hl = emu8086_app_style_scheme_get_color_by_index(priv->scheme, 7);
    gtk_accel_map_add_entry("<Code-Widget>/Format", GDK_KEY_I, GDK_CONTROL_MASK);
    GSimpleActionGroup *ca = g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(ca),
                                    code_entries, G_N_ELEMENTS(code_entries),
                                    code);

    gtk_widget_insert_action_group(GTK_WIDGET(code), "code", G_ACTION_GROUP(ca));
    gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(code),
                                         GTK_TEXT_WINDOW_LEFT,
                                         20);
    gtk_style_context_add_provider(gtk_widget_get_style_context(GTK_WIDGET(code)), priv->provider, G_MAXUINT);

    g_settings_bind(priv->settings, "ai", code, "auto_indent", G_SETTINGS_BIND_GET);
    g_settings_bind(priv->settings, "highlight", code, "highlight_line", G_SETTINGS_BIND_GET);
    g_signal_connect(GTK_TEXT_VIEW(code), "paste-clipboard", G_CALLBACK(_refreshLines), NULL);

    priv->break_points_len = 0;

    Emu8086AppCodeBuffer *buffer = emu8086_app_code_buffer_new(NULL);

    gtk_text_view_set_buffer(GTK_TEXT_VIEW(code), GTK_TEXT_BUFFER(buffer));

    Emu8086AppCodeGutter *gutter;
    gutter = emu8086_app_code_gutter_new(code, GTK_TEXT_WINDOW_LEFT);

    // setCode(buffer, code);
    priv->buffer = buffer;
    priv->gutter = gutter;

    g_signal_connect(GTK_TEXT_BUFFER(buffer), "insert-text", G_CALLBACK(user_function), code);
    g_signal_connect(GTK_TEXT_BUFFER(buffer), "delete-range", G_CALLBACK(user_function2), code);
    g_signal_connect(priv->scheme, "theme_changed", G_CALLBACK(_refresh_theme), code);
}

void select_line(GtkWidget *co, gint line)
{
    Emu8086AppCode *code = EMU8086_APP_CODE(co);
    GtkTextMark *mark;
    gboolean ret = TRUE;
    GtkTextIter iter, start;

    PRIV_CODE;
    Emu8086AppCodeBuffer *buffer = priv->buffer;
    if (priv->mark == NULL)
    {
        gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(buffer), &iter, line);

        start = iter;
        gtk_text_iter_forward_to_line_end(&iter);

        priv->mark = gtk_text_buffer_create_mark(GTK_TEXT_BUFFER(buffer),
                                                 "sel_line",
                                                 &iter,
                                                 FALSE);
        priv->line = line;

        gtk_text_buffer_apply_tag_by_name(GTK_TEXT_BUFFER(buffer), "step", &start, &iter);
    }
    else if (GTK_IS_TEXT_MARK(priv->mark))
    {
        gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(buffer), &iter, priv->line);
        start = iter;
        gtk_text_iter_forward_to_line_end(&iter);
        gtk_text_buffer_remove_tag_by_name(GTK_TEXT_BUFFER(buffer), "step", &start, &iter);
        gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(buffer), &iter, line);

        start = iter;
        gtk_text_iter_forward_to_line_end(&iter);

        priv->line = line;

        gtk_text_buffer_apply_tag_by_name(GTK_TEXT_BUFFER(buffer), "step", &start, &iter);
    }

    gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(buffer), &iter);

    emu8086_app_code_scroll_to_view(code);
    //
}

void reset_code(GtkWidget *co)
{
    Emu8086AppCode *code = EMU8086_APP_CODE(co);

    PRIV_CODE;
    priv->mark = NULL;
    if (priv->line > -1)
    {
        GtkTextIter iter, start;
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(code));
        gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(buffer),
                                         &iter,
                                         priv->line);
        g_return_if_fail(gtk_text_iter_get_buffer(&iter) == buffer);

        gtk_text_iter_forward_to_line_end(&iter);
        gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(buffer),
                                         &start,
                                         priv->line);
        gtk_text_buffer_remove_tag_by_name(buffer, "step", &start, &iter);
    }
}

void emu8086_app_code_scroll_to_view(Emu8086AppCode *code)
{
    GtkTextView *view;
    GtkTextIter mstart;
    GtkTextMark *cursor;
    GtkTextBuffer *buffer;
    GdkRectangle visible, location;
    g_return_if_fail(EMU8086_IS_APP_CODE(code));
    PRIV_CODE;
    view = GTK_TEXT_VIEW(code);
    buffer = GTK_TEXT_BUFFER(priv->buffer);
    cursor = gtk_text_buffer_get_insert(buffer);
    gtk_text_view_scroll_mark_onscreen(view, cursor);

    gtk_text_buffer_get_iter_at_mark(buffer, &mstart, cursor);
    gtk_text_view_get_visible_rect(view, &visible);

    gtk_text_view_get_iter_location(view, &mstart, &location);
    if (location.y < visible.y || visible.y + visible.height < location.y)
    {
        gtk_text_view_scroll_to_mark(view,
                                     cursor,
                                     0.0,
                                     TRUE,
                                     0.5, 0.5);
    }
}

void emu8086_app_code_scroll_to_mark(Emu8086AppCode *code, GtkTextMark *mark)
{
    GtkTextView *view;
    GtkTextIter mstart, mend;
    g_return_if_fail(EMU8086_IS_APP_CODE(code));
    PRIV_CODE;
    view = GTK_TEXT_VIEW(code);
    GdkRectangle visible, location;
    gtk_text_view_scroll_mark_onscreen(view, mark);
    gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(priv->buffer), &mstart, mark);
    gtk_text_view_get_visible_rect(view, &visible);

    gtk_text_view_get_iter_location(view, &mstart, &location);
    if (location.y < visible.y || visible.y + visible.height < location.y)
    {
        gtk_text_view_scroll_to_mark(view,
                                     mark,
                                     0.0,
                                     TRUE,
                                     0.5, 0.5);
    }
}

static void user_function(GtkTextBuffer *textbuffer,
                          GtkTextIter *location,
                          gchar *text,
                          gint len,
                          gpointer user_data)
{

    Emu8086AppCode *code = EMU8086_APP_CODE(user_data);
    PRIV_CODE;
    emu8086_app_code_scroll_to_view(code);
}

static void user_function2(GtkTextBuffer *textbuffer,
                           GtkTextIter *start,
                           GtkTextIter *end,
                           gpointer user_data)
{

    Emu8086AppCode *code = EMU8086_APP_CODE(user_data);
    PRIV_CODE;

    emu8086_app_code_scroll_to_view(code);
}

void editFontSize(Emu8086AppCode *code, gint size)
{
    PRIV_CODE;
    gchar *font = code->font, *tempsize;
    gint nsize;
    // font += strlen(font);
    // Copy the current editor font e.g monospace 14
    tempsize = g_strdup(font);

    // monospace 14 -> 41 ecapsonom
    g_strreverse(tempsize);

    // Remove all the letters in the  string
    // 41 ecapsonom -> 41
    g_strcanon(tempsize, "1234567890", '\0');

    // 41 -> 14
    g_strreverse(tempsize);

    gchar tempfont[strlen(font)];

    strcpy(tempfont, font);
    // Copy only the font letters
    tempfont[strlen(font) - strlen(tempsize)] = '\0';

    sscanf(tempsize, "%d", &priv->fontsize);

    gchar new[strlen(tempsize) + 2];
    priv->fontsize += size;
    sprintf(new, "%d", priv->fontsize);

    gchar *tmp = strcat(tempfont, new);
    code->font = g_strdup(tmp);
    g_settings_set_string(priv->settings, "font", tmp);
    PangoFontDescription *desc;
    desc = pango_font_description_from_string(tmp);
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(priv->provider), (emu8086_pango_font_description_to_css(desc, code->color)), -1, NULL);

    pango_font_description_free(desc);
    g_free(tempsize);
    g_free(font);

    return;
}

Emu8086AppCode *create_new(Emu8086AppCodeType type)
{
    Emu8086AppCode *code;
    GtkStyleProvider *provider;
    code = emu8086_app_code_new(type);
    GdkRGBA _color;
    _color.alpha = 1.0;
    _color.blue = 0.20;
    _color.red = 0.20;
    _color.green = 0.20;
    PRIV_CODE;
    provider = priv->provider;
    GdkRGBA color;

    color.red = 0.22;
    color.green = 0.22;
    color.blue = 0.22;
    color.alpha = 1;
    gtk_widget_override_background_color(GTK_WIDGET(code), GTK_STATE_NORMAL, &color);

    priv->line = 0;

    g_settings_bind(priv->settings, "font", code, "font", G_SETTINGS_BIND_GET);

    return code;
}

static Emu8086AppCode *emu8086_app_code_new(Emu8086AppCodeType type)
{
    return g_object_new(EMU8086_APP_CODE_TYPE, "code_type", type, NULL);
}

void get_break_points(Emu8086AppCode *code, gint *bps, gint *len)
{
    PRIV_CODE;
    *len = priv->break_points_len;

    bps = priv->break_points;
}

void emu8086_app_code_set_show_lines(Emu8086AppCode *code, gboolean show)
{
    g_return_if_fail(EMU8086_IS_APP_CODE(code));
    code->priv->show_line_numbers = show;
}

Emu8086AppCodeBuffer *emu8086_app_code_get_mbuffer(Emu8086AppCode *code)
{
    g_return_val_if_fail(EMU8086_IS_APP_CODE(code), NULL);

    return EMU8086_APP_CODE_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(code)));
}

gboolean emu8086_app_code_mgo_to_line(Emu8086AppCode *code, gint line)
{
    g_return_val_if_fail(EMU8086_IS_APP_CODE(code), FALSE);
    g_return_val_if_fail(EMU8086_IS_APP_CODE_BUFFER(code->priv->buffer), FALSE);
    g_return_val_if_fail(line > 0, FALSE);
    GtkTextIter iter;
    GtkTextBuffer *buffer;
    GtkTextView *tv;
    tv = GTK_TEXT_VIEW(code);
    buffer = GTK_TEXT_BUFFER(code->priv->buffer);
    gtk_text_buffer_get_iter_at_line(buffer, &iter, line - 1);
    gtk_text_iter_forward_to_line_end(&iter);
    gtk_text_buffer_place_cursor(buffer, &iter);
    emu8086_app_code_scroll_to_view(code);
};