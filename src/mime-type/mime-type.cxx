/*
 *      mime-type.c
 *
 *      Copyright 2007 Houng Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/* Currently this library is NOT MT-safe */

#include <cstdint>

#include <fcntl.h>

#include "mime-type.hxx"
#include "mime-cache.hxx"

/*
 * FIXME:
 * Currently, mmap cannot be used because of the limitation of mmap.
 * When a file is mapped for mime-type sniffing (checking file magic),
 * they could be deleted during the check and hence result in Bus error.
 * (Refer to the man page of mmap for detail)
 * So here I undef HAVE_MMAP to disable the implementation using mmap.
 */
#undef HAVE_MMAP

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

/* max extent used to checking text files */
#define TEXT_MAX_EXTENT 512

const char xdg_mime_type_unknown[] = "application/octet-stream";
const char xdg_mime_type_directory[] = "inode/directory";
const char xdg_mime_type_executable[] = "application/x-executable";
const char xdg_mime_type_plain_text[] = "text/plain";

static void mime_cache_foreach(GFunc func, void* user_data);
static bool mime_type_is_subclass(const char* type, const char* parent);

static MimeCache** caches = nullptr;
static unsigned int n_caches = 0;
uint32_t mime_cache_max_extent = 0;

/* allocated buffer used for mime magic checking to
     prevent frequent memory allocation */
static char* mime_magic_buf = nullptr;
/* for MT safety, the buffer should be locked */
G_LOCK_DEFINE_STATIC(mime_magic_buf);

/* load all mime.cache files on the system,
 * including /usr/share/mime/mime.cache,
 * /usr/local/share/mime/mime.cache,
 * and $HOME/.local/share/mime/mime.cache. */
static void mime_cache_load_all();

/* free all mime.cache files on the system */
static void mime_cache_free_all();

static bool mime_type_is_data_plain_text(const char* data, int len);

/*
 * Get mime-type of the specified file (quick, but less accurate):
 * Mime-type of the file is determined by cheking the filename only.
 * If statbuf != nullptr, it will be used to determine if the file is a directory.
 */
const char* mime_type_get_by_filename(const char* filename, struct stat* statbuf)
{
    const char* type = nullptr;
    const char* suffix_pos = nullptr;
    const char* prev_suffix_pos = (const char*)-1;
    int i;
    MimeCache* cache;

    if (G_UNLIKELY(statbuf && S_ISDIR(statbuf->st_mode)))
        return XDG_MIME_TYPE_DIRECTORY;

    for (i = 0; !type && i < n_caches; ++i)
    {
        cache = caches[i];
        type = mime_cache_lookup_literal(cache, filename);
        if (G_LIKELY(!type))
        {
            const char* _type = mime_cache_lookup_suffix(cache, filename, &suffix_pos);
            if (_type && suffix_pos < prev_suffix_pos)
            {
                type = _type;
                prev_suffix_pos = suffix_pos;
            }
        }
    }

    if (G_UNLIKELY(!type)) /* glob matching */
    {
        int max_glob_len = 0;
        int glob_len = 0;
        for (i = 0; !type && i < n_caches; ++i)
        {
            cache = caches[i];
            const char* matched_type;
            matched_type = mime_cache_lookup_glob(cache, filename, &glob_len);
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
 * tried first.  If the mime-type couldn't be determined, the content of
 * the file will be checked, which is much more time-consuming.
 * If statbuf is not nullptr, it will be used to determine if the file is a directory,
 * or if the file is an executable file; otherwise, the function will call stat()
 * to gather this info itself. So if you already have stat info of the file,
 * pass it to the function to prevent checking the file stat again.
 * If you have basename of the file, pass it to the function can improve the
 * efifciency, too. Otherwise, the function will try to get the basename of
 * the specified file again.
 */
const char* mime_type_get_by_file(const char* filepath, struct stat* statbuf, const char* basename)
{
    const char* type;
    struct stat _statbuf;

    /* IMPORTANT!! vfs-file-info.c:vfs_file_info_reload_mime_type() depends
     * on this function only using the st_mode from statbuf.
     * Also see vfs-dir.c:vfs_dir_load_thread */
    if (statbuf == nullptr || G_UNLIKELY(S_ISLNK(statbuf->st_mode)))
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
        if (G_LIKELY(basename))
            ++basename;
        else
            basename = filepath;
    }

    if (G_LIKELY(basename))
    {
        type = mime_type_get_by_filename(basename, statbuf);
        if (G_LIKELY(strcmp(type, XDG_MIME_TYPE_UNKNOWN)))
            return type;
        type = nullptr;
    }

    // sfm added check for reg or link due to hangs on fifo and chr dev
    if (G_LIKELY(statbuf->st_size > 0 && (S_ISREG(statbuf->st_mode) || S_ISLNK(statbuf->st_mode))))
    {
        int fd = -1;
        char* data;

        /* Open the file and map it into memory */
        fd = open(filepath, O_RDONLY, 0);
        if (fd != -1)
        {
            int len =
                mime_cache_max_extent < statbuf->st_size ? mime_cache_max_extent : statbuf->st_size;
#ifdef HAVE_MMAP
            data = (char*)mmap(nullptr, len, PROT_READ, MAP_SHARED, fd, 0);
#else
            /*
             * FIXME: Can g_alloca() be used here? It's very fast, but is it safe?
             * Actually, we can allocate a block of memory with the size of mime_cache_max_extent,
             * then we don't need to  do dynamic allocation/free every time, but multi-threading
             * will be a nightmare, so...
             */
            /* try to lock the common buffer */
            if (G_LIKELY(G_TRYLOCK(mime_magic_buf)))
                data = mime_magic_buf;
            else /* the buffer is in use, allocate new one */
                data = (char*)g_malloc(len);

            len = read(fd, data, len);

            if (G_UNLIKELY(len == -1))
            {
                if (G_LIKELY(data == mime_magic_buf))
                    G_UNLOCK(mime_magic_buf);
                else
                    g_free(data);
                data = (char*)-1;
            }
#endif
            if (data != (char*)-1)
            {
                int i;
                for (i = 0; !type && i < n_caches; ++i)
                    type = mime_cache_lookup_magic(caches[i], data, len);

                /* Check for executable file */
                if (!type && g_file_test(filepath, G_FILE_TEST_IS_EXECUTABLE))
                    type = XDG_MIME_TYPE_EXECUTABLE;

                /* fallback: check for plain text */
                if (!type)
                {
                    if (mime_type_is_data_plain_text(data,
                                                     len > TEXT_MAX_EXTENT ? TEXT_MAX_EXTENT : len))
                        type = XDG_MIME_TYPE_PLAIN_TEXT;
                }

#ifdef HAVE_MMAP
                munmap((char*)data, len);
#else
                if (G_LIKELY(data == mime_magic_buf))
                    G_UNLOCK(mime_magic_buf); /* unlock the common buffer */
                else                          /* we use our own buffer */
                    g_free(data);
#endif
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

static char* parse_xml_icon(const char* buf, size_t len, bool is_local)
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

    return g_strdup(icon_tag);
}

static char* parse_xml_desc(const char* buf, size_t len, const char* locale)
{
    const char* buf_end = buf + len;
    const char* comment = nullptr;
    const char* comment_end;
    const char* eng_comment;
    size_t comment_len = 0;
    static const char end_comment_tag[] = "</comment>";

    eng_comment = g_strstr_len(buf, len, "<comment>"); /* default English comment */
    if (G_UNLIKELY(!eng_comment))                      /* This xml file is invalid */
        return nullptr;
    len -= 9;
    eng_comment += 9;
    comment_end = g_strstr_len(eng_comment, len, end_comment_tag); /* find </comment> */
    if (G_UNLIKELY(!comment_end))
        return nullptr;
    size_t eng_comment_len = comment_end - eng_comment;

    if (G_LIKELY(locale))
    {
        char target[64];
        int target_len = g_snprintf(target, 64, "<comment xml:lang=\"%s\">", locale);
        buf = comment_end + 10;
        len = (buf_end - buf);
        if (G_LIKELY((comment = g_strstr_len(buf, len, target))))
        {
            len -= target_len;
            comment += target_len;
            comment_end = g_strstr_len(comment, len, end_comment_tag); /* find </comment> */
            if (G_LIKELY(comment_end))
                comment_len = (comment_end - comment);
            else
                comment = nullptr;
        }
    }
    if (G_LIKELY(comment))
        return g_strndup(comment, comment_len);
    return g_strndup(eng_comment, eng_comment_len);
}

static char* _mime_type_get_desc_icon(const char* file_path, const char* locale, bool is_local,
                                      char** icon_name)
{
    struct stat statbuf; // skip stat

    // char file_path[ 256 ];  //sfm to improve speed, file_path is passed

    // g_snprintf( file_path, 256, "%s/mime/%s.xml", data_dir, type );

    int fd = open(file_path, O_RDONLY, 0);
    if (G_UNLIKELY(fd == -1))
        return nullptr;
    if (G_UNLIKELY(fstat(fd, &statbuf) == -1))
    {
        close(fd);
        return nullptr;
    }

    char* buffer;
#ifdef HAVE_MMAP
    buffer = (char*)mmap(nullptr, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
#else
    buffer = (char*)g_malloc(statbuf.st_size);
    if (read(fd, buffer, statbuf.st_size) == -1)
    {
        g_free(buffer);
        buffer = (char*)-1;
    }
#endif
    close(fd);
    if (G_UNLIKELY(buffer == (void*)-1))
        return nullptr;

    char* _locale = nullptr;
    if (!locale)
    {
        const char* const* langs = g_get_language_names();
        char* dot = (char*)strchr(langs[0], '.');
        if (dot)
            locale = _locale = g_strndup(langs[0], (size_t)(dot - langs[0]));
        else
            locale = langs[0];
    }
    char* desc = parse_xml_desc(buffer, statbuf.st_size, locale);
    g_free(_locale);

    // only look for <icon /> tag in .local
    if (is_local && icon_name && *icon_name == nullptr)
        *icon_name = parse_xml_icon(buffer, statbuf.st_size, is_local);

#ifdef HAVE_MMAP
    munmap(buffer, statbuf.st_size);
#else
    g_free(buffer);
#endif
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
char* mime_type_get_desc_icon(const char* type, const char* locale, char** icon_name)
{
    char* desc;
    char file_path[256];

    /*  //sfm 0.7.7+ FIXED:
     * According to specs on freedesktop.org, user_data_dir has
     * higher priority than system_data_dirs, but in most cases, there was
     * no file, or very few files in user_data_dir, so checking it first will
     * result in many unnecessary open() system calls, yealding bad performance.
     * Since the spec really sucks, we don't follow it here.
     */
    /* FIXME: This path shouldn't be hard-coded. */
    g_snprintf(file_path, 256, "%s/mime/%s.xml", g_get_user_data_dir(), type);
    if (faccessat(0, file_path, F_OK, AT_EACCESS) != -1)
    {
        desc = _mime_type_get_desc_icon(file_path, locale, true, icon_name);
        if (desc)
            return desc;
    }

    // look in system dirs
    const char* const* dir = g_get_system_data_dirs();
    for (; *dir; ++dir)
    {
        /* FIXME: This path shouldn't be hard-coded. */
        g_snprintf(file_path, 256, "%s/mime/%s.xml", *dir, type);
        if (faccessat(0, file_path, F_OK, AT_EACCESS) != -1)
        {
            desc = _mime_type_get_desc_icon(file_path, locale, false, icon_name);
            if (G_LIKELY(desc))
                return desc;
        }
    }
    return nullptr;
}

void mime_type_finalize()
{
    mime_cache_free_all();
}

void mime_type_init()
{
    mime_cache_load_all();
    //    table = g_hash_table_new_full( g_str_hash, g_str_equal, g_free,
    //    (GDestroyNotify)mime_type_unref );
}

/* load all mime.cache files on the system,
 * including /usr/share/mime/mime.cache,
 * /usr/local/share/mime/mime.cache,
 * and $HOME/.local/share/mime/mime.cache. */
static void mime_cache_load_all()
{
    const char filename[] = "/mime/mime.cache";

    const char* const* dirs = g_get_system_data_dirs();
    n_caches = g_strv_length((char**)dirs) + 1;
    caches = (MimeCache**)g_slice_alloc(n_caches * sizeof(MimeCache*));

    char* path = g_build_filename(g_get_user_data_dir(), filename, nullptr);
    caches[0] = mime_cache_new(path);
    g_free(path);
    if (caches[0]->magic_max_extent > mime_cache_max_extent)
        mime_cache_max_extent = caches[0]->magic_max_extent;

    int i;
    for (i = 1; i < n_caches; ++i)
    {
        path = g_build_filename(dirs[i - 1], filename, nullptr);
        caches[i] = mime_cache_new(path);
        g_free(path);
        if (caches[i]->magic_max_extent > mime_cache_max_extent)
            mime_cache_max_extent = caches[i]->magic_max_extent;
    }
    mime_magic_buf = (char*)g_malloc(mime_cache_max_extent);
    return;
}

/* free all mime.cache files on the system */
static void mime_cache_free_all()
{
    mime_cache_foreach((GFunc)mime_cache_free, nullptr);
    g_slice_free1(n_caches * sizeof(MimeCache*), caches);
    n_caches = 0;
    caches = nullptr;
    mime_cache_max_extent = 0;

    g_free(mime_magic_buf);
    mime_magic_buf = nullptr;
}

/* Iterate through all mime caches */
static void mime_cache_foreach(GFunc func, void* user_data)
{
    int i;
    for (i = 0; i < n_caches; ++i)
        func(caches[i], user_data);
}

bool mime_cache_reload(MimeCache* cache)
{
    int i;
    bool ret = mime_cache_load(cache, cache->file_path);
    /* recalculate max magic extent */
    for (i = 0; i < n_caches; ++i)
    {
        if (caches[i]->magic_max_extent > mime_cache_max_extent)
            mime_cache_max_extent = caches[i]->magic_max_extent;
    }

    G_LOCK(mime_magic_buf);

    mime_magic_buf = (char*)g_realloc(mime_magic_buf, mime_cache_max_extent);

    G_UNLOCK(mime_magic_buf);

    return ret;
}

static bool mime_type_is_data_plain_text(const char* data, int len)
{
    int i;
    if (G_LIKELY(len >= 0 && data))
    {
        for (i = 0; i < len; ++i)
        {
            if (data[i] == '\0')
                return false;
        }
        return true;
    }
    return false;
}

bool mime_type_is_text_file(const char* file_path, const char* mime_type)
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
        if (!g_str_has_prefix(mime_type, "text/") && !g_str_has_prefix(mime_type, "application/"))
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
#ifdef HAVE_MMAP
                char* data;
                rlen = statbuf.st_size < TEXT_MAX_EXTENT ? statbuf.st_size : TEXT_MAX_EXTENT;
                data = (char*)mmap(nullptr, rlen, PROT_READ, MAP_SHARED, file, 0);
                ret = mime_type_is_data_plain_text(data, rlen);
                munmap((char*)data, rlen);
#else
                unsigned char data[TEXT_MAX_EXTENT];
                rlen = read(file, data, sizeof(data));
                ret = mime_type_is_data_plain_text((char*)data, rlen);
#endif
            }
        }
        close(file);
    }
    return ret;
}

bool mime_type_is_executable_file(const char* file_path, const char* mime_type)
{
    if (!mime_type)
    {
        mime_type = mime_type_get_by_file(file_path, nullptr, nullptr);
    }

    /*
     * Only executable types can be executale.
     * Since some common types, such as application/x-shellscript,
     * are not in mime database, we have to add them ourselves.
     */
    if (mime_type != XDG_MIME_TYPE_UNKNOWN &&
        (mime_type_is_subclass(mime_type, XDG_MIME_TYPE_EXECUTABLE) ||
         mime_type_is_subclass(mime_type, "application/x-shellscript")))
    {
        if (file_path)
        {
            if (!g_file_test(file_path, G_FILE_TEST_IS_EXECUTABLE))
                return false;
        }
        return true;
    }
    return false;
}

/* Check if the specified mime_type is the subclass of the specified parent type */
static bool mime_type_is_subclass(const char* type, const char* parent)
{
    /* special case, the type specified is identical to the parent type. */
    if (G_UNLIKELY(!strcmp(type, parent)))
        return true;

    int i;
    for (i = 0; i < n_caches; ++i)
    {
        const char** parents = mime_cache_lookup_parents(caches[i], type);
        if (parents)
        {
            const char** p;
            for (p = parents; *p; ++p)
            {
                if (!strcmp(parent, *p))
                    return true;
            }
        }
    }
    return false;
}

/*
 * Get mime caches
 */
MimeCache** mime_type_get_caches(int* n)
{
    *n = n_caches;
    return caches;
}
