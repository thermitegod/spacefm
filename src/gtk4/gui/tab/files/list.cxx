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
#include <memory>

#include <fnmatch.h>

#include <gdkmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/tab/files/list.hxx"

#include "logger.hxx"
#include "natsort/strnatcmp.hxx"

gui::list::list(const config::list_state& state, const std::shared_ptr<config::settings>& settings)
    : files_base(settings)
{
    list_state_ = state;

    set_enable_rubberband(true);
    set_single_click_activate(settings_->general.single_click_activate);
    // set_expand(true);

    set_reorderable(false);

    if (list_state_.compact)
    {
        add_css_class("data-table");
    }

    set_model(selection_model_);

    add_columns();

    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_PRIMARY);
    gesture->set_propagation_phase(Gtk::PropagationPhase::BUBBLE);
    gesture->signal_released().connect(sigc::mem_fun(*this, &list::on_background_click));
    add_controller(gesture);

    drag_source_ = Gtk::DragSource::create();
    drag_source_->set_actions(Gdk::DragAction::COPY | Gdk::DragAction::MOVE);
    drag_source_->signal_prepare().connect(sigc::mem_fun(*this, &list::on_drag_prepare), false);
    add_controller(drag_source_);

    drop_target_ =
        Gtk::DropTarget::create(GDK_TYPE_FILE_LIST, Gdk::DragAction::COPY | Gdk::DragAction::MOVE);
    drop_target_->signal_drop().connect(sigc::mem_fun(*this, &list::on_drag_data_received), false);
    drop_target_->signal_motion().connect(sigc::mem_fun(*this, &list::on_drag_motion), false);
    add_controller(drop_target_);

    signal_dir_loaded().connect(
        [this]()
        {
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
                },
                Glib::PRIORITY_DEFAULT);
        });

    signal_update_view_list().connect([this]() { update_list_visibility(); });
}

void
gui::list::add_columns() noexcept
{
    { // name
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_name), Gtk::Align::START));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_name));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_name));
        column_name_ = Gtk::ColumnViewColumn::create("Name", factory);
        column_name_->set_expand(true);
        append_column(column_name_);
    }

    { // size
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_size));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_size_ = Gtk::ColumnViewColumn::create("Size", factory);
        column_size_->set_expand(false);
        append_column(column_size_);
    }

    { // bytes
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_bytes));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_bytes_ = Gtk::ColumnViewColumn::create("Bytes", factory);
        column_bytes_->set_expand(false);
        append_column(column_bytes_);
    }

    { // type
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_type));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_type_ = Gtk::ColumnViewColumn::create("Type", factory);
        column_type_->set_expand(false);
        append_column(column_type_);
    }

    { // mime
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_mime));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_mime_ = Gtk::ColumnViewColumn::create("Mime", factory);
        column_mime_->set_expand(false);
        append_column(column_mime_);
    }

    { // perm
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_perm));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_perm_ = Gtk::ColumnViewColumn::create("Permissions", factory);
        column_perm_->set_expand(false);
        append_column(column_perm_);
    }

    { // owner
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_owner));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_owner_ = Gtk::ColumnViewColumn::create("Owner", factory);
        column_owner_->set_expand(false);
        append_column(column_owner_);
    }

    { // group
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_group));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_group_ = Gtk::ColumnViewColumn::create("Group", factory);
        column_group_->set_expand(false);
        append_column(column_group_);
    }

    { // atime
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_atime));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_atime_ = Gtk::ColumnViewColumn::create("Date Accessed", factory);
        column_atime_->set_expand(false);
        append_column(column_atime_);
    }

    { // btime
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_btime));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_btime_ = Gtk::ColumnViewColumn::create("Date Created", factory);
        column_btime_->set_expand(false);
        append_column(column_btime_);
    }

    { // ctime
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_ctime));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_ctime_ = Gtk::ColumnViewColumn::create("Date Metadata", factory);
        column_ctime_->set_expand(false);
        append_column(column_ctime_);
    }

    { // mtime
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            sigc::bind(sigc::mem_fun(*this, &list::on_setup_label), Gtk::Align::END));
        factory->signal_bind().connect(sigc::mem_fun(*this, &list::on_bind_mtime));
        factory->signal_unbind().connect(sigc::mem_fun(*this, &list::on_unbind_item));
        column_mtime_ = Gtk::ColumnViewColumn::create("Date Modified", factory);
        column_mtime_->set_expand(false);
        append_column(column_mtime_);
    }

    update_list_visibility();
}

void
gui::list::on_setup_name(const Glib::RefPtr<Gtk::ListItem>& item, Gtk::Align halign) noexcept
{
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    auto* image = Gtk::make_managed<Gtk::Image>();
    auto* label = Gtk::make_managed<Gtk::Label>();

    box->set_expand(true);
    box->set_can_target(true);
    box->set_focusable(true);

    image->set_icon_size(Gtk::IconSize::NORMAL);

    label->set_wrap(false);
    label->set_halign(halign);
    label->set_margin_start(5);

    box->append(*image);
    box->append(*label);
    item->set_child(*box);
}

void
gui::list::on_setup_label(const Glib::RefPtr<Gtk::ListItem>& item, Gtk::Align halign) noexcept
{
    item->set_child(*Gtk::make_managed<Gtk::Label>("", halign));
}

void
gui::list::on_bind_name(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
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

    auto update_image = [this, image, col]()
    {
        if (!image || !col)
        {
            return;
        }

        // DEV ignore thumbs
        // image->set_from_icon_name(col->file->is_directory() ? "folder" : "text-x-generic");
        image->set(col->file->icon(list_state_.icon_size));
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
gui::list::on_bind_size(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->display_size().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_bytes(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->display_size_in_bytes().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_type(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->mime_type()->description().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_mime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->mime_type()->type().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_perm(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->display_permissions().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_owner(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->display_owner().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_group(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->display_group().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_atime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->display_atime().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_btime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->display_btime().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_ctime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->display_ctime().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_bind_mtime(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());

    auto update_label = [label, col]()
    {
        if (!label || !col)
        {
            return;
        }

        label->set_text(col->file->display_mtime().data());
    };

    update_label();

    auto connection = std::make_unique<sigc::connection>();
    col->signal_changed().connect(update_label);

    item->set_data("connection",
                   connection.release(),
                   [](void* data) { delete static_cast<sigc::connection*>(data); });
}

void
gui::list::on_unbind_name(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
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
gui::list::on_unbind_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* connection = static_cast<sigc::connection*>(item->get_data("connection"));
    if (connection)
    {
        connection->disconnect();
    }
    item->set_data("connection", nullptr);
}

void
gui::list::on_background_click(std::int32_t n_press, double x, double y) noexcept
{
    if (n_press != 1)
    {
        return;
    }

    Gtk::Widget* target = pick(x, y);
    if (target == this ||
        (target && std::string_view(G_OBJECT_TYPE_NAME(target->gobj())) == "GtkColumnView"))
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
gui::list::on_drag_prepare(double x, double y) const noexcept
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
gui::list::on_drag_data_received(const Glib::ValueBase& value, double x, double y) noexcept
{
    Gtk::Widget* target = pick(x, y);
    if (target && target != this &&
        std::string_view(G_OBJECT_TYPE_NAME(target->gobj())) != "GtkColumnView")
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
gui::list::on_drag_motion(double x, double y) noexcept
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

void
gui::list::update_list_visibility() noexcept
{
    if (list_state_.compact)
    {
        add_css_class("data-table");
    }
    else
    {
        remove_css_class("data-table");
    }

    column_name_->set_visible(list_state_.name);
    column_size_->set_visible(list_state_.size);
    column_bytes_->set_visible(list_state_.bytes);
    column_type_->set_visible(list_state_.type);
    column_mime_->set_visible(list_state_.mime);
    column_perm_->set_visible(list_state_.perm);
    column_owner_->set_visible(list_state_.owner);
    column_group_->set_visible(list_state_.group);
    column_atime_->set_visible(list_state_.atime);
    column_btime_->set_visible(list_state_.btime);
    column_ctime_->set_visible(list_state_.ctime);
    column_mtime_->set_visible(list_state_.mtime);
}
