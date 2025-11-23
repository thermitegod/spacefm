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

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/file.hxx"
#include "vfs/notify-cpp/notify_controller.hxx"

#define PTK_DIR_TREE(obj)             (static_cast<gui::dir_tree*>(obj))
#define PTK_DIR_TREE_REINTERPRET(obj) (reinterpret_cast<gui::dir_tree*>(obj))

namespace gui
{
struct dir_tree // : public std::enable_shared_from_this<gui::dir_tree>, Gtk::TreeModel
{
    /* Columns of folder view */
    enum class column : std::uint8_t
    {
        icon,
        disp_name,
        info,
    };

    GObject parent;

    static gui::dir_tree* create() noexcept;

    void expand_row(GtkTreeIter* iter, GtkTreePath* path) noexcept;
    void collapse_row(GtkTreeIter* iter, GtkTreePath* path) noexcept;
    static std::optional<std::filesystem::path> get_dir_path(GtkTreeIter* iter) noexcept;

    /* <private> */
    struct node : public std::enable_shared_from_this<node>
    {
        node() = default;
        ~node()
        {
            if (this->notifier_thread.joinable())
            {
                this->notifier_thread.request_stop();
                this->notifier_thread.join();
            }
        }

        [[nodiscard]] static std::shared_ptr<node> create() noexcept;
        [[nodiscard]] static std::shared_ptr<node>
        create(gui::dir_tree* tree, const std::shared_ptr<node>& parent,
               const std::filesystem::path& path) noexcept;

        std::shared_ptr<vfs::file> file{nullptr};
        std::shared_ptr<node> children{nullptr};
        std::int32_t n_children{0};
        std::int32_t n_expand{0};
        std::shared_ptr<node> parent{nullptr};
        std::shared_ptr<node> next{nullptr};
        std::shared_ptr<node> prev{nullptr};
        std::shared_ptr<node> last{nullptr};
        gui::dir_tree* tree{nullptr}; /* FIXME: This is a waste of memory :-( */

        notify::notify_controller notifier = notify::inotify_controller();
        std::jthread notifier_thread;

        std::shared_ptr<node> get_nth_node(std::int32_t n) const noexcept;
        std::shared_ptr<node> find_node(const std::string_view name) const noexcept;
        std::ptrdiff_t get_node_index(const std::shared_ptr<node>& child) const noexcept;

        void on_file_created(const std::filesystem::path& path) noexcept;
        void on_file_deleted(const std::filesystem::path& path) noexcept;
    };
    std::shared_ptr<node> root{nullptr};

    /* GtkSortType sort_order; */ /* I do not want to support this :-( */

    /* Random integer to check whether an iter belongs to our model */
    std::int32_t stamp{0};

  private:
    void insert_child(const std::shared_ptr<node>& parent,
                      const std::filesystem::path& file_path = "") noexcept;
    void delete_child(const std::shared_ptr<gui::dir_tree::node>& child) noexcept;

    static std::int32_t node_compare(const std::shared_ptr<gui::dir_tree::node>& a,
                                     const std::shared_ptr<gui::dir_tree::node>& b) noexcept;
};
} // namespace gui
