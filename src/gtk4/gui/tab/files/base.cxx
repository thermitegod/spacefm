/**
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

#include <algorithm>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>

#include <fnmatch.h>

#include <gdkmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/tab/files/base.hxx"

#include "logger.hxx"
#include "natsort/strnatcmp.hxx"

gui::files_base::files_base(const std::shared_ptr<config::settings>& settings) : settings_(settings)
{
    dir_model_ = Gio::ListStore<ModelColumns>::create();
    selection_model_ = Gtk::MultiSelection::create(dir_model_);
}

gui::files_base::~files_base()
{
    signal_files_changed.disconnect();
    signal_files_created.disconnect();
    signal_files_deleted.disconnect();
    signal_thumbnail_loaded.disconnect();
}

std::shared_ptr<vfs::file>
gui::files_base::get_item(u32 position) const noexcept
{
    auto col = dir_model_->get_item(position.data());
    if (!col)
    {
        return nullptr;
    }
    return col->file;
}

[[nodiscard]] std::vector<std::shared_ptr<vfs::file>>
gui::files_base::selected_files() const noexcept
{
    auto selection = selection_model_->get_selection();
    if (selection->is_empty())
    {
        return {};
    }

    std::vector<std::shared_ptr<vfs::file>> selected;
    const auto n_items = dir_model_->get_n_items();
    for (std::uint32_t i = 0; i < n_items; ++i)
    {
        if (selection_model_->is_selected(i))
        {
            selected.push_back(get_item(i));
        }
    }
    return selected;
}

std::int32_t
gui::files_base::model_sort(const Glib::RefPtr<const ModelColumns>& a,
                            const Glib::RefPtr<const ModelColumns>& b) const noexcept
{
    const auto lhs = a->file;
    const auto rhs = b->file;

    std::int32_t result{0};

    if (sorting_.sort_dir != config::sort_dir::mixed)
    {
        result = lhs->is_directory() - rhs->is_directory();
        if (result != 0)
        {
            return sorting_.sort_dir == config::sort_dir::first ? -result : result;
        }
    }

    if (lhs->is_hidden() || rhs->is_hidden())
    {
        result = lhs->is_hidden() - rhs->is_hidden();
        if (result != 0)
        {
            return sorting_.sort_hidden == config::sort_hidden::first ? -result : result;
        }
    }

    switch (sorting_.sort_by)
    {
        case config::sort_by::name:
        {
            if (sorting_.sort_natural)
            {
                result = strnatcmp(lhs->name(), rhs->name(), sorting_.sort_case);
            }
            else
            {
                result = lhs->name().compare(rhs->name());
            }
            break;
        }
        case config::sort_by::size:
        case config::sort_by::bytes:
        {
            result = (lhs->size() > rhs->size()) ? 1 : (lhs->size() == rhs->size()) ? 0 : -1;
            break;
        }
        case config::sort_by::type:
        {
            result = lhs->mime_type()->type().compare(rhs->mime_type()->type());
            break;
        }
        case config::sort_by::mime:
        {
            result = lhs->mime_type()->description().compare(rhs->mime_type()->description());
            break;
        }
        case config::sort_by::perm:
        {
            result = lhs->display_permissions().compare(rhs->display_permissions());
            break;
        }
        case config::sort_by::owner:
        {
            result = lhs->display_owner().compare(rhs->display_owner());
            break;
        }
        case config::sort_by::group:
        {
            result = lhs->display_group().compare(rhs->display_group());
            break;
        }
        case config::sort_by::atime:
        {
            result = (lhs->atime() > rhs->atime()) ? 1 : (lhs->atime() == rhs->atime()) ? 0 : -1;
            break;
        }
        case config::sort_by::btime:
        {
            result = (lhs->btime() > rhs->btime()) ? 1 : (lhs->btime() == rhs->btime()) ? 0 : -1;
            break;
        }
        case config::sort_by::ctime:
        {
            result = (lhs->ctime() > rhs->ctime()) ? 1 : (lhs->ctime() == rhs->ctime()) ? 0 : -1;
            break;
        }
        case config::sort_by::mtime:
        {
            result = (lhs->mtime() > rhs->mtime()) ? 1 : (lhs->mtime() == rhs->mtime()) ? 0 : -1;
            break;
        }
    };

    return sorting_.sort_type == config::sort_type::ascending ? result : -result;
}

void
gui::files_base::update() noexcept
{
    dir_model_->remove_all();

    std::vector<Glib::RefPtr<ModelColumns>> items;
    items.reserve(dir_->files().size());
    for (const auto& file : dir_->files())
    {
        if ((sorting_.show_hidden || !file->is_hidden()) && is_pattern_match(file->name()))
        {
            items.push_back(ModelColumns::create(file));
        }
    }
    dir_model_->splice(0, 0, items);

    sort();
}

void
gui::files_base::sort() noexcept
{
    dir_model_->sort(sigc::mem_fun(*this, &files_base::model_sort));
}

bool
gui::files_base::is_selected() const noexcept
{
    auto selected = selection_model_->get_selection();
    return !selected->is_empty();
}

void
gui::files_base::select_all() const noexcept
{
    selection_model_->select_all();
}

void
gui::files_base::unselect_all() const noexcept
{
    selection_model_->unselect_all();
}

void
gui::files_base::select_file(const std::filesystem::path& filename,
                             const bool unselect_others) const noexcept
{
    if (unselect_others)
    {
        unselect_all();
    }

    const auto n_items = dir_model_->get_n_items();
    for (std::uint32_t i = 0; i < n_items; ++i)
    {
        const auto file = get_item(i);
        if (file->name() == filename)
        {
            selection_model_->select_item(i, false);
            break;
        }
    }
}

void
gui::files_base::select_files(
    const std::span<const std::filesystem::path> select_filenames) const noexcept
{
    unselect_all();

    const auto n_items = dir_model_->get_n_items();
    for (std::uint32_t i = 0; i < n_items; ++i)
    {
        const auto file = get_item(i);
        if (std::ranges::contains(select_filenames, file->name()))
        {
            selection_model_->select_item(i, false);
        }
    }
}

void
gui::files_base::unselect_file(const std::filesystem::path& filename) const noexcept
{
    const auto n_items = dir_model_->get_n_items();
    for (std::uint32_t i = 0; i < n_items; ++i)
    {
        const auto file = get_item(i);
        if (file->name() == filename)
        {
            // Do not need to check if file is actually selected
            selection_model_->unselect_item(i);
            break;
        }
    }
}

void
gui::files_base::select_pattern(const std::string_view search_key) noexcept
{
    unselect_all();

    if (search_key.empty())
    {
        return;
    }

    const auto n_items = dir_model_->get_n_items();
    for (std::uint32_t i = 0; i < n_items; ++i)
    {
        const auto file = get_item(i);
        const bool select = (fnmatch(search_key.data(), file->name().data(), 0) == 0);
        if (select)
        {
            selection_model_->select_item(i, false);
        }
    }
}

void
gui::files_base::invert_selection() noexcept
{
    const auto n_items = dir_model_->get_n_items();
    for (std::uint32_t i = 0; i < n_items; ++i)
    {
        if (selection_model_->is_selected(i))
        {
            selection_model_->unselect_item(i);
        }
        else
        {
            selection_model_->select_item(i, false);
        }
    }
}

void
gui::files_base::set_dir(const std::shared_ptr<vfs::dir>& dir, const config::sorting& sorting,
                         const config::columns& columns) noexcept
{
    if (!dir || dir_ == dir)
    {
        return;
    }

    signal_files_changed.disconnect();
    signal_files_created.disconnect();
    signal_files_deleted.disconnect();
    signal_thumbnail_loaded.disconnect();

    dir_ = dir;
    sorting_ = sorting;
    columns_ = columns;

    signal_files_changed = dir_->signal_files_changed().connect([this](const auto& files)
                                                                { on_files_changed(files); });
    signal_files_created = dir_->signal_files_created().connect([this](const auto& files)
                                                                { on_files_created(files); });
    signal_files_deleted = dir_->signal_files_deleted().connect([this](const auto& files)
                                                                { on_files_deleted(files); });

    signal_thumbnail_loaded =
        dir_->signal_thumbnail_loaded().connect([this](auto f) { on_thumbnail_loaded(f); });

    signal_dir_loaded().emit();
}

void
gui::files_base::set_thumbnail_size(const vfs::file::thumbnail_size size) noexcept
{
    thumbnail_size_ = size;
}

void
gui::files_base::set_pattern(const std::string_view pattern) noexcept
{
    pattern_ = pattern.data();
}

void
gui::files_base::set_sorting(const config::sorting& sorting, bool full_update) noexcept
{
    sorting_ = sorting;

    if (full_update)
    {
        update();
    }
    else
    {
        sort();
    }

    signal_update_sorting().emit();
}

void
gui::files_base::set_columns(const config::columns& columns) noexcept
{
    columns_ = columns;

    update();

    signal_update_columns().emit();
}

bool
gui::files_base::is_pattern_match(const std::filesystem::path& filename) const noexcept
{
    if (pattern_.empty())
    {
        return true;
    }
    return fnmatch(pattern_.data(), filename.c_str(), 0) == 0;
}

std::pair<bool, std::uint32_t>
gui::files_base::find_file(const std::shared_ptr<vfs::file>& file) noexcept
{
    if (!dir_model_ || !file)
    {
        // mimicking Gio::ListStore::find()
        return {false, std::numeric_limits<std::uint32_t>::max()};
    }

    const auto n_items = dir_model_->get_n_items();
    for (std::uint32_t i = 0; i < n_items; ++i)
    {
        auto item = dir_model_->get_item(i);
        if (item && item->file == file)
        {
            return {true, i};
        }
    }

    return {false, std::numeric_limits<std::uint32_t>::max()};
}

void
gui::files_base::on_files_created(const std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    // logger::debug("gui::grid::on_files_created({})", files.size());

    for (const auto& file : files)
    {
        if ((!sorting_.show_hidden && file->is_hidden()) || !is_pattern_match(file->name()))
        {
            continue;
        }

#if 0
        // this seems to work without needing to use the idle handler
        dir_model_->insert_sorted(ModelColumns::create(file),
                                  sigc::mem_fun(*this, &grid::model_sort));
#else
        Glib::signal_idle().connect_once(
            [this, file]()
            {
                dir_model_->insert_sorted(ModelColumns::create(file),
                                          sigc::mem_fun(*this, &files_base::model_sort));
            },
            Glib::PRIORITY_DEFAULT);
#endif

        if (enable_thumbnail_ && (file->mime_type()->is_video() || file->mime_type()->is_image()))
        {
            if (!file->is_thumbnail_loaded(thumbnail_size_))
            {
                dir_->load_thumbnail(file, thumbnail_size_);
            }
        }
    }
}

void
gui::files_base::on_files_deleted(const std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    // logger::debug("gui::grid::on_files_deleted({})", files.size());

    for (const auto& file : files)
    {
        if ((!sorting_.show_hidden && file->is_hidden()) || !is_pattern_match(file->name()))
        {
            continue;
        }
    }

    for (const auto& file : files)
    {
        const auto [found, position] = find_file(file);
        if (found)
        {
#if 0
            // this seems to work without needing to use the idle handler
            dir_model_->remove(position);
#else
            Glib::signal_idle().connect_once([this, position]() { dir_model_->remove(position); },
                                             Glib::PRIORITY_DEFAULT);
#endif
        }
    }
}

void
gui::files_base::on_files_changed(const std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    // logger::debug("gui::grid::on_files_changed({})", files.size());

    if (!dir_ || dir_->is_loading())
    {
        return;
    }

    for (const auto& file : files)
    {
        if ((!sorting_.show_hidden && file->is_hidden()) || !is_pattern_match(file->name()))
        {
            continue;
        }

        const auto [found, position] = find_file(file);
        if (found)
        {
#if 0
            auto item = dir_model_->get_item(position);
            item->signal_changed().emit();
#else
            Glib::signal_idle().connect_once(
                [this, file, position]()
                {
                    auto item = dir_model_->get_item(position);
                    item->signal_changed().emit();
                },
                Glib::PRIORITY_DEFAULT);
#endif
        }

        const auto now = std::chrono::system_clock::now();
        if (enable_thumbnail_ && (now - file->mtime() > std::chrono::seconds(5)) &&
            (file->mime_type()->is_video() || file->mime_type()->is_image()))
        {
            if (!file->is_thumbnail_loaded(thumbnail_size_))
            {
                dir_->load_thumbnail(file, thumbnail_size_);
            }
        }
    }
}

void
gui::files_base::on_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept
{
    // logger::debug("gui::grid::on_thumbnail_loaded({})", file->path().string());

    const auto [found, position] = find_file(file);
    if (found)
    {
#if 0
        auto item = dir_model_->get_item(position);
        item->signal_thumbnail_loaded().emit();
#else
        Glib::signal_idle().connect_once(
            [this, file, position]()
            {
                auto item = dir_model_->get_item(position);
                item->signal_thumbnail_loaded().emit();
            },
            Glib::PRIORITY_DEFAULT);
#endif
    }
}

void
gui::files_base::enable_thumbnails() noexcept
{
    if (dir_)
    {
        dir_->enable_thumbnails(true);
        dir_->load_thumbnails(thumbnail_size_);
    }

    // just regen the whole model
    update();
}

void
gui::files_base::disable_thumbnails() noexcept
{
    if (dir_)
    {
        dir_->enable_thumbnails(false);
        dir_->unload_thumbnails(thumbnail_size_);
    }

    // just regen the whole model
    update();
}
