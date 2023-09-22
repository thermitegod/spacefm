/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#define PTK_DIR_TREE(obj)             (static_cast<PtkDirTree*>(obj))
#define PTK_DIR_TREE_REINTERPRET(obj) (reinterpret_cast<PtkDirTree*>(obj))

/* Columns of folder view */
namespace ptk::dir_tree
{
    enum class column
    {
        icon,
        disp_name,
        info,
    };
}

struct PtkDirTreeNode;

struct PtkDirTree
{
    GObject parent;
    /* <private> */

    PtkDirTreeNode* root{nullptr};
    /* GtkSortType sort_order; */ /* I do not want to support this :-( */
    /* Random integer to check whether an iter belongs to our model */
    const i32 stamp{std::rand()};
};

GType ptk_dir_tree_get_type();

PtkDirTree* ptk_dir_tree_new();

void ptk_dir_tree_expand_row(PtkDirTree* tree, GtkTreeIter* iter, GtkTreePath* path);

void ptk_dir_tree_collapse_row(PtkDirTree* tree, GtkTreeIter* iter, GtkTreePath* path);

char* ptk_dir_tree_get_dir_path(PtkDirTree* tree, GtkTreeIter* iter);
