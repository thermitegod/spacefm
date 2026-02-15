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

#include "gui/tab/files.hxx"

#include "logger.hxx"
#include "natsort/strnatcmp.hxx"

gui::files::files(const std::shared_ptr<config::settings>& settings) : settings_(settings)
{
    set_enable_rubberband(true);
    set_single_click_activate(settings_->general.single_click_activate);
    // set_expand(true);

    // set_halign(Gtk::Align::START);
    set_min_columns(1);
    // set_max_columns(10);
    set_max_columns(1000);

    auto attr = Pango::Attribute::create_attr_insert_hyphens(false);
    attrs_.insert(attr);

    dir_model_ = Gio::ListStore<ModelColumns>::create();
    selection_model_ = Gtk::MultiSelection::create(dir_model_);

    factory_ = Gtk::SignalListItemFactory::create();
    factory_->signal_setup().connect(sigc::mem_fun(*this, &files::on_setup_listitem));
    factory_->signal_bind().connect(sigc::mem_fun(*this, &files::on_bind_listitem));
    factory_->signal_unbind().connect(sigc::mem_fun(*this, &files::on_unbind_listitem));

    set_model(selection_model_);
    set_factory(factory_);

    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_PRIMARY);
    gesture->set_propagation_phase(Gtk::PropagationPhase::BUBBLE);
    gesture->signal_released().connect(sigc::mem_fun(*this, &files::on_background_click));
    add_controller(gesture);

    drag_source_ = Gtk::DragSource::create();
    drag_source_->set_actions(Gdk::DragAction::COPY | Gdk::DragAction::MOVE);
    drag_source_->signal_prepare().connect(sigc::mem_fun(*this, &files::on_drag_prepare), false);
    add_controller(drag_source_);

    drop_target_ =
        Gtk::DropTarget::create(GDK_TYPE_FILE_LIST, Gdk::DragAction::COPY | Gdk::DragAction::MOVE);
    drop_target_->signal_drop().connect(sigc::mem_fun(*this, &files::on_drag_data_received), false);
    drop_target_->signal_motion().connect(sigc::mem_fun(*this, &files::on_drag_motion), false);
    add_controller(drop_target_);
}

gui::files::~files()
{
    signal_files_changed.disconnect();
    signal_files_created.disconnect();
    signal_files_deleted.disconnect();
    signal_thumbnail_loaded.disconnect();
}

void
gui::files::on_setup_listitem(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    auto* image = Gtk::make_managed<Gtk::Image>();
    auto* label = Gtk::make_managed<Gtk::Label>();

    box->set_expand(true);
    // TODO this fixes the item resizing with a window
    // resize. not the best way to do this.
    box->set_size_request(80, 80);
    // box->set_focusable(false);
    box->set_can_target(true);
    box->set_focusable(true);

    image->set_icon_size(Gtk::IconSize::LARGE);
    // box->set_focusable(true);

    // label->set_expand(false);
    label->set_attributes(attrs_);
    label->set_wrap(true);
    label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    label->set_justify(Gtk::Justification::CENTER);
    label->set_xalign(0.5f);
    label->set_yalign(0.0f);
    // box->set_focusable(true);

    box->append(*image);
    box->append(*label);
    item->set_child(*box);
}

void
gui::files::on_bind_listitem(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* box = dynamic_cast<Gtk::Box*>(item->get_child());
    auto* image = dynamic_cast<Gtk::Image*>(box->get_first_child());
    auto* label = dynamic_cast<Gtk::Label*>(image->get_next_sibling());

    auto connections = std::make_unique<std::vector<sigc::connection>>();

    if (col->file->is_directory())
    {
        col->drop_target = Gtk::DropTarget::create(GDK_TYPE_FILE_LIST,
                                                   Gdk::DragAction::COPY | Gdk::DragAction::MOVE);
        col->drop_target->signal_drop().connect(
            [this, col](const Glib::ValueBase& value, double, double)
            {
                (void)this;

                Glib::Value<GSList*> gslist_value;
                gslist_value.init(value.gobj());
                auto files = Glib::SListHandler<Glib::RefPtr<Gio::File>>::slist_to_vector(
                    gslist_value.get(),
                    Glib::OwnershipType::OWNERSHIP_NONE);
                for (const auto& file : files)
                {
                    Glib::RefPtr<Gio::File> destination =
                        Gio::File::create_for_path(col->file->path() / file->get_basename());

                    logger::debug<logger::gui>("Source: {}", file->get_path());
                    logger::debug<logger::gui>("Target: {}", col->file->path().string());

                    // TODO file actions
                }
                return true;
            },
            false);
        box->add_controller(col->drop_target);
    }

    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_SECONDARY);
    gesture->signal_pressed().connect(
        [this, item](std::int32_t n_press, double x, double y)
        {
            (void)n_press;
            (void)x;
            (void)y;

            const auto position = item->get_position();

            if (!is_selected() || !selection_model_->is_selected(position))
            {
                // no items are currently selected, select this item.
                // or
                // right click is not over one of the currently selected
                // items, unselect those and select this item.
                selection_model_->select_item(position, true);
            }
        },
        false);
    box->add_controller(gesture);

    item->set_selectable(true);
    label->set_text(col->file->name().data());

    auto update_image = [image, col]()
    {
        if (!image || !col)
        {
            return;
        }

        Glib::RefPtr<Gdk::Paintable> icon;
        if (col->file->is_thumbnail_loaded(vfs::file::thumbnail_size::big))
        {
            icon = col->file->thumbnail(vfs::file::thumbnail_size::big);
        }
        else
        {
            icon = col->file->icon(vfs::file::thumbnail_size::big);
        }

        // DEV ignore thumbs
        // image->set_from_icon_name(col->file->is_directory() ? "folder" : "text-x-generic");

        image->set(icon);
    };

    auto update_label = [label, col]()
    {
        // TODO figure out what needs to be updated on a file change event.
        // this is more needed for detailed/compact view mode
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->name().data());
    };

    update_image();
    update_label();

    connections->push_back(col->signal_thumbnail_loaded().connect(update_image));
    connections->push_back(col->signal_changed().connect(update_label));

    item->set_data("connections",
                   connections.release(),
                   [](void* data) { delete static_cast<std::vector<sigc::connection>*>(data); });
}

void
gui::files::on_unbind_listitem(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* box = dynamic_cast<Gtk::Box*>(item->get_child());
    // auto* image = dynamic_cast<Gtk::Image*>(box->get_first_child());
    // auto* label = dynamic_cast<Gtk::Label*>(image->get_next_sibling());

    if (col->drop_target)
    {
        box->remove_controller(col->drop_target);
    }

    auto* connections = static_cast<std::vector<sigc::connection>*>(item->get_data("connections"));
    if (connections)
    {
        for (auto& connection : *connections)
        {
            connection.disconnect();
        }
    }
    item->set_data("connections", nullptr);
}

void
gui::files::on_background_click(std::int32_t n_press, double x, double y) noexcept
{
    if (n_press != 1)
    {
        return;
    }

    Gtk::Widget* target = pick(x, y);
    if (target == this ||
        (target && std::string_view(G_OBJECT_TYPE_NAME(target->gobj())) == "GtkGridView"))
    {
        const auto selection = selection_model_->get_selection();
        if (!selection->is_empty())
        {
            selection_model_->unselect_all();
            grab_focus();
        }
    }
}

Glib::RefPtr<Gdk::ContentProvider>
gui::files::on_drag_prepare(double x, double y) const noexcept
{
    (void)x;
    (void)y;

    auto selected = selected_files();
    if (selected.empty())
    {
        return nullptr;
    }

    std::vector<GFile*> files;
    files.reserve(selected.size());
    for (const auto& file : selected)
    {
        files.push_back(g_file_new_for_path(file->path().c_str()));
    }
    GdkFileList* file_list = gdk_file_list_new_from_array(files.data(), files.size());
    std::ranges::for_each(files, g_object_unref);

    GdkContentProvider* provider = gdk_content_provider_new_typed(GDK_TYPE_FILE_LIST, file_list);
    return Glib::wrap(provider);
}

bool
gui::files::on_drag_data_received(const Glib::ValueBase& value, double x, double y) noexcept
{
    Gtk::Widget* target = pick(x, y);
    if (target && target != this &&
        std::string_view(G_OBJECT_TYPE_NAME(target->gobj())) != "GtkGridView")
    {
        return false;
    }

    Glib::Value<GSList*> gslist_value;
    gslist_value.init(value.gobj());
    auto files = Glib::SListHandler<Glib::RefPtr<Gio::File>>::slist_to_vector(
        gslist_value.get(),
        Glib::OwnershipType::OWNERSHIP_NONE);
    for (const auto& file : files)
    {
        Glib::RefPtr<Gio::File> destination =
            Gio::File::create_for_path(dir_->path() / file->get_basename());

        logger::debug<logger::gui>("Source: {}", file->get_path());
        logger::debug<logger::gui>("Target: {}", dir_->path().string());

        // TODO file actions
    }

    return true;
}

Gdk::DragAction
gui::files::on_drag_motion(double x, double y) noexcept
{
    Gtk::Widget* target = pick(x, y);
    Gtk::Widget* current = target;
    while (current && current != this)
    {
        auto controllers = current->observe_controllers();
        for (std::uint32_t i = 0; i < g_list_model_get_n_items(controllers->gobj()); ++i)
        {
            auto* controller = GTK_EVENT_CONTROLLER(g_list_model_get_item(controllers->gobj(), i));
            if (GTK_IS_DROP_TARGET(controller))
            {
                g_object_unref(controller);
                return Gdk::DragAction::NONE;
            }
            g_object_unref(controller);
        }
        current = current->get_parent();
    }
    // requires a unique preferred action
    // return Gdk::DragAction::COPY | Gdk::DragAction::MOVE;
    return Gdk::DragAction::MOVE;
}

std::shared_ptr<vfs::file>
gui::files::get_item(u32 position) const noexcept
{
    auto col = dir_model_->get_item(position.data());
    if (!col)
    {
        return nullptr;
    }
    return col->file;
}

[[nodiscard]] std::vector<std::shared_ptr<vfs::file>>
gui::files::selected_files() const noexcept
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
gui::files::model_sort(const Glib::RefPtr<const ModelColumns>& a,
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
gui::files::update() noexcept
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
gui::files::sort() noexcept
{
    dir_model_->sort(sigc::mem_fun(*this, &files::model_sort));
}

bool
gui::files::is_selected() const noexcept
{
    auto selected = selection_model_->get_selection();
    return !selected->is_empty();
}

void
gui::files::select_all() const noexcept
{
    selection_model_->select_all();
}

void
gui::files::unselect_all() const noexcept
{
    selection_model_->unselect_all();
}

void
gui::files::select_file(const std::filesystem::path& filename,
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
gui::files::select_files(
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
gui::files::unselect_file(const std::filesystem::path& filename) const noexcept
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
gui::files::select_pattern(const std::string_view search_key) noexcept
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
gui::files::invert_selection() noexcept
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
gui::files::set_dir(const std::shared_ptr<vfs::dir>& dir, const config::sorting& sorting) noexcept
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

    signal_files_changed = dir_->signal_files_changed().connect([this](const auto& files)
                                                                { on_files_changed(files); });
    signal_files_created = dir_->signal_files_created().connect([this](const auto& files)
                                                                { on_files_created(files); });
    signal_files_deleted = dir_->signal_files_deleted().connect([this](const auto& files)
                                                                { on_files_deleted(files); });

    signal_thumbnail_loaded =
        dir_->signal_thumbnail_loaded().connect([this](auto f) { on_thumbnail_loaded(f); });

    Glib::signal_idle().connect_once(
        [this]()
        {
            update();

            if (dir_model_->get_n_items() > 0)
            {
                // Start at the top of the view,
                // otherwise will be at the bottom of the view
                scroll_to(0);
            }

            dir_->load_thumbnails(vfs::file::thumbnail_size::big);
        },
        Glib::PRIORITY_DEFAULT);
}

void
gui::files::set_thumbnail_size(const vfs::file::thumbnail_size size) noexcept
{
    thumbnail_size_ = size;
}

void
gui::files::set_pattern(const std::string_view pattern) noexcept
{
    pattern_ = pattern.data();
}

void
gui::files::set_sorting(const config::sorting& sorting, bool full_update) noexcept
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
}

bool
gui::files::is_pattern_match(const std::filesystem::path& filename) const noexcept
{
    if (pattern_.empty())
    {
        return true;
    }
    return fnmatch(pattern_.data(), filename.c_str(), 0) == 0;
}

std::pair<bool, std::uint32_t>
gui::files::find_file(const std::shared_ptr<vfs::file>& file) noexcept
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
gui::files::on_files_created(const std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    // logger::debug("gui::files::on_files_created({})", files.size());

    for (const auto& file : files)
    {
        if ((!sorting_.show_hidden && file->is_hidden()) || !is_pattern_match(file->name()))
        {
            continue;
        }

#if 0
        // this seems to work without needing to use the idle handler
        dir_model_->insert_sorted(ModelColumns::create(file),
                                  sigc::mem_fun(*this, &files::model_sort));
#else
        Glib::signal_idle().connect_once(
            [this, file]()
            {
                dir_model_->insert_sorted(ModelColumns::create(file),
                                          sigc::mem_fun(*this, &files::model_sort));
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
gui::files::on_files_deleted(const std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    // logger::debug("gui::files::on_files_deleted({})", files.size());

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
gui::files::on_files_changed(const std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    // logger::debug("gui::files::on_files_changed({})", files.size());

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
gui::files::on_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept
{
    // logger::debug("gui::files::on_thumbnail_loaded({})", file->path().string());

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
gui::files::enable_thumbnails() noexcept
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
gui::files::disable_thumbnails() noexcept
{
    if (dir_)
    {
        dir_->enable_thumbnails(false);
        dir_->unload_thumbnails(thumbnail_size_);
    }

    // just regen the whole model
    update();
}
