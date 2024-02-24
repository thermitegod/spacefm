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

#define PTK_DIR_TREE(obj)             (static_cast<ptk::dir_tree*>(obj))
#define PTK_DIR_TREE_REINTERPRET(obj) (reinterpret_cast<ptk::dir_tree*>(obj))

namespace ptk
{
struct dir_tree // : public std::enable_shared_from_this<ptk::dir_tree>, Gtk::TreeModel
{
    /* Columns of folder view */
    enum class column
    {
        icon,
        disp_name,
        info,
    };

    GObject parent;

    static ptk::dir_tree* create() noexcept;

    void expand_row(GtkTreeIter* iter, GtkTreePath* path) noexcept;
    void collapse_row(GtkTreeIter* iter, GtkTreePath* path) noexcept;
    static const std::optional<std::filesystem::path> get_dir_path(GtkTreeIter* iter) noexcept;

    /* <private> */
    struct node : public std::enable_shared_from_this<node>
    {
        [[nodiscard]] static const std::shared_ptr<node> create() noexcept;
        [[nodiscard]] static const std::shared_ptr<node>
        create(ptk::dir_tree* tree, const std::shared_ptr<node>& parent,
               const std::filesystem::path& path) noexcept;

        std::shared_ptr<vfs::file> file{nullptr};
        std::shared_ptr<node> children{nullptr};
        i32 n_children{0};
        std::shared_ptr<vfs::monitor> monitor{nullptr};
        i32 n_expand{0};
        std::shared_ptr<node> parent{nullptr};
        std::shared_ptr<node> next{nullptr};
        std::shared_ptr<node> prev{nullptr};
        std::shared_ptr<node> last{nullptr};
        ptk::dir_tree* tree{nullptr}; /* FIXME: This is a waste of memory :-( */

        const std::shared_ptr<node> get_nth_node(i32 n) const noexcept;
        const std::shared_ptr<node> find_node(const std::string_view name) const noexcept;
        isize get_node_index(const std::shared_ptr<node>& child) const noexcept;

        /* file monitor callback */
        void on_monitor_event(const vfs::monitor::event event,
                              const std::filesystem::path& path) noexcept;
    };
    std::shared_ptr<node> root{nullptr};

    /* GtkSortType sort_order; */ /* I do not want to support this :-( */

    /* Random integer to check whether an iter belongs to our model */
    i32 stamp{0};

  private:
    void insert_child(const std::shared_ptr<node>& parent,
                      const std::filesystem::path& file_path = "") noexcept;
    void delete_child(const std::shared_ptr<ptk::dir_tree::node>& child) noexcept;

    static i32 node_compare(const std::shared_ptr<ptk::dir_tree::node>& a,
                            const std::shared_ptr<ptk::dir_tree::node>& b) noexcept;
};
} // namespace ptk
