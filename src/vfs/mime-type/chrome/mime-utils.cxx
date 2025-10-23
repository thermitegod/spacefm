// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrono>
#include <filesystem>
#include <stack>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

#include <cstdint>

#include <arpa/inet.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/user-dirs.hxx"

#include "vfs/mime-type/chrome/mime-utils.hxx"
#include "vfs/utils/file-ops.hxx"
#include "vfs/utils/utils.hxx"

#include "logger.hxx"

// https://source.chromium.org/chromium/chromium/src/+/main:base/third_party/icu/icu_utf.h

/**
 * Append a code point to a string, overwriting 1 to 4 bytes.
 * The offset points to the current end of the string contents
 * and is advanced (post-increment).
 * "Unsafe" macro, assumes a valid code point and sufficient space in the string.
 * Otherwise, the result is undefined.
 *
 * @param s const uint8_t * string buffer
 * @param i string offset
 * @param c code point to append
 * @see U8_APPEND
 * @stable ICU 2.4
 */
#define CBU8_APPEND_UNSAFE(s, i, c)                                                                \
    do                                                                                             \
    {                                                                                              \
        uint32_t __uc = (uint32_t)(c);                                                             \
        if (__uc <= 0x7f)                                                                          \
        {                                                                                          \
            (s)[(i)++] = (uint8_t)__uc;                                                            \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            if (__uc <= 0x7ff)                                                                     \
            {                                                                                      \
                (s)[(i)++] = (uint8_t)((__uc >> 6) | 0xc0);                                        \
            }                                                                                      \
            else                                                                                   \
            {                                                                                      \
                if (__uc <= 0xffff)                                                                \
                {                                                                                  \
                    (s)[(i)++] = (uint8_t)((__uc >> 12) | 0xe0);                                   \
                }                                                                                  \
                else                                                                               \
                {                                                                                  \
                    (s)[(i)++] = (uint8_t)((__uc >> 18) | 0xf0);                                   \
                    (s)[(i)++] = (uint8_t)(((__uc >> 12) & 0x3f) | 0x80);                          \
                }                                                                                  \
                (s)[(i)++] = (uint8_t)(((__uc >> 6) & 0x3f) | 0x80);                               \
            }                                                                                      \
            (s)[(i)++] = (uint8_t)((__uc & 0x3f) | 0x80);                                          \
        }                                                                                          \
    } while (0)

// https://source.chromium.org/chromium/chromium/src/+/main:base/strings/utf_string_conversion_utils.cc

static size_t
WriteUnicodeCharacter(const std::int32_t code_point, std::string* output) noexcept
{
    if (code_point >= 0 && code_point <= 0x7f)
    {
        // Fast path the common case of one byte.
        output->push_back(static_cast<char>(code_point));
        return 1;
    }

    // The maximum number of UTF-8 code units (bytes) per Unicode code point (U+0000..U+10ffff).
    static constexpr uint8_t CBU8_MAX_LENGTH = 4;

    // CBU8_APPEND_UNSAFE can append up to 4 bytes.
    auto char_offset = output->length();
    auto original_char_offset = char_offset;
    output->resize(char_offset + CBU8_MAX_LENGTH);

    CBU8_APPEND_UNSAFE(reinterpret_cast<uint8_t*>(output->data()), char_offset, code_point);

    // CBU8_APPEND_UNSAFE will advance our pointer past the inserted character, so
    // it will represent the new length of the string.
    output->resize(char_offset);
    return char_offset - original_char_offset;
}

// https://source.chromium.org/chromium/chromium/src/+/main:base/files/file_util.cc
// only using the API

static bool
ReadFileToStringWithMaxSize(const std::filesystem::path& path, std::string& contents,
                            const std::size_t max_size) noexcept
{
    const auto buffer = vfs::utils::read_file(path, max_size);
    if (!buffer)
    {
        logger::error<logger::vfs>("Failed to read file: {} {}",
                                   path.string(),
                                   buffer.error().message());
        return false;
    }

    contents = buffer.value();
    return true;
}

// https://source.chromium.org/chromium/chromium/src/+/main:base/nix/mime_util_xdg.h

// Mime type with weight.
struct WeightedMime final
{
    std::string mime_type;
    uint8_t weight{0};
};

// Map of file extension to weighted mime type.
using MimeTypeMap = std::unordered_map<std::string, WeightedMime>;

// Parses a file at `file_path` which should be in the same format as the
// /usr/share/mime/mime.cache file on Linux.
// https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-0.21.html#idm46058238280640
// `out_mime_types` will be populated with keys that are a file extension and a
// value that is a MIME type if a higher weighted mime is found that currently
// exists. Returns true if there was a valid list parsed from the file and false
// otherwise.
static bool ParseMimeTypes(const std::filesystem::path& file_path,
                           MimeTypeMap& out_mime_types) noexcept;

// https://source.chromium.org/chromium/chromium/src/+/main:base/nix/mime_util_xdg.cc

// Ridiculously large size for a /usr/share/mime/mime.cache file.
// Default file is about 100KB, allow up to 10MB.
constexpr size_t kMaxMimeTypesFileSize = 10uz * 1024uz * 1024uz;
// Maximum number of nodes to allow in reverse suffix tree.
// Default file has ~3K nodes, allow up to 30K.
constexpr size_t kMaxNodes = 30000uz;
// Maximum file extension size.
constexpr size_t kMaxExtSize = 100uz;
// Header size in mime.cache file.
constexpr size_t kHeaderSize = 40uz;
// Largest valid unicode code point is U+10ffff.
constexpr uint32_t kMaxUnicode = 0x10ffff;
// Default mime glob weight is 50, max is 100.
constexpr uint8_t kDefaultGlobWeight = 50;

// Path and last modified of mime.cache file.
struct FileInfo final
{
    std::filesystem::path path;
    std::chrono::system_clock::time_point last_modified;
};

// Load all mime cache files on the system.
static void
LoadAllMimeCacheFiles(MimeTypeMap& map, std::vector<FileInfo>& files) noexcept
{
    std::vector<std::filesystem::path> mime_cache_files;

    const auto user_mime_cache = vfs::user::data() / "mime/mime.cache";
    mime_cache_files.push_back(user_mime_cache);

    for (const std::filesystem::path sys_dir : Glib::get_system_data_dirs())
    {
        const auto sys_mime_cache = sys_dir / "mime/mime.cache";
        mime_cache_files.push_back(sys_mime_cache);
    }

    for (const auto& mime_cache : mime_cache_files)
    {
        const auto mime_stat = ztd::stat::create(mime_cache);
        if (mime_stat && ParseMimeTypes(mime_cache, map))
        {
            files.emplace_back(mime_cache, mime_stat->mtime());
        }
    }
}

// Read 4 bytes from string `buf` at `offset` as network order uint32_t.
// Returns false if `offset > buf.size() - 4` or `offset` is not aligned to a
// 4-byte word boundary, or `*result` is not between `min_result` and
// `max_result`. `field_name` is used in error message.
static bool
ReadInt(const std::string& buf, uint32_t offset, const std::string& field_name, uint32_t min_result,
        size_t max_result, uint32_t* result) noexcept
{
    if (offset > buf.size() - 4 || (offset & 0x3))
    {
        logger::error<logger::vfs>("Invalid offset={} for {}, string size={}",
                                   offset,
                                   field_name,
                                   buf.size());
        return false;
    }
    *result = ntohl(*reinterpret_cast<const uint32_t*>(buf.c_str() + offset));
    if (*result < min_result || *result > max_result)
    {
        logger::error<logger::vfs>("Invalid {} = {} not between min_result={} and max_result={}",
                                   field_name,
                                   *result,
                                   min_result,
                                   max_result);
        return false;
    }
    return true;
}

bool
ParseMimeTypes(const std::filesystem::path& file_path, MimeTypeMap& out_mime_types) noexcept
{
    // File format from
    // https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-0.21.html#idm46070612075440
    // Header:
    // 2      CARD16    MAJOR_VERSION  1
    // 2      CARD16    MINOR_VERSION  2
    // 4      CARD32    ALIAS_LIST_OFFSET
    // 4      CARD32    PARENT_LIST_OFFSET
    // 4      CARD32    LITERAL_LIST_OFFSET
    // 4      CARD32    REVERSE_SUFFIX_TREE_OFFSET
    // ...
    // ReverseSuffixTree:
    // 4      CARD32    N_ROOTS
    // 4      CARD32    FIRST_ROOT_OFFSET
    // ReverseSuffixTreeNode:
    // 4      CARD32    CHARACTER
    // 4      CARD32    N_CHILDREN
    // 4      CARD32    FIRST_CHILD_OFFSET
    // ReverseSuffixTreeLeafNode:
    // 4      CARD32    0
    // 4      CARD32    MIME_TYPE_OFFSET
    // 4      CARD32    WEIGHT in lower 8 bits
    //                  FLAGS in rest:
    //                  0x100 = case-sensitive

    std::string buf;
    if (!ReadFileToStringWithMaxSize(file_path, buf, kMaxMimeTypesFileSize))
    {
        logger::error<logger::vfs>("Failed reading in mime.cache file: {}", file_path.string());
        return false;
    }

    if (buf.size() < kHeaderSize)
    {
        logger::error<logger::vfs>("Invalid mime.cache file size={}", buf.size());
        return false;
    }

    // Validate file[ALIAS_LIST_OFFSET - 1] is null to ensure that any
    // null-terminated strings dereferenced at addresses below ALIAS_LIST_OFFSET
    // will not overflow.
    uint32_t alias_list_offset = 0;
    if (!ReadInt(buf, 4, "ALIAS_LIST_OFFSET", kHeaderSize, buf.size(), &alias_list_offset))
    {
        return false;
    }
    if (buf[alias_list_offset - 1] != 0)
    {
        logger::error<logger::vfs>(
            "Invalid mime.cache file does not contain null prior to ALIAS_LIST_OFFSET={}",
            alias_list_offset);
        return false;
    }

    // Parse ReverseSuffixTree. Read all nodes and place them on `stack`,
    // allowing max of kMaxNodes and max extension of kMaxExtSize.
    uint32_t tree_offset = 0;
    if (!ReadInt(buf, 16, "REVERSE_SUFFIX_TREE_OFFSET", kHeaderSize, buf.size(), &tree_offset))
    {
        return false;
    }

    struct Node final
    {
        std::string ext;
        uint32_t n_children{0};
        uint32_t first_child_offset{0};
    };

    // Read root node and put it on the stack.
    Node root;
    if (!ReadInt(buf, tree_offset, "N_ROOTS", 0, kMaxUnicode, &root.n_children))
    {
        return false;
    }
    if (!ReadInt(buf,
                 tree_offset + 4,
                 "FIRST_ROOT_OFFSET",
                 tree_offset,
                 buf.size(),
                 &root.first_child_offset))
    {
        return false;
    }
    std::stack<Node> stack;
    stack.push(std::move(root));

    uint32_t num_nodes = 0;
    while (!stack.empty())
    {
        // Pop top node from the stack and process children.
        Node n = std::move(stack.top());
        stack.pop();
        uint32_t p = n.first_child_offset;
        for (uint32_t i = 0; i < n.n_children; i++)
        {
            uint32_t c = 0;
            if (!ReadInt(buf, p, "CHARACTER", 0, kMaxUnicode, &c))
            {
                return false;
            }
            p += 4;

            // Leaf node, add mime type if it is highest weight.
            if (c == 0)
            {
                uint32_t mime_type_offset = 0;
                if (!ReadInt(buf,
                             p,
                             "mime type offset",
                             kHeaderSize,
                             alias_list_offset - 1,
                             &mime_type_offset))
                {
                    return false;
                }
                p += 4;
                uint8_t weight = kDefaultGlobWeight;
                if ((p + 3) < buf.size())
                {
                    weight = static_cast<uint8_t>(buf[p + 3]);
                }
                p += 4;
                if (!n.ext.empty() && n.ext[0] == '.')
                {
                    const std::string ext = n.ext.substr(1);
                    auto it = out_mime_types.find(ext);
                    if (it == out_mime_types.cend() || weight > it->second.weight)
                    {
                        out_mime_types[ext] = {std::string(buf.c_str() + mime_type_offset), weight};
                    }
                }
                continue;
            }

            // Regular node, parse and add it to the stack.
            Node node;
            WriteUnicodeCharacter(static_cast<int>(c), &node.ext);
            node.ext += n.ext;
            if (!ReadInt(buf, p, "N_CHILDREN", 0, kMaxUnicode, &node.n_children))
            {
                return false;
            }
            p += 4;
            if (!ReadInt(buf,
                         p,
                         "FIRST_CHILD_OFFSET",
                         tree_offset,
                         buf.size(),
                         &node.first_child_offset))
            {
                return false;
            }
            p += 4;

            // Check limits.
            if (++num_nodes > kMaxNodes)
            {
                logger::error<logger::vfs>("Exceeded maxium number of nodes={}", kMaxNodes);
                return false;
            }
            if (node.ext.size() > kMaxExtSize)
            {
                logger::warn<logger::vfs>("Ignoring large extension exceeds size={} ext={}",
                                          kMaxExtSize,
                                          node.ext);
                continue;
            }

            stack.push(std::move(node));
        }
    }

    return true;
}

std::string
vfs::detail::mime_type::chrome::GetFileMimeType(const std::filesystem::path& filepath) noexcept
{
    const auto [_, extension] = vfs::utils::filename_stem_and_extension(filepath);
    if (extension.empty())
    {
        return "application/octet-stream";
    }

    static std::vector<FileInfo> xdg_mime_files;

    static MimeTypeMap mime_type_map(
        []
        {
            MimeTypeMap map;
            LoadAllMimeCacheFiles(map, xdg_mime_files);
            return map;
        }());

    // match xdgmime behavior and check every 5s and reload if any files have changed.
    static std::chrono::system_clock::time_point last_check;
    // Lock is required since this may be called on any thread.
    static std::mutex lock;
    {
        const std::scoped_lock<std::mutex> scoped_lock(lock);

        const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        if (last_check + std::chrono::seconds(5) < now)
        {
            if (std::ranges::any_of(xdg_mime_files,
                                    [](const FileInfo& file_info)
                                    {
                                        const auto info = ztd::stat::create(file_info.path);
                                        if (info)
                                        {
                                            return file_info.last_modified != info->mtime();
                                        }
                                        return false;
                                    }))
            {
                mime_type_map.clear();
                xdg_mime_files.clear();
                LoadAllMimeCacheFiles(mime_type_map, xdg_mime_files);
            }
            last_check = now;
        }
    }

    const auto it = mime_type_map.find(extension.substr(1));
    return it != mime_type_map.cend() ? it->second.mime_type
                                      : std::string("application/octet-stream");
}
