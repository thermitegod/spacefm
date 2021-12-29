/*
 *  C Interface: ptk-dir-tree
 *
 * Description:
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>

#include <sys/types.h>

#define PTK_TYPE_DIR_TREE    (ptk_dir_tree_get_type())
#define PTK_DIR_TREE(obj)    (reinterpret_cast<PtkDirTree*>(obj))
#define PTK_IS_DIR_TREE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_DIR_TREE))

/* Columns of folder view */
enum PTKDirTreeCol
{
    COL_DIR_TREE_ICON,
    COL_DIR_TREE_DISP_NAME,
    COL_DIR_TREE_INFO,
    N_DIR_TREE_COLS
};

struct PtkDirTreeNode;

struct PtkDirTree
{
    GObject parent;
    /* <private> */

    PtkDirTreeNode* root;
    /* GtkSortType sort_order; */ /* I don't want to support this :-( */
    /* Random integer to check whether an iter belongs to our model */
    int stamp;
};

struct PtkDirTreeClass
{
    GObjectClass parent;
    /* Default signal handlers */
};

GType ptk_dir_tree_get_type();

PtkDirTree* ptk_dir_tree_new();

void ptk_dir_tree_expand_row(PtkDirTree* tree, GtkTreeIter* iter, GtkTreePath* path);

void ptk_dir_tree_collapse_row(PtkDirTree* tree, GtkTreeIter* iter, GtkTreePath* path);

char* ptk_dir_tree_get_dir_path(PtkDirTree* tree, GtkTreeIter* iter);
