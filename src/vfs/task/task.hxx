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

#include <expected>
#include <filesystem>
#include <string>
#include <system_error>

#include <sigc++/sigc++.h>

namespace vfs
{
class task
{
  public:
    task() noexcept = default;
    virtual ~task() noexcept = default;
    task(const task& other) noexcept = default;
    task(task&& other) noexcept = default;
    task& operator=(const task& other) noexcept = default;
    task& operator=(task&& other) noexcept = default;

    void run() noexcept;
    [[nodiscard]] std::error_code error() const noexcept;
    [[nodiscard]] std::expected<std::string, std::error_code> dump() const noexcept;

    virtual void compile() = 0;

    [[nodiscard]] auto
    signal_success() noexcept
    {
        return this->signal_success_;
    }

    [[nodiscard]] auto
    signal_failure() noexcept
    {
        return this->signal_failure_;
    }

  protected:
    std::string cmd_;
    std::error_code ec_;

    [[nodiscard]] static bool is_root(std::filesystem::path& path) noexcept;

  private:
    // TODO send stdout, stderr, exit code
    sigc::signal<void()> signal_success_;
    sigc::signal<void()> signal_failure_;
};

class chmod : public task
{
  public:
    // Options
    chmod& recursive() noexcept;

    // Required
    chmod& mode(std::filesystem::perms mode) noexcept;
    chmod& path(const std::filesystem::path& path) noexcept;

    void compile() noexcept override;

  private:
    std::string options_;
    std::string mode_;
    std::filesystem::path path_;
};

class chown : public task
{
  public:
    // Options
    chown& recursive() noexcept;

    // Required
    chown& user(const std::string_view user) noexcept;
    chown& group(const std::string_view group) noexcept;
    chown& path(const std::filesystem::path& path) noexcept;

    void compile() noexcept override;

  private:
    std::string options_;
    std::string user_;
    std::string group_;
    std::filesystem::path path_;
};

class copy : public task
{
  public:
    // Options
    copy& archive() noexcept;
    copy& recursive() noexcept;
    copy& force() noexcept;

    // Required
    copy& source(const std::filesystem::path& path) noexcept;
    copy& destination(const std::filesystem::path& path) noexcept;

    void compile() noexcept override;

  private:
    std::string options_;
    std::filesystem::path source_;
    std::filesystem::path destination_;
};

class move : public task
{
  public:
    // Options
    move& force() noexcept;

    // Required
    move& source(const std::filesystem::path& path) noexcept;
    move& destination(const std::filesystem::path& path) noexcept;

    void compile() noexcept override;

  private:
    std::string options_;
    std::filesystem::path source_;
    std::filesystem::path destination_;
};

class remove : public task
{
  public:
    // Options
    remove& recursive() noexcept;
    remove& force() noexcept;

    // Required
    remove& path(const std::filesystem::path& path) noexcept;

    void compile() noexcept override;

  private:
    std::string options_;
    std::filesystem::path path_;
};

class create_directory : public task
{
  public:
    // Options
    create_directory& create_parents() noexcept;

    // Required
    create_directory& path(const std::filesystem::path& path) noexcept;

    void compile() noexcept override;

  private:
    std::string options_;
    std::filesystem::path path_;
};

class create_file : public task
{
  public:
    // Required
    create_file& path(const std::filesystem::path& path) noexcept;

    void compile() noexcept override;

  private:
    std::filesystem::path path_;
};

class create_hardlink : public task
{
  public:
    // Options
    create_hardlink& force() noexcept;

    // Required
    create_hardlink& target(const std::filesystem::path& path) noexcept;
    create_hardlink& name(const std::filesystem::path& path) noexcept;

    void compile() noexcept override;

  private:
    std::string options_;
    std::filesystem::path target_;
    std::filesystem::path name_;
};

class create_symlink : public task
{
  public:
    // Options
    create_symlink& force() noexcept;

    // Required
    create_symlink& target(const std::filesystem::path& path) noexcept;
    create_symlink& name(const std::filesystem::path& path) noexcept;

    void compile() noexcept override;

  private:
    std::string options_;
    std::filesystem::path target_;
    std::filesystem::path name_;
};
} // namespace vfs
