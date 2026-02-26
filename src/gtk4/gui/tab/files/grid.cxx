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
#include <utility>

#include <fnmatch.h>

#include <gdkmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/tab/files/grid.hxx"

#include "vfs/task-manager.hxx"

#include "logger.hxx"
#include "natsort/strnatcmp.hxx"

#define LAYOUT_TESTING

gui::grid::grid(const config::grid_state& state,
                const std::shared_ptr<vfs::task_manager>& task_manager,
                const std::shared_ptr<config::settings>& settings)
    : files_base(task_manager, settings)
{
    grid_state_ = state;

    set_enable_rubberband(true);
    set_single_click_activate(settings_->general.single_click_activate);
    // set_expand(true);

    // set_halign(Gtk::Align::START);
    set_min_columns(1);
    // set_max_columns(10);
    set_max_columns(1000);

    auto attr = Pango::Attribute::create_attr_insert_hyphens(false);
    attrs_.insert(attr);

    set_model(selection_model_);

    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(sigc::mem_fun(*this, &grid::on_setup_item));
    factory->signal_bind().connect(sigc::mem_fun(*this, &grid::on_bind_item));
    factory->signal_unbind().connect(sigc::mem_fun(*this, &grid::on_unbind_item));
    set_factory(factory);

    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_PRIMARY);
    gesture->set_propagation_phase(Gtk::PropagationPhase::BUBBLE);
    gesture->signal_released().connect(sigc::mem_fun(*this, &grid::on_background_click));
    add_controller(gesture);

    drag_source_ = Gtk::DragSource::create();
    drag_source_->set_actions(Gdk::DragAction::COPY | Gdk::DragAction::MOVE);
    drag_source_->signal_prepare().connect(sigc::mem_fun(*this, &grid::on_drag_prepare), false);
    add_controller(drag_source_);

    drop_target_ =
        Gtk::DropTarget::create(GDK_TYPE_FILE_LIST, Gdk::DragAction::COPY | Gdk::DragAction::MOVE);
    drop_target_->signal_drop().connect(sigc::mem_fun(*this, &grid::on_drag_data_received), false);
    drop_target_->signal_motion().connect(sigc::mem_fun(*this, &grid::on_drag_motion), false);
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

                    if (grid_state_.thumbnails)
                    {
                        dir_->load_thumbnails(std::to_underlying(grid_state_.icon_size));
                    }
                },
                Glib::PRIORITY_DEFAULT);
        });

    signal_update_view_state().connect(
        [this]()
        {
            if (grid_state_.thumbnails)
            {
                dir_->load_thumbnails(std::to_underlying(grid_state_.icon_size));
            }
        });
}

void
gui::grid::on_setup_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
#if defined(LAYOUT_TESTING)
    auto* picture_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
#endif
    auto* picture = Gtk::make_managed<Gtk::Picture>();
    auto* label = Gtk::make_managed<Gtk::Label>();

    const auto size = std::to_underlying(grid_state_.icon_size);

    box->set_size_request(size, size);
    box->set_expand(true);
    box->set_can_target(true);
    box->set_focusable(true);

    picture->set_size_request(size, size);
    picture->set_content_fit(Gtk::ContentFit::SCALE_DOWN);
    picture->set_can_shrink(false);
    picture->set_halign(Gtk::Align::CENTER);
    picture->set_valign(Gtk::Align::CENTER);
    picture->set_hexpand(false);
    picture->set_vexpand(false);

    label->set_attributes(attrs_);
    label->set_wrap(true);
    label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    label->set_justify(Gtk::Justification::CENTER);
    label->set_halign(Gtk::Align::CENTER);
    label->set_valign(Gtk::Align::START);

#if defined(LAYOUT_TESTING)
    box->append(*picture_box);
    picture_box->append(*picture);
#else
    box->append(*picture);
#endif
    box->append(*label);
    item->set_child(*box);
}

void
gui::grid::on_bind_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* box = dynamic_cast<Gtk::Box*>(item->get_child());
#if defined(LAYOUT_TESTING)
    auto* picture_box = dynamic_cast<Gtk::Box*>(box->get_first_child());
    auto* picture = dynamic_cast<Gtk::Picture*>(picture_box->get_first_child());
    auto* label = dynamic_cast<Gtk::Label*>(picture_box->get_next_sibling());
#else
    auto* picture = dynamic_cast<Gtk::Picture*>(box->get_first_child());
    auto* label = dynamic_cast<Gtk::Label*>(picture->get_next_sibling());
#endif

    auto connections = std::make_unique<std::vector<sigc::connection>>();

    if (col->file->is_directory())
    {
        col->drop_target = Gtk::DropTarget::create(GDK_TYPE_FILE_LIST,
                                                   Gdk::DragAction::COPY | Gdk::DragAction::MOVE);
        col->drop_target->signal_drop().connect(
            [this, col](const Glib::ValueBase& value, double, double)
            {
                Glib::Value<GSList*> gslist_value;
                gslist_value.init(value.gobj());
                auto files = Glib::SListHandler<Glib::RefPtr<Gio::File>>::slist_to_vector(
                    gslist_value.get(),
                    Glib::OwnershipType::OWNERSHIP_NONE);
                for (const auto& file : files)
                {
                    Glib::RefPtr<Gio::File> destination =
                        Gio::File::create_for_path(col->file->path() / file->get_basename());

                    // logger::debug<logger::gui>("DnD Source: {}", file->get_path());
                    // logger::debug<logger::gui>("DnD Target: {}", col->file->path().string());

                    // TODO smart, move if on same fs, copy otherwise
                    auto task = vfs::move_task{
                        .options = {},
                        .source = file->get_path(),
                        .destination = col->file->path(),
                    };
                    task_manager_->add(task);
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

    auto update_image = [this, box, picture, col]()
    {
        if (!picture || !col)
        {
            return;
        }

        const auto size = std::to_underlying(grid_state_.icon_size);

        box->set_size_request(size, size);
        picture->set_size_request(size, size);

        Glib::RefPtr<Gdk::Paintable> icon;
        if (col->file->is_thumbnail_loaded(size) && grid_state_.thumbnails)
        {
            icon = col->file->thumbnail(size);
        }
        else
        {
            icon = col->file->icon(size);
        }

        picture->set_paintable(icon);
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

    connections->push_back(col->signal_update_thumbnail().connect(update_image));
    connections->push_back(col->signal_changed().connect(update_label));

    item->set_data("connections",
                   connections.release(),
                   [](void* data) { delete static_cast<std::vector<sigc::connection>*>(data); });
}

void
gui::grid::on_unbind_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept
{
    auto col = std::dynamic_pointer_cast<ModelColumns>(item->get_item());
    if (!col)
    {
        return;
    }

    auto* box = dynamic_cast<Gtk::Box*>(item->get_child());
#if defined(LAYOUT_TESTING)
    // auto* picture_box = dynamic_cast<Gtk::Box*>(box->get_first_child());
    // auto* picture = dynamic_cast<Gtk::Picture*>(picture_box->get_first_child());
    // auto* label = dynamic_cast<Gtk::Label*>(picture_box->get_next_sibling());
#else
    // auto* picture = dynamic_cast<Gtk::Picture*>(box->get_first_child());
    // auto* label = dynamic_cast<Gtk::Label*>(picture->get_next_sibling());
#endif

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
gui::grid::on_background_click(std::int32_t n_press, double x, double y) noexcept
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
gui::grid::on_drag_prepare(double x, double y) const noexcept
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
gui::grid::on_drag_data_received(const Glib::ValueBase& value, double x, double y) noexcept
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

        // logger::debug<logger::gui>("DnD Source: {}", file->get_path());
        // logger::debug<logger::gui>("DnD Target: {}", dir_->path().string());

        // TODO smart, move if on same fs, copy otherwise
        auto task = vfs::move_task{
            .options = {},
            .source = file->get_path(),
            .destination = dir_->path(),
        };
        task_manager_->add(task);
    }

    return true;
}

Gdk::DragAction
gui::grid::on_drag_motion(double x, double y) noexcept
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
