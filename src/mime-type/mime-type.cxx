/**
 * Copyright (C) 2007 PCMan <pcman.tw@gmail.com>
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

/* Currently this library is NOT MT-safe */

#include <string>
#include <string_view>

#include <filesystem>

#include <vector>

#include <iostream>
#include <fstream>

#include <fcntl.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "utils.hxx"

#include "vfs/vfs-user-dir.hxx"

#include "mime-type/mime-type.hxx"
#include "mime-type/mime-cache.hxx"

/* max extent used to checking text files */
#define TEXT_MAX_EXTENT 512

static bool mime_type_is_subclass(const char* type, const char* parent);

static std::size_t n_caches = 0;
std::vector<MimeCache> caches;

std::uint32_t mime_cache_max_extent = 0;

/* allocated buffer used for mime magic checking to
     prevent frequent memory allocation */
static char* mime_magic_buf = nullptr;
/* for MT safety, the buffer should be locked */
G_LOCK_DEFINE_STATIC(mime_magic_buf);

/* free all mime.cache files on the system */
static void mime_cache_free_all();

static bool mime_type_is_data_plain_text(const char* data, int len);

/*
 * Get mime-type of the specified file (quick, but less accurate):
 * Mime-type of the file is determined by cheking the filename only.
 * If statbuf != nullptr, it will be used to determine if the file is a directory.
 */
const char*
mime_type_get_by_filename(const char* filename, struct stat* statbuf)
{
    const char* type = nullptr;
    const char* suffix_pos = nullptr;
    const char* prev_suffix_pos = (const char*)-1;

    if (statbuf && S_ISDIR(statbuf->st_mode))
        return XDG_MIME_TYPE_DIRECTORY;

    for (std::size_t i = 0; !type && i < n_caches; ++i)
    {
        MimeCache cache = caches.at(i);
        type = cache.lookup_literal(filename);
        if (!type)
        {
            const char* _type = cache.lookup_suffix(filename, &suffix_pos);
            if (_type && suffix_pos < prev_suffix_pos)
            {
                type = _type;
                prev_suffix_pos = suffix_pos;
            }
        }
    }

    if (!type) /* glob matching */
    {
        int max_glob_len = 0;
        int glob_len = 0;
        for (std::size_t i = 0; !type && i < n_caches; ++i)
        {
            MimeCache cache = caches.at(i);
            const char* matched_type;
            matched_type = cache.lookup_glob(filename, &glob_len);
            /* according to the mime.cache 1.0 spec, we should use the longest glob matched. */
            if (matched_type && glob_len > max_glob_len)
            {
                type = matched_type;
                max_glob_len = glob_len;
            }
        }
    }

    return type && *type ? type : XDG_MIME_TYPE_UNKNOWN;
}

/*
 * Get mime-type info of the specified file (slow, but more accurate):
 * To determine the mime-type of the file, mime_type_get_by_filename() is
 * tried first.  If the mime-type could not be determined, the content of
 * the file will be checked, which is much more time-consuming.
 * If statbuf is not nullptr, it will be used to determine if the file is a directory,
 * or if the file is an executable file; otherwise, the function will call stat()
 * to gather this info itself. So if you already have stat info of the file,
 * pass it to the function to prevent checking the file stat again.
 * If you have basename of the file, pass it to the function can improve the
 * efifciency, too. Otherwise, the function will try to get the basename of
 * the specified file again.
 */
const char*
mime_type_get_by_file(const char* filepath, struct stat* statbuf, const char* basename)
{
    const char* type;
    struct stat _statbuf;

    /* IMPORTANT!! vfs-file-info.c:vfs_file_info_reload_mime_type() depends
     * on this function only using the st_mode from statbuf.
     * Also see vfs-dir.c:vfs_dir_load_thread */
    if (statbuf == nullptr || S_ISLNK(statbuf->st_mode))
    {
        statbuf = &_statbuf;
        if (stat(filepath, statbuf) == -1)
            return XDG_MIME_TYPE_UNKNOWN;
    }

    if (S_ISDIR(statbuf->st_mode))
        return XDG_MIME_TYPE_DIRECTORY;

    if (basename == nullptr)
    {
        basename = g_utf8_strrchr(filepath, -1, '/');
        if (basename)
            ++basename;
        else
            basename = filepath;
    }

    if (basename)
    {
        type = mime_type_get_by_filename(basename, statbuf);
        if (strcmp(type, XDG_MIME_TYPE_UNKNOWN))
            return type;
        type = nullptr;
    }

    // sfm added check for reg or link due to hangs on fifo and chr dev
    if (statbuf->st_size > 0 && (S_ISREG(statbuf->st_mode) || S_ISLNK(statbuf->st_mode)))
    {
        int fd = -1;
        char* data;

        /* Open the file and map it into memory */
        fd = open(filepath, O_RDONLY, 0);
        if (fd != -1)
        {
            int len =
                mime_cache_max_extent < statbuf->st_size ? mime_cache_max_extent : statbuf->st_size;
            /*
             * FIXME: Can g_alloca() be used here? It is very fast, but is it safe?
             * Actually, we can allocate a block of memory with the size of mime_cache_max_extent,
             * then we do not need to  do dynamic allocation/free every time, but multi-threading
             * will be a nightmare, so...
             */
            /* try to lock the common buffer */
            if (G_TRYLOCK(mime_magic_buf))
                data = mime_magic_buf;
            else /* the buffer is in use, allocate new one */
                data = CHAR(g_malloc(len));

            len = read(fd, data, len);

            if (len == -1)
            {
                if (data == mime_magic_buf)
                    G_UNLOCK(mime_magic_buf);
                else
                    free(data);
                data = (char*)-1;
            }
            if (data != (char*)-1)
            {
                for (std::size_t i = 0; !type && i < caches.size(); ++i)
                {
                    type = caches.at(i).lookup_magic(data, len);
                }

                /* Check for executable file */
                if (!type && have_x_access(filepath))
                    type = XDG_MIME_TYPE_EXECUTABLE;

                /* fallback: check for plain text */
                if (!type)
                {
                    if (mime_type_is_data_plain_text(data,
                                                     len > TEXT_MAX_EXTENT ? TEXT_MAX_EXTENT : len))
                        type = XDG_MIME_TYPE_PLAIN_TEXT;
                }

                if (data == mime_magic_buf)
                    G_UNLOCK(mime_magic_buf); /* unlock the common buffer */
                else                          /* we use our own buffer */
                    free(data);
            }
            close(fd);
        }
    }
    else
    {
        /* empty file can be viewed as text file */
        type = XDG_MIME_TYPE_PLAIN_TEXT;
    }
    return type && *type ? type : XDG_MIME_TYPE_UNKNOWN;
}

static char*
parse_xml_icon(const char* buf, std::size_t len, bool is_local)
{ // Note: This function modifies contents of buf
    char* icon_tag = nullptr;

    if (is_local)
    {
        //  "<icon name=.../>" is only used in user .local XML files
        // find <icon name=
        icon_tag = g_strstr_len(buf, len, "<icon name=");
        if (icon_tag)
        {
            icon_tag += 11;
            len -= 11;
        }
    }
    if (!icon_tag && !is_local)
    {
        // otherwise find <generic-icon name=
        icon_tag = g_strstr_len(buf, len, "<generic-icon name=");
        if (icon_tag)
        {
            icon_tag += 19;
            len -= 19;
        }
    }
    if (!icon_tag)
        return nullptr; // no icon found

    // find />
    char* end_tag = g_strstr_len(icon_tag, len, "/>");
    if (!end_tag)
        return nullptr;
    end_tag[0] = '\0';
    if (strchr(end_tag, '\n'))
        return nullptr; // linefeed in tag

    // remove quotes
    if (icon_tag[0] == '"')
        icon_tag++;
    if (end_tag[-1] == '"')
        end_tag[-1] = '\0';

    if (icon_tag == end_tag)
        return nullptr; // blank name

    return ztd::strdup(icon_tag);
}

static char*
parse_xml_desc(const char* buf, std::size_t len, const char* locale)
{
    const char* buf_end = buf + len;
    const char* comment = nullptr;
    const char* comment_end;
    const char* eng_comment;
    std::size_t comment_len = 0;
    static const char end_comment_tag[] = "</comment>";

    eng_comment = g_strstr_len(buf, len, "<comment>"); /* default English comment */
    if (!eng_comment)                                  /* This xml file is invalid */
        return nullptr;
    len -= 9;
    eng_comment += 9;
    comment_end = g_strstr_len(eng_comment, len, end_comment_tag); /* find </comment> */
    if (!comment_end)
        return nullptr;
    std::size_t eng_comment_len = comment_end - eng_comment;

    if (locale)
    {
        char target[64];
        int target_len = g_snprintf(target, 64, "<comment xml:lang=\"%s\">", locale);
        buf = comment_end + 10;
        len = (buf_end - buf);
        if ((comment = g_strstr_len(buf, len, target)))
        {
            len -= target_len;
            comment += target_len;
            comment_end = g_strstr_len(comment, len, end_comment_tag); /* find </comment> */
            if (comment_end)
                comment_len = (comment_end - comment);
            else
                comment = nullptr;
        }
    }
    if (comment)
        return strndup(comment, comment_len);
    return strndup(eng_comment, eng_comment_len);
}

static char*
_mime_type_get_desc_icon(const char* file_path, const char* locale, bool is_local, char** icon_name)
{
    std::string line;
    std::ifstream file(file_path);
    if (!file.is_open())
        return nullptr;

    std::string buffer;
    while (std::getline(file, line))
    {
        buffer.append(line);
    }
    file.close();

    if (buffer.empty())
        return nullptr;

    char* _locale = nullptr;
    if (!locale)
    {
        const char* const* langs = g_get_language_names();
        char* dot = (char*)strchr(langs[0], '.');
        if (dot)
            locale = _locale = strndup(langs[0], (size_t)(dot - langs[0]));
        else
            locale = langs[0];
    }
    char* desc = parse_xml_desc(buffer.c_str(), buffer.size(), locale);
    free(_locale);

    // only look for <icon /> tag in .local
    if (is_local && icon_name && *icon_name == nullptr)
        *icon_name = parse_xml_icon(buffer.c_str(), buffer.size(), is_local);

    return desc;
}

/* Get human-readable description and icon name of the mime-type
 * If locale is nullptr, current locale will be used.
 * The returned string should be freed when no longer used.
 * The icon_name will only be set if points to nullptr, and must be freed.
 *
 * Note: Spec is not followed for icon.  If icon tag is found in .local
 * xml file, it is used.  Otherwise vfs_mime_type_get_icon guesses the icon.
 * The Freedesktop spec /usr/share/mime/generic-icons is NOT parsed. */
char*
mime_type_get_desc_icon(const char* type, const char* locale, char** icon_name)
{
    char* desc;
    std::string file_path;

    /*  //sfm 0.7.7+ FIXED:
     * According to specs on freedesktop.org, user_data_dir has
     * higher priority than system_data_dirs, but in most cases, there was
     * no file, or very few files in user_data_dir, so checking it first will
     * result in many unnecessary open() system calls, yealding bad performance.
     * Since the spec really sucks, we do not follow it here.
     */

    file_path = fmt::format("{}/mime/{}.xml", vfs_user_data_dir(), type);
    if (faccessat(0, file_path.c_str(), F_OK, AT_EACCESS) != -1)
    {
        desc = _mime_type_get_desc_icon(file_path.c_str(), locale, true, icon_name);
        if (desc)
            return desc;
    }

    // look in system dirs
    for (std::string_view sys_dir: vfs_system_data_dir())
    {
        file_path = fmt::format("{}/mime/{}.xml", sys_dir, type);
        if (faccessat(0, file_path.c_str(), F_OK, AT_EACCESS) != -1)
        {
            desc = _mime_type_get_desc_icon(file_path.c_str(), locale, false, icon_name);
            if (desc)
                return desc;
        }
    }
    return nullptr;
}

void
mime_type_finalize()
{
    mime_cache_free_all();
}

// load all mime.cache files on the system,
// including /usr/share/mime/mime.cache,
// /usr/local/share/mime/mime.cache,
// and $HOME/.local/share/mime/mime.cache.
void
mime_type_init()
{
    const std::string filename = "/mime/mime.cache";

    const std::vector<std::string> dirs = vfs_system_data_dir();
    n_caches = dirs.size();

    std::string path = Glib::build_filename(vfs_user_data_dir(), filename);
    caches.push_back(MimeCache(path));

    if (caches[0].get_magic_max_extent() > mime_cache_max_extent)
        mime_cache_max_extent = caches[0].get_magic_max_extent();

    for (std::size_t i = 0; i < n_caches; ++i)
    {
        path = Glib::build_filename(dirs[i], filename);
        caches.push_back(MimeCache(path));

        if (caches[i].get_magic_max_extent() > mime_cache_max_extent)
            mime_cache_max_extent = caches[i].get_magic_max_extent();
    }

    mime_magic_buf = CHAR(g_malloc(mime_cache_max_extent));
}

/* free all mime.cache files on the system */
static void
mime_cache_free_all()
{
    while (true)
    {
        if (caches.empty())
            break;
        caches.pop_back();
    }

    mime_cache_max_extent = 0;

    free(mime_magic_buf);
    mime_magic_buf = nullptr;
}

void
mime_cache_reload(MimeCache& cache)
{
    cache.reload();

    /* recalculate max magic extent */
    for (std::size_t i = 0; i < n_caches; ++i)
    {
        if (caches[i].get_magic_max_extent() > mime_cache_max_extent)
            mime_cache_max_extent = caches[i].get_magic_max_extent();
    }

    G_LOCK(mime_magic_buf);

    mime_magic_buf = (char*)g_realloc(mime_magic_buf, mime_cache_max_extent);

    G_UNLOCK(mime_magic_buf);
}

static bool
mime_type_is_data_plain_text(const char* data, int len)
{
    if (len >= 0 && data)
    {
        for (int i = 0; i < len; ++i)
        {
            if (data[i] == '\0')
                return false;
        }
        return true;
    }
    return false;
}

bool
mime_type_is_text_file(const char* file_path, const char* mime_type)
{
    int rlen;
    bool ret = false;

    if (mime_type)
    {
        if (!strcmp(mime_type, "application/pdf"))
            // seems to think this is XDG_MIME_TYPE_PLAIN_TEXT
            return false;
        if (mime_type_is_subclass(mime_type, XDG_MIME_TYPE_PLAIN_TEXT))
            return true;
        if (!ztd::startswith(mime_type, "text/") && !ztd::startswith(mime_type, "application/"))
            return false;
    }

    if (!file_path)
        return false;

    int file = open(file_path, O_RDONLY);
    if (file != -1)
    {
        struct stat statbuf;
        if (fstat(file, &statbuf) != -1)
        {
            if (S_ISREG(statbuf.st_mode))
            {
                unsigned char data[TEXT_MAX_EXTENT];
                rlen = read(file, data, sizeof(data));
                ret = mime_type_is_data_plain_text((char*)data, rlen);
            }
        }
        close(file);
    }
    return ret;
}

bool
mime_type_is_executable_file(const char* file_path, const char* mime_type)
{
    if (!mime_type)
        mime_type = mime_type_get_by_file(file_path, nullptr, nullptr);

    /*
     * Only executable types can be executale.
     * Since some common types, such as application/x-shellscript,
     * are not in mime database, we have to add them ourselves.
     */
    if (!ztd::same(mime_type, XDG_MIME_TYPE_UNKNOWN) &&
        (mime_type_is_subclass(mime_type, XDG_MIME_TYPE_EXECUTABLE) ||
         mime_type_is_subclass(mime_type, "application/x-shellscript")))
    {
        if (file_path)
        {
            if (!have_x_access(file_path))
                return false;
        }
        return true;
    }
    return false;
}

/* Check if the specified mime_type is the subclass of the specified parent type */
static bool
mime_type_is_subclass(const char* type, const char* parent)
{
    /* special case, the type specified is identical to the parent type. */
    if (!strcmp(type, parent))
        return true;

    for (MimeCache& cache: caches)
    {
        const std::vector<const char*> parents = cache.lookup_parents(type);
        if (parents.empty())
            break;

        for (const char* p: parents)
        {
            if (!strcmp(parent, p))
                return true;
        }
    }
    return false;
}

/*
 * Get mime caches
 */
std::vector<MimeCache>&
mime_type_get_caches()
{
    return caches;
}

/*
 * Reload all mime caches
 */
void
mime_type_regen_all_caches()
{
    for (MimeCache& cache: caches)
    {
        mime_cache_reload(cache);
    }
}
