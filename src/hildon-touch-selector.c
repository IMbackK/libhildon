/*
 * This file is a part of hildon
 *
 * Copyright (C) 2005, 2008 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version. or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * SECTION:hildon-touch-selector
 * @short_description: A selector widget with several columns.
 *
 * #HildonTouchSelector is a selector widget, that allows users to
 * select items from one to many predefined lists. It is very similar
 * to #GtkComboBox, but with several individual pannable columns.
 *
 * Normally, you would use #HildonTouchSelector together with a
 * #HildonPickerDialog activated from a button. For the most common
 * cases, you should use #HildonPickerButton.
 *
 * The composition of each column in the selector is represented by a
 * #GtkTreeModel. To add a new column to a #HildonTouchSelector, use
 * hildon_touch_selector_append_column(). If you want to add a
 * text-only column, without special attributes, use
 * hildon_touch_selector_append_text_column().
 *
 * It is highly recommended that you use only one column
 * #HildonTouchSelector<!-- -->s.
 * If you only need a text only, one column selector, you can create it with
 * hildon_touch_selector_new_text() and populate with
 * hildon_touch_selector_append_text(), hildon_touch_selector_prepend_text(),
 * and hildon_touch_selector_insert_text().
 *
 * If you need a selector widget that also accepts user inputs, you
 * can use #HildonTouchSelectorEntry.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "hildon-gtk.h"

#include "hildon-pannable-area.h"
#include "hildon-touch-selector.h"

#define HILDON_TOUCH_SELECTOR_GET_PRIVATE(obj)                          \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_TOUCH_SELECTOR, HildonTouchSelectorPrivate))

G_DEFINE_TYPE (HildonTouchSelector, hildon_touch_selector, GTK_TYPE_VBOX)

#define CENTER_ON_SELECTED_ITEM_DELAY 50

/*
 * Struct to maintain the data of each column. The columns are the elements
 * of the widget that belongs properly to the selection behaviour. As
 * the selector contents are arranged in a #GtkHBox, you can add more widgets, like buttons etc.
 * between the columns, but this doesn't belongs to the selection
 * logic
 */
typedef struct _SelectorColumn SelectorColumn;
struct _SelectorColumn
{
  HildonTouchSelector *parent;    /* the selector that contains this column */
  GtkTreeModel *model;
  GtkTreeView *tree_view;

  GtkWidget *panarea;           /* the pannable widget */
};

struct _HildonTouchSelectorPrivate
{
  GSList *columns;              /* the selection columns */
  GtkWidget *hbox;              /* the container for the selector's columns */

  HildonTouchSelectorPrintFunc print_func;
};

enum
{
  PROP_HAS_MULTIPLE_SELECTION = 1
};

enum
{
  CHANGED,
  LAST_SIGNAL
};

static gint hildon_touch_selector_signals[LAST_SIGNAL] = { 0 };

static void hildon_touch_selector_get_property (GObject * object,
                                                guint prop_id,
                                                GValue * value, GParamSpec * pspec);

/* gtkwidget */
static void hildon_touch_selector_map (GtkWidget * widget);

/* gtkcontainer */
static void hildon_touch_selector_remove (GtkContainer * container,
                                          GtkWidget * widget);
/* private functions */
static void _selection_changed_cb (GtkTreeSelection * selection,
                                   gpointer user_data);
static gchar *_default_print_func (HildonTouchSelector * selector);

static SelectorColumn *_create_new_column (HildonTouchSelector * selector,
                                           GtkTreeModel * model,
                                           GtkCellRenderer * renderer,
                                           va_list args);
static gboolean _hildon_touch_selector_center_on_selected_items (gpointer data);

static void
_hildon_touch_selector_set_model (HildonTouchSelector * selector,
                                  gint num_column, GtkTreeModel * model);
static gboolean
_hildon_touch_selector_has_multiple_selection (HildonTouchSelector * selector);

static void
hildon_touch_selector_class_init (HildonTouchSelectorClass * class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass *) class;
  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  /* GObject */

  gobject_class->get_property = hildon_touch_selector_get_property;

  /* GtkWidget */
  widget_class->map = hildon_touch_selector_map;

  /* GtkContainer */
  container_class->remove = hildon_touch_selector_remove;

  /* HildonTouchSelector */
  class->set_model = _hildon_touch_selector_set_model;

  class->has_multiple_selection = _hildon_touch_selector_has_multiple_selection;

  /* signals */
  /**
   * HildonTouchSelector::changed:
   * @widget: the object which received the signal
   *
   * The changed signal is emitted when the active
   * item is changed. This can be due to the user selecting
   * a different item from the list, or due to a
   * call to hildon_touch_selector_set_active_iter() on
   * one of the columns.
   *
   */
  hildon_touch_selector_signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (HildonTouchSelectorClass, changed),
                  NULL, NULL,
                  gtk_marshal_NONE__INT, G_TYPE_NONE, 1, G_TYPE_INT);
  /* properties */

  g_object_class_install_property (gobject_class, PROP_HAS_MULTIPLE_SELECTION,
                                   g_param_spec_boolean ("has-multiple-selection",
                                                         "has multiple selection",
                                                         "Whether the widget has multiple "
                                                         "selection (like multiple columns, "
                                                         "multiselection mode, or multiple "
                                                         "internal widgets) and therefore "
                                                         "it may need a confirmation button, "
                                                         "for instance.",
                                                         FALSE,
                                                         G_PARAM_READABLE));

  /* style properties */
  g_type_class_add_private (object_class, sizeof (HildonTouchSelectorPrivate));
}

static void
hildon_touch_selector_get_property (GObject * object,
                                    guint prop_id,
                                    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
  case PROP_HAS_MULTIPLE_SELECTION:
    g_value_set_boolean (value,
                         hildon_touch_selector_has_multiple_selection (HILDON_TOUCH_SELECTOR (object)));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
hildon_touch_selector_init (HildonTouchSelector * selector)
{
  selector->priv = HILDON_TOUCH_SELECTOR_GET_PRIVATE (selector);

  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (selector), GTK_NO_WINDOW);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (selector), FALSE);

  selector->priv->columns = NULL;

  selector->priv->print_func = NULL;
  selector->priv->hbox = gtk_hbox_new (FALSE, 0);

  gtk_box_pack_end (GTK_BOX (selector), selector->priv->hbox,
                    TRUE, TRUE, 0);
  gtk_widget_show (selector->priv->hbox);

  /* FIXME: this is the correct height? A fixed height is the correct 
     implementation */
  gtk_widget_set_size_request (GTK_WIDGET (selector), -1, 320);
}

static void
hildon_touch_selector_map (GtkWidget * widget)
{
  GTK_WIDGET_CLASS (hildon_touch_selector_parent_class)->map (widget);

  g_timeout_add (CENTER_ON_SELECTED_ITEM_DELAY,
                 _hildon_touch_selector_center_on_selected_items, widget);
}

/*------------------------------ GtkContainer ------------------------------ */

/*
 * Required in order to free the column at the columns list
 */
static void
hildon_touch_selector_remove (GtkContainer * container, GtkWidget * widget)
{
  HildonTouchSelector *selector = NULL;
  GSList *iter = NULL;
  gint position = 0;
  SelectorColumn *current_column = NULL;
  gint num_columns = 0;

  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR (container));

  selector = HILDON_TOUCH_SELECTOR (container);
  num_columns = hildon_touch_selector_get_num_columns (selector);

  /* Check if the widget is inside a column and remove
     it from the list */
  iter = selector->priv->columns;
  position = 0;
  while (iter) {
    current_column = (SelectorColumn *) iter->data;
    if (widget == current_column->panarea) {
      current_column = g_slist_nth_data (selector->priv->columns, position);

      selector->priv->columns = g_slist_remove (selector->priv->columns,
                                                current_column);
      g_free (current_column);

      break;
    }

    position++;
    iter = g_slist_next (iter);
  }
  if (position >= num_columns) {
    g_debug ("This widget was not inside the selector column");
  }

  GTK_CONTAINER_CLASS (hildon_touch_selector_parent_class)->remove (container, widget);
}

/* ------------------------------ PRIVATE METHODS ---------------------------- */
/**
 * default_print_func:
 * @selector: a #HildonTouchSelector
 *
 * Default print function
 *
 * Returns: a new string that represent the selected items
 **/
static gchar *
_default_print_func (HildonTouchSelector * selector)
{
  gchar *result = NULL;
  gchar *aux = NULL;
  gint num_columns = 0;
  GtkTreeIter iter;
  GtkTreeModel *model = NULL;
  gchar *current_string = NULL;
  gint i;
  HildonTouchSelectorSelectionMode mode;
  GList *item = NULL;
  GtkTreePath *current_path = NULL;
  GList *selected_rows = NULL;
  gint initial_value = 0;

  num_columns = hildon_touch_selector_get_num_columns (selector);

  mode = hildon_touch_selector_get_column_selection_mode (selector);

  if ((mode == HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE)
      && (num_columns > 0)) {
    /* In this case we get the first column first */
    selected_rows = hildon_touch_selector_get_selected_rows (selector, 0);
    model = hildon_touch_selector_get_model (selector, 0);

    result = g_strdup_printf ("(");
    i = 0;
    for (item = selected_rows; item; item = g_list_next (item)) {
      current_path = item->data;
      gtk_tree_model_get_iter (model, &iter, current_path);

      gtk_tree_model_get (model, &iter, 0, &current_string, -1);

      if (i < g_list_length (selected_rows) - 1) {
        aux = g_strconcat (result, current_string, ",", NULL);
        g_free (result);
        result = aux;
      } else {
        aux = g_strconcat (result, current_string, NULL);
        g_free (result);
        result = aux;
      }
      i++;
    }

    aux = g_strconcat (result, ")", NULL);
    g_free (result);
    result = aux;

    g_list_foreach (selected_rows, (GFunc) (gtk_tree_path_free), NULL);
    g_list_free (selected_rows);
    initial_value = 1;
  } else {
    initial_value = 0;
  }

  for (i = initial_value; i < num_columns; i++) {
    model = hildon_touch_selector_get_model (selector, i);
    if (hildon_touch_selector_get_active_iter (selector, i, &iter)) {

      gtk_tree_model_get (model, &iter, 0, &current_string, -1);
      if (i != 0) {
        aux = g_strconcat (result, ":", current_string, NULL);
        g_free (result);
        result = aux;
      } else {
        result = g_strdup_printf ("%s", current_string);
      }
    }
  }

  return result;
}

static void
_selection_changed_cb (GtkTreeSelection * selection, gpointer user_data)
{
  HildonTouchSelector *selector = NULL;
  SelectorColumn *column = NULL;
  gint num_column = -1;

  column = (SelectorColumn *) user_data;
  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR (column->parent));

  selector = column->parent;

  num_column = g_slist_index (selector->priv->columns, column);

  g_signal_emit (selector, hildon_touch_selector_signals[CHANGED], 0, num_column);
}


static SelectorColumn *
_create_new_column (HildonTouchSelector * selector,
                    GtkTreeModel * model,
                    GtkCellRenderer * renderer, va_list args)
{
  SelectorColumn *new_column = NULL;
  GtkTreeViewColumn *tree_column = NULL;
  GValue val = { 0, };
  GtkTreeView *tv = NULL;
  GtkWidget *panarea = NULL;
  GtkTreeSelection *selection = NULL;
  GtkTreeIter iter;
  gchar *attribute;
  gint value;

  tree_column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (tree_column, renderer, TRUE);

  attribute = va_arg (args, gchar *);
  while (attribute != NULL) {
    value = va_arg (args, gint);
    gtk_tree_view_column_add_attribute (tree_column, renderer, attribute,
                                        value);
    attribute = va_arg (args, gchar *);
  }

  tv = GTK_TREE_VIEW (hildon_gtk_tree_view_new (HILDON_UI_MODE_EDIT));
  gtk_tree_view_set_model (tv, model);
  gtk_tree_view_set_rules_hint (tv, TRUE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), tree_column);

  new_column = (SelectorColumn *) g_malloc0 (sizeof (SelectorColumn));
  new_column->parent = selector;

  panarea = hildon_pannable_area_new ();

  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val, GTK_POLICY_NEVER);
  g_object_set_property (G_OBJECT (panarea), "vscrollbar-policy", &val);

  g_value_unset (&val);
  g_value_init (&val, G_TYPE_BOOLEAN);
  g_value_set_boolean (&val, FALSE);
  g_object_set_property (G_OBJECT (panarea), "initial-hint", &val);

  gtk_container_add (GTK_CONTAINER (panarea), GTK_WIDGET (tv));

  new_column->model = model;
  new_column->tree_view = tv;
  new_column->panarea = panarea;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

  /* select the first item */
  if (gtk_tree_model_get_iter_first (model, &iter)) {
    gtk_tree_selection_select_iter (selection, &iter);
  }

  gtk_widget_grab_focus (GTK_WIDGET (tv));

  /* connect to the changed signal connection */
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (_selection_changed_cb), new_column);

  return new_column;
}

/* ------------------------------ PUBLIC METHODS ---------------------------- */

/**
 * hildon_touch_selector_new:
 *
 * Creates a new empty #HildonTouchSelector.
 *
 * Returns: a new #HildonTouchSelector.
 **/
GtkWidget *
hildon_touch_selector_new (void)
{
  return g_object_new (HILDON_TYPE_TOUCH_SELECTOR, NULL);
}

/**
 * hildon_touch_selector_new_text:
 *
 * Creates a #HildonTouchSelector with a single text column that
 * can be populated conveniently through hildon_touch_selector_append_text(),
 * hildon_touch_selector_prepend_text(), hildon_touch_selector_insert_text().
 *
 * Returns: A new #HildonTouchSelector
 **/
GtkWidget *
hildon_touch_selector_new_text (void)
{
  GtkWidget *selector;
  GtkListStore *store;

  selector = hildon_touch_selector_new ();
  store = gtk_list_store_new (1, G_TYPE_STRING);

  hildon_touch_selector_append_text_column (HILDON_TOUCH_SELECTOR (selector),
                                            GTK_TREE_MODEL (store), TRUE);

  return selector;
}

/**
 * hildon_touch_selector_append_text:
 * @selector: A #HildonTouchSelector.
 * @text: a non %NULL text string.
 *
 * Appends a new entry in a #HildonTouchSelector created with
 * hildon_touch_selector_new_text().
 **/
void
hildon_touch_selector_append_text (HildonTouchSelector * selector,
                                   const gchar * text)
{
  GtkTreeIter iter;
  GtkTreeModel *model;

  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR (selector));
  g_return_if_fail (text != NULL);

  model = hildon_touch_selector_get_model (HILDON_TOUCH_SELECTOR (selector), 0);

  g_return_if_fail (GTK_IS_LIST_STORE (model));

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, text, -1);
}

/**
 * hildon_touch_selector_prepend_text:
 * @selector: A #HildonTouchSelector.
 * @text: a non %NULL text string.
 *
 * Prepends a new entry in a #HildonTouchSelector created with
 * hildon_touch_selector_new_text().
 **/
void
hildon_touch_selector_prepend_text (HildonTouchSelector * selector,
                                    const gchar * text)
{
  GtkTreeIter iter;
  GtkTreeModel *model;

  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR (selector));
  g_return_if_fail (text != NULL);

  model = hildon_touch_selector_get_model (HILDON_TOUCH_SELECTOR (selector), 0);

  g_return_if_fail (GTK_IS_LIST_STORE (model));

  gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, text, -1);
}

/**
 * hildon_touch_selector_insert_text:
 * @selector: a #HildonTouchSelector.
 * @position: the position to insert @text.
 * @text: A non %NULL text string.
 *
 * Inserts a new entry in particular position of a #HildoTouchSelector created
 * with hildon_touch_selector_new_text().
 *
 **/
void
hildon_touch_selector_insert_text (HildonTouchSelector * selector,
                                   gint position, const gchar * text)
{
  GtkTreeIter iter;
  GtkTreeModel *model;

  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR (selector));
  g_return_if_fail (text != NULL);
  g_return_if_fail (position >= 0);

  model = hildon_touch_selector_get_model (HILDON_TOUCH_SELECTOR (selector), 0);

  g_return_if_fail (GTK_IS_LIST_STORE (model));

  gtk_list_store_insert (GTK_LIST_STORE (model), &iter, position);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, text, -1);
}

/**
 * hildon_touch_selector_append_column
 * @selector: a #HildonTouchSelector
 * @model: the #GtkTreeModel with the data of the column
 * @cell_renderer: The #GtkCellRenderer where to draw each row contents.
 * @Varargs: a %NULL-terminated pair of attributes and column numbers.
 *
 * This functions adds a new column to the widget, whose data will
 * be obtained from the model. Only widgets added this way should used on
 * the selection logic, i.e., the print function, the #HildonTouchPicker::changed
 * signal, etc.
 *
 * Contents will be represented in @cell_renderer. You can pass a %NULL-terminated
 * list of pairs property/value, in the same way you would use
 * gtk_tree_view_column_set_attributes().
 *
 * There is a prerequisite to be considered on models used: text data must
 * be in the first column.
 *
 * This method basically adds a #GtkTreeView to the widget, using the model and
 * the data received.
 *
 * Returns: %TRUE if a new column was added, %FALSE otherwise
 **/

gboolean
hildon_touch_selector_append_column (HildonTouchSelector * selector,
                                     GtkTreeModel * model,
                                     GtkCellRenderer * cell_renderer, ...)
{
  va_list args;
  SelectorColumn *new_column = NULL;

  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);

  if (model != NULL) {

    va_start (args, cell_renderer);
    new_column = _create_new_column (selector, model, cell_renderer, args);
    va_end (args);

    selector->priv->columns = g_slist_append (selector->priv->columns,
                                              new_column);
    gtk_box_pack_start (GTK_BOX (selector->priv->hbox), new_column->panarea, TRUE, TRUE, 6);

    gtk_widget_show_all (new_column->panarea);
  } else {
    return FALSE;
  }

  return TRUE;
}

/**
 * hildon_touch_selector_append_text_column
 * @selector: a #HildonTouchSelector
 * @model: a #GtkTreeModel with data for the column
 * @center: whether to center the text on the column
 *
 * Equivalent to hildon_touch_selector_append_column(), but using a
 * default text cell renderer. This is the most common use case of the
 * widget.
 *
 * Returns: %TRUE if a new column was added, %FALSE otherwise.
 **/
gboolean
hildon_touch_selector_append_text_column (HildonTouchSelector * selector,
                                          GtkTreeModel * model, gboolean center)
{
  GtkCellRenderer *renderer = NULL;
  GValue val = { 0, };

  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);

  if (model != NULL) {
    renderer = gtk_cell_renderer_text_new ();

    if (center) {
      g_value_init (&val, G_TYPE_FLOAT);
      g_value_set_float (&val, 0.5);
      /* FIXME: center the text, this should be configurable */
      g_object_set_property (G_OBJECT (renderer), "xalign", &val);
    }

    return hildon_touch_selector_append_column (selector, model, renderer,
                                                "text", 0, NULL);
  } else {
    return FALSE;
  }
}

/**
 * hildon_touch_selector_remove_column:
 * @selector: a #HildonTouchSelector
 * @column: the position of the column to be removed
 *
 * Removes a column from @selector.
 *
 * Returns: %TRUE if the column was removed, %FALSE otherwise
 **/
gboolean
hildon_touch_selector_remove_column (HildonTouchSelector * selector, gint column)
{
  SelectorColumn *current_column = NULL;

  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), FALSE);
  g_return_val_if_fail (column <
                        hildon_touch_selector_get_num_columns (selector), FALSE);

  current_column = g_slist_nth_data (selector->priv->columns, column);

  gtk_container_remove (GTK_CONTAINER (selector), current_column->panarea);

  return TRUE;
}

/**
 * hildon_touch_selector_set_column_attributes:
 * @selector: a #HildonTouchSelector
 * @num_column: the number of the column whose attributes we're setting
 * @cell_renderer: the #GtkCellRendere we're setting the attributes of
 * @Varargs: A %NULL-terminated list of attributes.
 *
 * Sets the attributes for the given column. The attributes must be given
 * in attribute/column pairs, just like in gtk_tree_view_column_set_attributes().
 * All existing attributes are removed and replaced with the new ones.
 *
 **/
void
hildon_touch_selector_set_column_attributes (HildonTouchSelector * selector,
                                             gint num_column,
                                             GtkCellRenderer * cell_renderer,
                                             ...)
{
  va_list args;
  GtkTreeViewColumn *tree_column = NULL;
  SelectorColumn *current_column = NULL;
  gchar *attribute = NULL;
  gint value = 0;

  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR (selector));
  g_return_if_fail (num_column <
                    hildon_touch_selector_get_num_columns (selector));

  current_column = g_slist_nth_data (selector->priv->columns, num_column);

  tree_column = gtk_tree_view_get_column (current_column->tree_view, 0);
  gtk_tree_view_remove_column (current_column->tree_view, tree_column);

  tree_column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (tree_column, cell_renderer, TRUE);

  va_start (args, cell_renderer);
  attribute = va_arg (args, gchar *);

  gtk_tree_view_column_clear_attributes (tree_column, cell_renderer);

  while (attribute != NULL) {
    value = va_arg (args, gint);
    gtk_tree_view_column_add_attribute (tree_column, cell_renderer,
                                        attribute, value);
    attribute = va_arg (args, gchar *);
  }

  va_end (args);

  gtk_tree_view_append_column (current_column->tree_view, tree_column);
}

#if 0
gboolean
hildon_touch_selector_insert_column (HildonTouchSelector * selector, gint position)
{
  g_warning ("Un-implemented!");

  return TRUE;
}
#endif

/**
 * hildon_touch_selector_get_num_columns:
 * @selector: a #HildonTouchSelector
 *
 * Gets the number of columns in the #HildonTouchSelector.
 *
 * Returns: the number of columns in @selector.
 **/
gint
hildon_touch_selector_get_num_columns (HildonTouchSelector * selector)
{
  return g_slist_length (selector->priv->columns);
}

/**
 * hildon_touch_selector_get_column_selection_mode:
 * @selector: a #HildonTouchSelector
 *
 * Gets the selection mode of @selector.
 *
 * Returns: one of #HildonTouchSelectorSelectionMode
 **/
HildonTouchSelectorSelectionMode
hildon_touch_selector_get_column_selection_mode (HildonTouchSelector * selector)
{
  HildonTouchSelectorSelectionMode result =
    HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE;
  GtkSelectionMode treeview_mode = GTK_SELECTION_SINGLE;
  SelectorColumn *column = NULL;
  GtkTreeSelection *selection = NULL;

  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), result);
  g_return_val_if_fail (hildon_touch_selector_get_num_columns (selector) > 0,
                        result);

  column = (SelectorColumn *) selector->priv->columns->data;

  selection = gtk_tree_view_get_selection (column->tree_view);
  treeview_mode = gtk_tree_selection_get_mode (selection);


  if (treeview_mode == GTK_SELECTION_MULTIPLE) {
    result = HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE;
  } else {
    result = HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE;
  }

  return result;
}

/**
 * hildon_touch_selector_set_column_selection_mode:
 * @selector: a #HildonTouchSelector
 * @mode: the #HildonTouchSelectorMode for @selector
 *
 * Sets the selection mode for @selector. See #HildonTouchSelectorSelectionMode.
 **/
void
hildon_touch_selector_set_column_selection_mode (HildonTouchSelector * selector,
                                                 HildonTouchSelectorSelectionMode mode)
{
  GtkTreeView *tv = NULL;
  SelectorColumn *column = NULL;
  GtkTreeSelection *selection = NULL;
  GtkSelectionMode treeview_mode = GTK_SELECTION_MULTIPLE;
  GtkTreeIter iter;

  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR (selector));
  g_return_if_fail (hildon_touch_selector_get_num_columns (selector) > 0);

  column = (SelectorColumn *) (g_slist_nth (selector->priv->columns, 0))->data;
  tv = column->tree_view;

  if (tv) {
    switch (mode) {
    case HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE:
      treeview_mode = GTK_SELECTION_SINGLE;
      break;
    case HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE:
      treeview_mode = GTK_SELECTION_MULTIPLE;
      break;
    }

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));
    gtk_tree_selection_set_mode (selection, treeview_mode);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));
    gtk_tree_model_get_iter_first (column->model, &iter);
    gtk_tree_selection_unselect_all (selection);
    gtk_tree_selection_select_iter (selection, &iter);
  }

}

/**
 * hildon_touch_selector_set_print_func:
 * @selector: a #HildonTouchSelector
 * @func: a #HildonTouchSelectorPrintFunc function
 *
 * Sets the function to be used by hildon_touch_selector_get_current_text()
 * to produce a text representation of the currently selected items in @selector.
 * The default function will return a concatenation of comma separated items
 * selected in each column in @selector. Use this to override this method if you
 * need a particular representation for your application.
 *
 **/
void
hildon_touch_selector_set_print_func (HildonTouchSelector * selector,
                                      HildonTouchSelectorPrintFunc func)
{
  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR (selector));

  selector->priv->print_func = func;
}

/**
 * hildon_touch_selector_get_print_func:
 * @selector: a #HildonTouchSelector
 *
 * Gets the #HildonTouchSelectorPrintFunc currently used. See
 * hildon_touch_selector_set_print_func().
 *
 * Returns: a #HildonTouchSelectorPrintFunc or %NULL if the default
 * one is currently used.
 **/
HildonTouchSelectorPrintFunc
hildon_touch_selector_get_print_func (HildonTouchSelector * selector)
{
  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), NULL);

  return selector->priv->print_func;
}

/**
 * hildon_touch_selector_get_active_iter:
 * @selector: a #HildonTouchSelector
 * @column: the column number we want to get the element
 * @iter: #GtkTreeIter currently selected
 *
 * Sets @iter to the currently selected node on the nth-column, if selection is set to
 * %HILDON_TOUCH_SELECTOR_SINGLE. @iter may be %NULL if you just want to test if selection
 * has any selected items.
 *
 * This function will not work if selection is in
 * %HILDON_TOUCH_SELECTOR_MULTIPLE mode.
 *
 * See gtk_tree_selection_get_selected() for more information.
 *
 * Returns: %TRUE if @iter was correctly set, %FALSE otherwise
 **/
gboolean
hildon_touch_selector_get_active_iter (HildonTouchSelector * selector,
                                       gint column, GtkTreeIter * iter)
{
  GtkTreeSelection *selection = NULL;
  SelectorColumn *current_column = NULL;

  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), FALSE);
  g_return_val_if_fail (hildon_touch_selector_get_column_selection_mode (selector)
                        == HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE, FALSE);
  g_return_val_if_fail (column < hildon_touch_selector_get_num_columns (selector),
                        FALSE);

  current_column = g_slist_nth_data (selector->priv->columns, column);

  selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (current_column->tree_view));

  return gtk_tree_selection_get_selected (selection, NULL, iter);
}

/**
 * hildon_touch_selector_set_active_iter
 * @selector: a #HildonTouchSelector
 * @column:   the column to selects
 * @iter:     the #GtkTreeIter to be selected
 * @scroll_to: whether to smoothly scroll to the item
 *
 * Sets the currently selected item in the column @column to the one pointed by @iter,
 * optionally smoothly scrolling to it.
 *
 **/
void
hildon_touch_selector_set_active_iter (HildonTouchSelector * selector,
                                       gint column, GtkTreeIter * iter,
                                       gboolean scroll_to)
{
  GtkTreePath *path;
  GtkTreeModel *model;
  GdkRectangle rect;
  SelectorColumn *current_column = NULL;
  GtkTreeSelection *selection = NULL;
  gint y;

  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR (selector));
  g_return_if_fail (column < hildon_touch_selector_get_num_columns (selector));

  current_column = g_slist_nth_data (selector->priv->columns, column);

  selection = gtk_tree_view_get_selection (current_column->tree_view);
  model = gtk_tree_view_get_model (current_column->tree_view);
  path = gtk_tree_model_get_path (model, iter);

  gtk_tree_selection_select_iter (selection, iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (current_column->tree_view), path, NULL, FALSE);

  if (scroll_to) {
    gtk_tree_view_get_background_area (current_column->tree_view,
                                       path, NULL, &rect);
    gtk_tree_view_convert_bin_window_to_tree_coords (current_column->tree_view,
                                                     0, rect.y, NULL, &y);
    hildon_pannable_area_scroll_to (HILDON_PANNABLE_AREA (current_column->panarea),
                                    -1, y);
  }
  gtk_tree_path_free (path);
}

/**
 * hildon_touch_selector_get_selected_rows:
 * @selector: a #HildonTouchSelector
 * @column: the position of the column to get the selected rows from
 *
 * Creates a list of #GtkTreePath<!-- -->s of all selected rows in a column. Additionally,
 * if you to plan to modify the model after calling this function, you may
 * want to convert the returned list into a list of GtkTreeRowReferences. To do this,
 * you can use gtk_tree_row_reference_new().
 *
 * See gtk_tree_selection_get_selected_rows() for more information.
 *
 * Returns: A new #GList containing a #GtkTreePath for each selected row in the column @column.
 *
 **/
GList *
hildon_touch_selector_get_selected_rows (HildonTouchSelector * selector,
                                         gint column)
{
  GList *result = NULL;
  SelectorColumn *current_column = NULL;
  GtkTreeSelection *selection = NULL;

  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), NULL);
  g_return_val_if_fail (column < hildon_touch_selector_get_num_columns (selector),
                        NULL);

  current_column = g_slist_nth_data (selector->priv->columns, column);
  selection = gtk_tree_view_get_selection (current_column->tree_view);

  result = gtk_tree_selection_get_selected_rows (selection, NULL);


  return result;
}

/**
 * hildon_touch_selector_get_model:
 * @selector: a #HildonTouchSelector
 * @column: the position of the column in @selector
 *
 * Gets the model of a column of @selector.
 *
 * Returns: the #GtkTreeModel for the column @column of @selector.
 **/
GtkTreeModel *
hildon_touch_selector_get_model (HildonTouchSelector * selector, gint column)
{
  SelectorColumn *current_column = NULL;

  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), NULL);
  g_return_val_if_fail (column < hildon_touch_selector_get_num_columns (selector),
                        NULL);

  current_column = g_slist_nth_data (selector->priv->columns, column);

  return current_column->model;
}

static void
_hildon_touch_selector_set_model (HildonTouchSelector * selector,
                                 gint column, GtkTreeModel * model)
{
  SelectorColumn *current_column = NULL;
  g_print ("this was actually called\n");

  current_column =
    (SelectorColumn *) g_slist_nth_data (selector->priv->columns, column);

  current_column->model = model;
  gtk_tree_view_set_model (current_column->tree_view, current_column->model);
}

/**
 * hildon_touch_selector_set_model:
 * @selector: a #HildonTouchSelector
 * @column: the position of the column to set the model to
 * @model: a #GtkTreeModel
 *
 * Sets the #GtkTreeModel for a particular column in @model.
 **/
void
hildon_touch_selector_set_model (HildonTouchSelector * selector,
                                 gint column, GtkTreeModel * model)
{
  g_return_if_fail (HILDON_TOUCH_SELECTOR (selector));
  g_return_if_fail (column < hildon_touch_selector_get_num_columns (selector));

  HILDON_TOUCH_SELECTOR_GET_CLASS (selector)->set_model (selector, column, model);
}

/**
 * hildon_touch_selector_get_current_text:
 * @selector: a #HildonTouchSelector
 *
 * Returns a string representing the currently selected items for
 * each column of @selector. See hildon_touch_selector_set_print_func().
 *
 * Returns: a newly allocated string.
 **/
gchar *
hildon_touch_selector_get_current_text (HildonTouchSelector * selector)
{
  gchar *result = NULL;
  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), NULL);

  if (selector->priv->print_func) {
    result = (*selector->priv->print_func) (selector);
  } else {
    result = _default_print_func (selector);
  }

  return result;
}

static gboolean
_hildon_touch_selector_center_on_selected_items (gpointer data)
{
  HildonTouchSelector *selector = NULL;
  SelectorColumn *column = NULL;
  GSList *iter_column = NULL;
  GtkTreeIter iter;
  GtkTreePath *path;
  GdkRectangle rect;
  gint y;
  gint i;
  HildonTouchSelectorSelectionMode selection_mode;

  /* ensure to center on the initial values */
  selector = HILDON_TOUCH_SELECTOR (data);

  selection_mode = hildon_touch_selector_get_column_selection_mode (selector);

  iter_column = selector->priv->columns;
  i = 0;
  while (iter_column) {
    column = (SelectorColumn *) iter_column->data;

    if ((i == 0)
        && (selection_mode == HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE)) {
      break;
    }
    if (hildon_touch_selector_get_active_iter (selector, i, &iter)) {
      path = gtk_tree_model_get_path (column->model, &iter);
      gtk_tree_view_get_background_area (GTK_TREE_VIEW
                                         (column->tree_view), path, NULL,
                                         &rect);

      gtk_tree_view_convert_bin_window_to_tree_coords (GTK_TREE_VIEW
                                                       (column->tree_view), 0,
                                                       rect.y, NULL, &y);

      hildon_pannable_area_scroll_to (HILDON_PANNABLE_AREA
                                      (column->panarea), -1, y);

      gtk_tree_path_free (path);
    }
    iter_column = iter_column->next;
    i++;
  }

  return FALSE;
}

static gboolean
_hildon_touch_selector_has_multiple_selection (HildonTouchSelector * selector)
{
  HildonTouchSelectorSelectionMode mode = HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE;
  gint n_columns = 0;

  n_columns = hildon_touch_selector_get_num_columns (selector);
  mode = hildon_touch_selector_get_column_selection_mode (selector);

  return ((n_columns > 1) || (mode == HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE));
}

/**
 * hildon_touch_selector_has_multiple_selection:
 * @selector: A #HildonTouchSelector
 *
 * Determines whether @selector is complex enough to actually require an
 * extra selection step than only picking an item. This is normally %TRUE
 * if @selector has multiple columns, multiple selection, or when it is a
 * more complex widget, like %HildonTouchSelectorEntry.
 *
 * This information is useful for widgets containing a %HildonTouchSelector,
 * like #HildonPickerDialog, that could need a "Done" button, in case that
 * its internal #HildonTouchSelector has multiple columns, for instance.
 *
 * Returns: %TRUE if @selector requires multiple selection steps.
 **/
gboolean
hildon_touch_selector_has_multiple_selection (HildonTouchSelector * selector)
{
  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR (selector), FALSE);

  return HILDON_TOUCH_SELECTOR_GET_CLASS (selector)->has_multiple_selection (selector);
}
