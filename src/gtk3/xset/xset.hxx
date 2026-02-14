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

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "xset/xset-lookup.hxx"

#include "types.hxx"

// need to forward declare to avoid circular header dependencies
namespace gui
{
struct browser;
}

namespace xset
{
struct set : public std::enable_shared_from_this<set>
{
    set() = delete;
    set(const xset::name name) noexcept;
    ~set() = default;
    set(const set& other) = delete;
    set(set&& other) = delete;
    set& operator=(const set& other) = delete;
    set& operator=(set&& other) = delete;

    /**
     * @brief get
     * - create an xset or return an existing xset.
     *
     * @param[in] name the xset name
     * @param[in] only_existing if true only return an xset if it already exists,
     * otherwise return nullptr instead of creating a new xset.
     *
     * @return a valid xset::set
     */
    [[nodiscard]] static std::shared_ptr<set> get(const xset::name name,
                                                  const bool only_existing = false) noexcept;

    /**
     * @brief get
     * - create an xset or return an existing xset.
     *
     * @param[in] name the xset name
     * @param[in] panel the panel number (1-4)
     *
     * @return a valid xset::set
     */
    [[nodiscard]] static std::shared_ptr<set> get(const std::string_view name,
                                                  const panel_t panel) noexcept;

    /**
     * @brief get
     * - create an xset or return an existing xset.
     *
     * @param[in] name the xset name
     * @param[in] panel the panel number (1-4)
     *
     * @return a valid xset::set
     */
    [[nodiscard]] static std::shared_ptr<set> get(const xset::panel name,
                                                  const panel_t panel) noexcept;

    /**
     * @brief get
     * - create an xset or return an existing xset.
     *
     * @param[in] name the xset name
     * @param[in] panel the panel number (1-4)
     * @param[in] mode the main window mode
     *
     * @return a valid xset::set
     */
    [[nodiscard]] static std::shared_ptr<set> get(const std::string_view name, const panel_t panel,
                                                  const xset::main_window_panel mode) noexcept;

    /**
     * @brief get
     * - create an xset or return an existing xset.
     *
     * @param[in] name the xset name
     * @param[in] panel the panel number (1-4)
     * @param[in] mode the main window mode
     *
     * @return a valid xset::set
     */
    [[nodiscard]] static std::shared_ptr<set> get(const xset::panel name, const panel_t panel,
                                                  const xset::main_window_panel mode) noexcept;

    /**
     * @brief get
     * - create an xset or return an existing xset.
     *
     * @param[in] name the xset name
     * @param[in] only_existing if true only return an xset if it already exists,
     * otherwise return nullptr instead of creating a new xset.
     *
     * @return a valid xset::set
     */
    [[nodiscard]] static std::shared_ptr<set> get(const std::string_view name,
                                                  const bool only_existing = false) noexcept;

    [[nodiscard]] std::string_view name() const noexcept;

    xset::name xset_name;

    enum enabled : std::uint8_t
    { // do not reorder - saved in config file
        unset,
        yes,
        no,
    };
    enabled b{enabled::unset}; // saved, tri-state enum 0=unset(false) 1=true 2=false

    std::optional<std::string> s{std::nullopt}; // saved
    std::optional<std::string> x{std::nullopt}; // saved
    std::optional<std::string> y{std::nullopt}; // saved
    std::optional<std::string> z{std::nullopt}; // saved, for menu_string locked, stores default
    bool disable{false};                        // not saved
    gui::browser* browser{nullptr};             // not saved - set automatically
    std::shared_ptr<set> shared_key{nullptr};   // not saved

    struct callback_data
    {
        GFunc func{nullptr}; // not saved
        void* data{nullptr}; // not saved
    };
    callback_data callback;

    enum class menu_type : std::uint8_t
    { // do not reorder - saved in config file
        normal,
        check,
        string,
        radio,
        reserved_00,
        reserved_01,
        reserved_02,
        reserved_03,
        reserved_04,
        reserved_05,
        reserved_06,
        reserved_07,
        reserved_08,
        reserved_09,
        reserved_10,
        reserved_11,
        reserved_12,
        submenu, // add new before submenu
        sep,
    };
    struct menu_data
    {
        std::optional<std::string> label{std::nullopt}; // not saved
        menu_type type{menu_type::normal};              // not saved
        // only used when type == menu_type::radio
        GSList* radio_group{nullptr};            // not saved
        std::shared_ptr<set> radio_set{nullptr}; // not saved
        // gobject data
        struct obj_data
        {
            const char* key{nullptr};
            void* data{nullptr};
        };
        obj_data obj;
    };
    menu_data menu;

    enum class keybinding_type : std::uint8_t
    {
        invalid, // keybindings are disabled for this xset
        navigation,
        editing,
        view,
        tabs,
        general,
        opening,
    };
    struct keybinding_data
    {
        u32 key{0};                                     // saved
        u32 modifier{0};                                // saved
        keybinding_type type{keybinding_type::invalid}; // not saved
    };
    keybinding_data keybinding;

    std::optional<std::string> icon{std::nullopt};  // not saved
    std::optional<std::string> desc{std::nullopt};  // not saved
    std::optional<std::string> title{std::nullopt}; // not saved

    std::vector<xset::name> context_menu_entries; // not saved, in order

  private:
    [[nodiscard]] static std::shared_ptr<set> create(const xset::name name) noexcept;
};

// all xsets
std::span<const std::shared_ptr<xset::set>> sets();
} // namespace xset

using xset_t = std::shared_ptr<xset::set>;

// get/set //

// void xset_set(const xset_t& set, const xset::var var, const std::string_view value) noexcept;
void xset_set(const xset::name name, const xset::var var, const std::string_view value) noexcept;

// B
bool xset_get_b(const xset::name name) noexcept;
bool xset_get_b(const std::string_view name) noexcept;
bool xset_get_b_panel(panel_t panel, const std::string_view name) noexcept;
bool xset_get_b_panel(panel_t panel, xset::panel name) noexcept;
bool xset_get_b_panel_mode(panel_t panel, const std::string_view name,
                           xset::main_window_panel mode) noexcept;
bool xset_get_b_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode) noexcept;

void xset_set_b(const xset::name name, bool bval) noexcept;
void xset_set_b(const std::string_view name, bool bval) noexcept;
void xset_set_b_panel(panel_t panel, const std::string_view name, bool bval) noexcept;
void xset_set_b_panel(panel_t panel, xset::panel name, bool bval) noexcept;
void xset_set_b_panel_mode(panel_t panel, const std::string_view name, xset::main_window_panel mode,
                           bool bval) noexcept;
void xset_set_b_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode,
                           bool bval) noexcept;

/**
 * Oneshot lookup functions
 */

// S
std::optional<std::string> xset_get_s(const xset::name name) noexcept;
std::optional<std::string> xset_get_s(const std::string_view name) noexcept;
std::optional<std::string> xset_get_s_panel(panel_t panel, const std::string_view name) noexcept;
std::optional<std::string> xset_get_s_panel(panel_t panel, xset::panel name) noexcept;

// X
std::optional<std::string> xset_get_x(const xset::name name) noexcept;
std::optional<std::string> xset_get_x(const std::string_view name) noexcept;

// Y
std::optional<std::string> xset_get_y(const xset::name name) noexcept;
std::optional<std::string> xset_get_y(const std::string_view name) noexcept;

// Z
std::optional<std::string> xset_get_z(const xset::name name) noexcept;
std::optional<std::string> xset_get_z(const std::string_view name) noexcept;

// Panel
void xset_set_panel(panel_t panel, const std::string_view name, xset::var var,
                    const std::string_view value) noexcept;
void xset_set_panel(panel_t panel, xset::panel name, xset::var var,
                    const std::string_view value) noexcept;

// CB

void xset_set_cb(const xset_t& set, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb(const xset::name name, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb(const std::string_view name, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb_panel(panel_t panel, const std::string_view name, GFunc cb_func,
                       void* cb_data) noexcept;
void xset_set_cb_panel(panel_t panel, xset::panel name, GFunc cb_func, void* cb_data) noexcept;

void xset_set_ob(const xset_t& set, const std::string_view key, void* user_data) noexcept;
void xset_set_ob(const xset_t& set, const std::string_view key, const i32 user_data) noexcept;
void xset_set_ob(const xset_t& set, const std::string_view key,
                 const std::string_view user_data) noexcept;

// Int

i32 xset_get_int(const xset_t& set, xset::var var) noexcept;
i32 xset_get_int(const xset::name name, xset::var var) noexcept;
i32 xset_get_int(const std::string_view name, xset::var var) noexcept;
i32 xset_get_int_panel(panel_t panel, const std::string_view name, xset::var var) noexcept;
i32 xset_get_int_panel(panel_t panel, xset::panel name, xset::var var) noexcept;
