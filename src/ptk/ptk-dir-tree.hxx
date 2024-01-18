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

#include <filesystem>

#include <memory>

#include <optional>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-monitor.hxx"
#include "vfs/vfs-file.hxx"

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

struct PtkDirTree // : public std::enable_shared_from_this<PtkDirTree>, Gtk::TreeModel
{
    GObject parent;

    static PtkDirTree* create() noexcept;

    void expand_row(GtkTreeIter* iter, GtkTreePath* path) noexcept;
    void collapse_row(GtkTreeIter* iter, GtkTreePath* path) noexcept;
    const std::optional<std::filesystem::path> get_dir_path(GtkTreeIter* iter) const noexcept;

    /* <private> */
    struct Node : public std::enable_shared_from_this<Node>
    {
        static const std::shared_ptr<Node> create();
        static const std::shared_ptr<Node> create(PtkDirTree* tree,
                                                  const std::shared_ptr<Node>& parent,
                                                  const std::filesystem::path& path);

        std::shared_ptr<vfs::file> file{nullptr};
        std::shared_ptr<Node> children{nullptr};
        i32 n_children{0};
        std::shared_ptr<vfs::monitor> monitor{nullptr};
        i32 n_expand{0};
        std::shared_ptr<Node> parent{nullptr};
        std::shared_ptr<Node> next{nullptr};
        std::shared_ptr<Node> prev{nullptr};
        std::shared_ptr<Node> last{nullptr};
        PtkDirTree* tree{nullptr}; /* FIXME: This is a waste of memory :-( */

        const std::shared_ptr<Node> get_nth_node(i32 n) const;
        const std::shared_ptr<Node> find_node(const std::string_view name) const;
        isize get_node_index(const std::shared_ptr<Node>& child) const;

        /* file monitor callback */
        void on_monitor_event(const vfs::monitor::event event, const std::filesystem::path& path);
    };
    std::shared_ptr<Node> root{nullptr};

    /* GtkSortType sort_order; */ /* I do not want to support this :-( */

    /* Random integer to check whether an iter belongs to our model */
    const i32 stamp{std::rand()};

  private:
    void insert_child(const std::shared_ptr<Node>& parent,
                      const std::filesystem::path& file_path = "") noexcept;
    void delete_child(const std::shared_ptr<PtkDirTree::Node>& child) noexcept;

    i32 node_compare(const std::shared_ptr<PtkDirTree::Node>& a,
                     const std::shared_ptr<PtkDirTree::Node>& b) const noexcept;
};
