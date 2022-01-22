/*
 *
 * License: See COPYING file
 *
 */

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

#include <fcntl.h>

#include "settings.hxx"
#include "extern.hxx"

#include "logger.hxx"
#include "utils.hxx"

void
print_command(const std::string& command)
{
    LOG_INFO("COMMAND={}", command);
}

void
print_task_command(const char* ptask, const char* cmd)
{
    LOG_INFO("TASK_COMMAND({:p})={}", ptask, cmd);
}

void
print_task_command_spawn(char* argv[], int pid)
{
    LOG_INFO("SPAWN=");
    int i = 0;
    while (argv[i])
    {
        LOG_INFO("  {}", argv[i]);
        i++;
    }

    LOG_INFO("    pid = {}", pid);
}

char*
randhex8()
{
    char hex[9];
    unsigned int n = mrand48();
    snprintf(hex, sizeof(hex), "%08x", n);
    // LOG_INFO("rand  : {}", n);
    // LOG_INFO("hex   : {}", hex);
    return g_strdup(hex);
}

char*
replace_line_subs(const char* line)
{
    const char* perc[] =
        {"%f", "%F", "%n", "%N", "%d", "%D", "%v", "%l", "%m", "%y", "%b", "%t", "%p", "%a"};
    const char* var[] = {"\"${fm_file}\"",
                         "\"${fm_files[@]}\"",
                         "\"${fm_filename}\"",
                         "\"${fm_filenames[@]}\"",
                         "\"${fm_pwd}\"",
                         "\"${fm_pwd}\"",
                         "\"${fm_device}\"",
                         "\"${fm_device_label}\"",
                         "\"${fm_device_mount_point}\"",
                         "\"${fm_device_fstype}\"",
                         "\"${fm_bookmark}\"",
                         "\"${fm_task_pwd}\"",
                         "\"${fm_task_pid}\"",
                         "\"${fm_value}\""};

    char* s = g_strdup(line);
    int num = G_N_ELEMENTS(perc);
    int i;
    for (i = 0; i < num; i++)
    {
        if (strstr(line, perc[i]))
        {
            char* old_s = s;
            s = replace_string(old_s, perc[i], var[i], false);
            g_free(old_s);
        }
    }
    return s;
}

bool
have_x_access(const char* path)
{
    if (!path)
        return false;
    return (faccessat(0, path, R_OK | X_OK, AT_EACCESS) == 0);
}

bool
have_rw_access(const char* path)
{
    if (!path)
        return false;
    return (faccessat(0, path, R_OK | W_OK, AT_EACCESS) == 0);
}

bool
dir_has_files(const char* path)
{
    if (!path)
        return false;

    if (!std::filesystem::exists(path))
        return false;

    if (!std::filesystem::is_directory(path))
        return false;

    for (const auto& file: std::filesystem::directory_iterator(path))
    {
        if (file.exists())
            return true;
    }

    return false;
}

char*
get_name_extension(const char* full_name, bool is_dir, char** ext)
{
    char* str;
    char* full = g_strdup(full_name);
    // get last dot
    char* dot;
    if (is_dir || !(dot = strrchr(full, '.')) || dot == full)
    {
        // dir or no dots or one dot first
        *ext = nullptr;
        return full;
    }
    dot[0] = '\0';
    char* final_ext = dot + 1;
    // get previous dot
    dot = strrchr(full, '.');
    unsigned int final_ext_len = strlen(final_ext);
    if (dot && !strcmp(dot + 1, "tar") && final_ext_len < 11 && final_ext_len)
    {
        // double extension
        final_ext[-1] = '.';
        *ext = g_strdup(dot + 1);
        dot[0] = '\0';
        str = g_strdup(full);
        g_free(full);
        return str;
    }
    // single extension, one or more dots
    if (final_ext_len < 11 && final_ext[0])
    {
        *ext = g_strdup(final_ext);
        str = g_strdup(full);
        g_free(full);
        return str;
    }
    else
    {
        // extension too long, probably part of name
        final_ext[-1] = '.';
        *ext = nullptr;
        return full;
    }
}

void
open_in_prog(const char* path)
{
    char* prog = g_find_program_in_path(g_get_prgname());
    if (!prog)
        prog = g_strdup(g_get_prgname());
    if (!prog)
        prog = g_strdup("spacefm");
    char* qpath = bash_quote(path);
    std::string command = fmt::format("{} {}", prog, qpath);
    print_command(command);
    g_spawn_command_line_async(command.c_str(), nullptr);
    g_free(qpath);
    g_free(prog);
}

char*
replace_string(const char* orig, const char* str, const char* replace, bool quote)
{ // replace all occurrences of str in orig with replace, optionally quoting
    char* rep;
    char* result = nullptr;
    char* old_result;
    const char* s;

    if (!orig || !(s = strstr(orig, str)))
        return g_strdup(orig); // str not in orig

    if (!replace)
    {
        if (quote)
            rep = g_strdup("''");
        else
            rep = g_strdup("");
    }
    else if (quote)
        rep = g_strdup_printf("'%s'", replace);
    else
        rep = g_strdup(replace);

    const char* cur = orig;
    do
    {
        if (result)
        {
            old_result = result;
            result = g_strdup_printf("%s%s%s", old_result, g_strndup(cur, s - cur), rep);
            g_free(old_result);
        }
        else
            result = g_strdup_printf("%s%s", g_strndup(cur, s - cur), rep);
        cur = s + strlen(str);
        s = strstr(cur, str);
    } while (s);
    old_result = result;
    result = g_strdup_printf("%s%s", old_result, cur);
    g_free(old_result);
    g_free(rep);
    return result;
}

char*
bash_quote(const char* str)
{
    if (!str)
        return g_strdup("\"\"");
    char* s1 = replace_string(str, "\"", "\\\"", false);
    char* s2 = g_strdup_printf("\"%s\"", s1);
    g_free(s1);
    return s2;
}

char*
plain_ascii_name(const char* orig_name)
{
    if (!orig_name)
        return g_strdup("");
    char* orig = g_strdup(orig_name);
    g_strcanon(orig, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_", ' ');
    char* s = replace_string(orig, " ", "", false);
    g_free(orig);
    return s;
}

char*
clean_label(const char* menu_label, bool kill_special, bool escape)
{
    char* s1;
    char* s2;
    if (menu_label && strstr(menu_label, "\\_"))
    {
        s1 = replace_string(menu_label, "\\_", "@UNDERSCORE@", false);
        s2 = replace_string(s1, "_", "", false);
        g_free(s1);
        s1 = replace_string(s2, "@UNDERSCORE@", "_", false);
        g_free(s2);
    }
    else
        s1 = replace_string(menu_label, "_", "", false);
    if (kill_special)
    {
        s2 = replace_string(s1, "&", "", false);
        g_free(s1);
        s1 = replace_string(s2, " ", "-", false);
        g_free(s2);
    }
    else if (escape)
    {
        s2 = g_markup_escape_text(s1, -1);
        g_free(s1);
        s1 = s2;
    }
    return s1;
}

void
string_copy_free(char** s, const char* src)
{
    char* discard = *s;
    *s = g_strdup(src);
    g_free(discard);
}

char*
unescape(const char* t)
{
    if (!t)
        return nullptr;

    char* s = g_strdup(t);

    int i = 0, j = 0;
    while (t[i])
    {
        switch (t[i])
        {
            case '\\':
                switch (t[++i])
                {
                    case 'n':
                        s[j] = '\n';
                        break;
                    case 't':
                        s[j] = '\t';
                        break;
                    case '\\':
                        s[j] = '\\';
                        break;
                    case '\"':
                        s[j] = '\"';
                        break;
                    default:
                        // copy
                        s[j++] = '\\';
                        s[j] = t[i];
                }
                break;
            default:
                s[j] = t[i];
        }
        ++i;
        ++j;
    }
    s[j] = t[i]; // null char
    return s;
}

char*
get_valid_su() // may return nullptr
{
    unsigned int i;
    char* use_su = nullptr;

    use_su = g_strdup(xset_get_s("su_command"));

    if (use_su)
    {
        // is Prefs use_su in list of valid su commands?
        for (i = 0; i < G_N_ELEMENTS(su_commands); i++)
        {
            if (!strcmp(su_commands[i], use_su))
                break;
        }
        if (i == G_N_ELEMENTS(su_commands))
        {
            // not in list - invalid
            g_free(use_su);
            use_su = nullptr;
        }
    }
    if (!use_su)
    {
        // discovery
        for (i = 0; i < G_N_ELEMENTS(su_commands); i++)
        {
            if ((use_su = g_find_program_in_path(su_commands[i])))
                break;
        }
        if (!use_su)
            use_su = g_strdup(su_commands[0]);
        xset_set("su_command", "s", use_su);
    }
    char* su_path = g_find_program_in_path(use_su);
    g_free(use_su);
    return su_path;
}
