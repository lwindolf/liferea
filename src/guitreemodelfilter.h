/* gtktreemodelfilter.h
 * Copyright (C) 2000,2001  Red Hat, Inc., Jonathan Blandford <jrb@redhat.com>
 * Copyright (C) 2001-2003  Kristian Rietveld <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GUI_TREE_MODEL_FILTER_H__
#define __GUI_TREE_MODEL_FILTER_H__

#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define GUI_TYPE_TREE_MODEL_FILTER              (gui_tree_model_filter_get_type ())
#define GUI_TREE_MODEL_FILTER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GUI_TYPE_TREE_MODEL_FILTER, GuiTreeModelFilter))
#define GUI_TREE_MODEL_FILTER_CLASS(vtable)     (G_TYPE_CHECK_CLASS_CAST ((vtable), GUI_TYPE_TREE_MODEL_FILTER, GuiTreeModelFilterClass))
#define GUI_IS_TREE_MODEL_FILTER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GUI_TYPE_TREE_MODEL_FILTER))
#define GUI_IS_TREE_MODEL_FILTER_CLASS(vtable)  (G_TYPE_CHECK_CLASS_TYPE ((vtable), GUI_TYPE_TREE_MODEL_FILTER))
#define GUI_TREE_MODEL_FILTER_GET_CLASS(inst)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GUI_TYPE_TREE_MODEL_FILTER, GuiTreeModelFilterClass))

typedef gboolean (* GuiTreeModelFilterVisibleFunc) (GtkTreeModel *model,
                                                    GtkTreeIter  *iter,
                                                    gpointer      data);
typedef void (* GuiTreeModelFilterModifyFunc) (GtkTreeModel *model,
                                               GtkTreeIter  *iter,
                                               GValue       *value,
                                               gint          column,
                                               gpointer      data);

typedef struct _GuiTreeModelFilter          GuiTreeModelFilter;
typedef struct _GuiTreeModelFilterClass     GuiTreeModelFilterClass;
typedef struct _GuiTreeModelFilterPrivate   GuiTreeModelFilterPrivate;

struct _GuiTreeModelFilter
{
  GObject parent;

  /*< private >*/
  GuiTreeModelFilterPrivate *priv;
};

struct _GuiTreeModelFilterClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved0) (void);
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
};

/* base */
GType         gui_tree_model_filter_get_type                   (void);
GtkTreeModel *gui_tree_model_filter_new                        (GtkTreeModel                 *child_model,
                                                                GtkTreePath                  *root);
void          gui_tree_model_filter_set_visible_func           (GuiTreeModelFilter           *filter,
                                                                GuiTreeModelFilterVisibleFunc func,
                                                                gpointer                      data,
                                                                GtkDestroyNotify              destroy);
void          gui_tree_model_filter_set_modify_func            (GuiTreeModelFilter           *filter,
                                                                gint                          n_columns,
                                                                GType                        *types,
                                                                GuiTreeModelFilterModifyFunc  func,
                                                                gpointer                      data,
                                                                GtkDestroyNotify              destroy);
void          gui_tree_model_filter_set_visible_column         (GuiTreeModelFilter           *filter,
                                                                gint                          column);

GtkTreeModel *gui_tree_model_filter_get_model                  (GuiTreeModelFilter           *filter);

/* conversion */
void          gui_tree_model_filter_convert_child_iter_to_iter (GuiTreeModelFilter           *filter,
                                                                GtkTreeIter                  *filter_iter,
                                                                GtkTreeIter                  *child_iter);
void          gui_tree_model_filter_convert_iter_to_child_iter (GuiTreeModelFilter           *filter,
                                                                GtkTreeIter                  *child_iter,
                                                                GtkTreeIter                  *filter_iter);
GtkTreePath  *gui_tree_model_filter_convert_child_path_to_path (GuiTreeModelFilter           *filter,
                                                                GtkTreePath                  *child_path);
GtkTreePath  *gui_tree_model_filter_convert_path_to_child_path (GuiTreeModelFilter           *path,
                                                                GtkTreePath                  *filter_path);

/* extras */
void          gui_tree_model_filter_refilter                   (GuiTreeModelFilter           *filter);
void          gui_tree_model_filter_clear_cache                (GuiTreeModelFilter           *filter);

G_END_DECLS

#endif /* __GUI_TREE_MODEL_FILTER_H__ */
