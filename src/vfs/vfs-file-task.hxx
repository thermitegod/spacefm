/*
 *  C Interface: vfs-file-task
 *
 * Description:
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum VFSFileTaskType
{
    VFS_FILE_TASK_MOVE = 0,
    VFS_FILE_TASK_COPY,
    VFS_FILE_TASK_DELETE,
    VFS_FILE_TASK_LINK,
    VFS_FILE_TASK_CHMOD_CHOWN, /* These two kinds of operation have lots in common,
                                * so put them together to reduce duplicated disk I/O */
    VFS_FILE_TASK_EXEC,        // MOD
    VFS_FILE_TASK_LAST
} VFSFileTaskType;

typedef enum ChmodActionType
{
    OWNER_R = 0,
    OWNER_W,
    OWNER_X,
    GROUP_R,
    GROUP_W,
    GROUP_X,
    OTHER_R,
    OTHER_W,
    OTHER_X,
    SET_UID,
    SET_GID,
    STICKY,
    N_CHMOD_ACTIONS
} ChmodActionType;

extern const mode_t chmod_flags[];

typedef enum VFSFileTaskState
{
    VFS_FILE_TASK_RUNNING,
    VFS_FILE_TASK_SIZE_TIMEOUT,
    VFS_FILE_TASK_QUERY_OVERWRITE,
    VFS_FILE_TASK_ERROR,
    VFS_FILE_TASK_PAUSE,
    VFS_FILE_TASK_QUEUE,
    VFS_FILE_TASK_FINISH
} VFSFileTaskState;

typedef enum VFSFileTaskOverwriteMode
{
    // do not reposition first four values
    VFS_FILE_TASK_OVERWRITE,     /* Overwrite current dest file / Ask */
    VFS_FILE_TASK_OVERWRITE_ALL, /* Overwrite all existing files without prompt */
    VFS_FILE_TASK_SKIP_ALL,      /* Don't try to overwrite any files */
    VFS_FILE_TASK_AUTO_RENAME,   /* Assign a new unique name */
    VFS_FILE_TASK_SKIP,          /* Don't overwrite current file */
    VFS_FILE_TASK_RENAME         /* Rename file */
} VFSFileTaskOverwriteMode;

typedef enum VFSExecType
{
    VFS_EXEC_NORMAL,
    VFS_EXEC_CUSTOM,
} VFSExecType;

typedef struct VFSFileTask VFSFileTask;

typedef bool (*VFSFileTaskStateCallback)(VFSFileTask*, VFSFileTaskState state, void* state_data,
                                         void* user_data);

typedef struct VFSFileTask
{
    VFSFileTaskType type;
    GList* src_paths; /* All source files. This list will be freed
                                 after file operation is completed. */
    char* dest_dir;   /* Destinaton directory */
    bool avoid_changes;
    GSList* devs;

    VFSFileTaskOverwriteMode overwrite_mode;
    bool recursive; /* Apply operation to all files under directories
                     * recursively. This is default to copy/delete,
                     * and should be set manually for chown/chmod. */

    /* For chown */
    uid_t uid;
    gid_t gid;

    /* For chmod */
    unsigned char* chmod_actions; /* If chmod is not needed, this should be nullptr */

    off_t total_size; /* Total size of the files to be processed, in bytes */
    off_t progress;   /* Total size of current processed files, in btytes */
    int percent;      /* progress (percentage) */
    bool custom_percent;
    time_t start_time;
    off_t last_speed;
    off_t last_progress;
    GTimer* timer;
    double last_elapsed;
    unsigned int current_item;
    int err_count;

    char* current_file; /* copy of Current processed file */
    char* current_dest; /* copy of Current destination file */

    int error;
    bool error_first;

    GThread* thread;
    VFSFileTaskState state;
    VFSFileTaskState state_pause;
    bool abort;
    GCond* pause_cond;
    bool queue_start;

    VFSFileTaskStateCallback state_cb;
    void* state_cb_data;

    GMutex* mutex;

    // sfm write directly to gtk buffer for speed
    GtkTextBuffer* add_log_buf;
    GtkTextMark* add_log_end;

    // MOD run task
    VFSExecType exec_type;
    char* exec_action;
    char* exec_command;
    bool exec_sync;
    bool exec_popup;
    bool exec_show_output;
    bool exec_show_error;
    bool exec_terminal;
    bool exec_keep_terminal;
    bool exec_export;
    bool exec_direct;
    char* exec_argv[7]; // for exec_direct, command ignored
                        // for su commands, must use bash -c
                        // as su does not execute binaries
    char* exec_script;
    bool exec_keep_tmp; // diagnostic to keep temp files
    void* exec_browser;
    void* exec_desktop;
    char* exec_as_user;
    char* exec_icon;
    GPid exec_pid;
    int exec_exit_status;
    unsigned int child_watch;
    bool exec_is_error;
    GIOChannel* exec_channel_out;
    GIOChannel* exec_channel_err;
    bool exec_scroll_lock;
    bool exec_write_root;
    bool exec_checksum;
    void* exec_set;
    GCond* exec_cond;
    void* exec_ptask;
} VFSFileTask;

/*
 * source_files sould be a newly allocated list, and it will be
 * freed after file operation has been completed
 */
VFSFileTask* vfs_task_new(VFSFileTaskType task_type, GList* src_files, const char* dest_dir);

void vfs_file_task_lock(VFSFileTask* task);
void vfs_file_task_unlock(VFSFileTask* task);

/* Set some actions for chmod, this array will be copied
 * and stored in VFSFileTask */
void vfs_file_task_set_chmod(VFSFileTask* task, unsigned char* chmod_actions);

void vfs_file_task_set_chown(VFSFileTask* task, uid_t uid, gid_t gid);

void vfs_file_task_set_state_callback(VFSFileTask* task, VFSFileTaskStateCallback cb,
                                      void* user_data);

void vfs_file_task_set_recursive(VFSFileTask* task, bool recursive);

void vfs_file_task_set_overwrite_mode(VFSFileTask* task, VFSFileTaskOverwriteMode mode);

void vfs_file_task_run(VFSFileTask* task);

void vfs_file_task_try_abort(VFSFileTask* task);

void vfs_file_task_abort(VFSFileTask* task);

void vfs_file_task_free(VFSFileTask* task);

char* vfs_file_task_get_cpids(GPid pid);
void vfs_file_task_kill_cpids(char* cpids, int signal);
char* vfs_file_task_get_unique_name(const char* dest_dir, const char* base_name, const char* ext);

G_END_DECLS
