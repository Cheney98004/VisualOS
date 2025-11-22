#include "shell.h"
#include "fs.h"
#include "input.h"
#include "terminal.h"
#include "pmm.h"

#define SHELL_BUF 128
#define MAX_ARGS  16

// ---------------------------------------------------------
// Basic string helpers
// ---------------------------------------------------------
static size_t str_len(const char *s) {
    size_t n = 0;
    while (s && s[n]) ++n;
    return n;
}

static int str_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        ++a; ++b;
    }
    return *a == *b;
}

static char *skip_spaces(char *p) {
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

// ---------------------------------------------------------
// Argument parser
// ---------------------------------------------------------
static int parse_args(char *input, char **argv, int max_args) {
    int argc = 0;
    char *p = skip_spaces(input);

    while (*p && argc < max_args) {
        argv[argc++] = p;

        while (*p && *p != ' ' && *p != '\t')
            ++p;

        if (!*p) break;

        *p++ = '\0';
        p = skip_spaces(p);
    }

    return argc;
}

// ---------------------------------------------------------
// Command handlers
// ---------------------------------------------------------
static void cmd_ls(int argc, char **argv, const FsNode **cwd) {
    const char *path = (argc > 1 ? argv[1] : NULL);
    const FsNode *target = (path ? fs_resolve(*cwd, path) : *cwd);

    if (!target) {
        terminal_error();
        terminal_printf("ls: path not found: %s\n", path);
        return;
    }
    fs_list(target);
}

static void cmd_cat(int argc, char **argv, const FsNode **cwd) {
    if (argc < 2) {
        terminal_error();
        terminal_write_line("cat: missing path");
        return;
    }

    const FsNode *target = fs_resolve(*cwd, argv[1]);
    if (!target) {
        terminal_error();
        terminal_printf("cat: no such file: %s\n", argv[1]);
        return;
    }
    fs_dump(target);
}

static void cmd_cd(int argc, char **argv, const FsNode **cwd) {
    if (argc < 2) {
        *cwd = fs_root();
        return;
    }

    const FsNode *target = fs_resolve(*cwd, argv[1]);
    if (!target) {
        terminal_error();
        terminal_printf("cd: no such directory: %s\n", argv[1]);
        return;
    }

    if (target->type != FS_NODE_DIR) {
        terminal_error();
        terminal_printf("cd: not a directory: %s\n", argv[1]);
        return;
    }

    *cwd = target;
}

static void cmd_pwd(int argc, char **argv, const FsNode **cwd) {
    const char *p = (*cwd)->path;
    terminal_write_line(p ? p : "/");
}

static void cmd_echo(int argc, char **argv, const FsNode **cwd) {
    (void)cwd;

    for (int i = 1; i < argc; ++i) {
        terminal_write(argv[i]);
        if (i + 1 < argc)
            terminal_putc(' ');
    }
    terminal_putc('\n');
}

static void cmd_clear() {
    terminal_clear();
}

static void cmd_help(int argc, char **argv, const FsNode **cwd);

static void cmd_mkdir(int argc, char **argv, const FsNode **cwd) {
    if (argc < 2) {
        terminal_error();
        terminal_write_line("mkdir: missing operand");
        return;
    }

    int recursive = 0;
    const char *path = NULL;

    if (str_eq(argv[1], "-p")) {
        if (argc < 3) {
            terminal_error();
            terminal_write_line("mkdir: missing operand after -p");
            return;
        }
        recursive = 1;
        path = argv[2];
    } else {
        path = argv[1];
    }

    FsNode *res = fs_mkdir((FsNode*)*cwd, path, recursive);
    if (!res) {
        terminal_error();
        terminal_printf("mkdir: cannot create directory '%s'\n", path);
    }
}

static void cmd_touch(int argc, char **argv, const FsNode **cwd) {
    if (argc < 2) {
        terminal_error();
        terminal_write_line("touch: missing operand");
        return;
    }

    for (int i = 1; i < argc; i++) {
        if (!fs_touch((FsNode*)*cwd, argv[i])) {
            terminal_error();
            terminal_printf("touch: cannot create '%s'\n", argv[i]);
            return;
        }
    }
}

static void cmd_rm(int argc, char **argv, const FsNode **cwd) {
    if (argc < 2) {
        terminal_error();
        terminal_write_line("rm: missing operand");
        return;
    }

    if (fs_rm((FsNode*)*cwd, argv[1]) != 0) {
        terminal_error();
        terminal_printf("rm: cannot remove '%s'\n", argv[1]);
    }
}

/* ==========================================================
   mem
   顯示 PMM 目前狀態
   ========================================================== */
static void cmd_mem(int argc, char **argv, const FsNode **cwd) {
    (void)cwd; (void)argc; (void)argv;

    uint32_t total = pmm_total_frames() * 4; // KB
    uint32_t used  = pmm_used_frames()  * 4; // KB
    uint32_t free  = pmm_free_frames()  * 4;

    terminal_printf("total: %u KB\n", total);
    terminal_printf("used : %u KB\n", used);
    terminal_printf("free : %u KB\n", free);
}

// ---------------------------------------------------------
// Command table
// ---------------------------------------------------------
typedef struct {
    const char *name;
    void (*handler)(int argc, char **argv, const FsNode **cwd);
    const char *help;
} ShellCommand;

static ShellCommand commands[] = {
    { "help",  cmd_help,  "help             - Show this help" },
    { "ls",    cmd_ls,    "ls [path]        - List directory" },
    { "cat",   cmd_cat,   "cat <path>       - Print file contents" },
    { "cd",    cmd_cd,    "cd <path>        - Change directory" },
    { "pwd",   cmd_pwd,   "pwd              - Show current working directory" },
    { "clear", cmd_clear, "clear            - Clear terminal"},
    { "echo",  cmd_echo,  "echo [args...]   - Print text" },
    { "touch", cmd_touch, "touch <file>     - Create empty file" },
    { "mkdir", cmd_mkdir, "mkdir [-p] <dir> - Create directory" },
    { "rm",    cmd_rm,    "rm <file>        - Remove file or directory" },
    { "mem",   cmd_mem,   "mem              - Show memory usage" },
};

static const size_t command_count = sizeof(commands) / sizeof(commands[0]);

static void cmd_help(int argc, char **argv, const FsNode **cwd) {
    (void)argc; (void)argv; (void)cwd;

    terminal_write_line("Commands:");
    for (size_t i = 0; i < command_count; ++i)
        terminal_write_line(commands[i].help);
}

// ---------------------------------------------------------
// Prompt
// ---------------------------------------------------------
static void render_prompt(const FsNode *cwd) {
    const char *p = (cwd && cwd->path[0]) ? cwd->path : "/";
    terminal_printf("visualos:%s$ ", p);
}

// ---------------------------------------------------------
// Shell main loop
// ---------------------------------------------------------
void shell_run(void) {
    char buffer[SHELL_BUF];
    const FsNode *cwd = fs_root();

    terminal_write_line("Shell ready. Type 'help' for commands.");

    for (;;) {
        render_prompt(cwd);

        size_t len = input_read_line(buffer, sizeof(buffer));
        if (len == 0) continue;

        // Strip newline
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
            buffer[--len] = '\0';

        // Parse args
        char *argv[MAX_ARGS];
        int argc = parse_args(buffer, argv, MAX_ARGS);

        if (argc == 0) continue;

        const char *cmd = argv[0];

        int found = 0;
        for (size_t i = 0; i < command_count; ++i) {
            if (str_eq(cmd, commands[i].name)) {
                commands[i].handler(argc, argv, &cwd);
                found = 1;
                break;
            }
        }

        if (!found) {
            terminal_error();
            terminal_printf("Unknown command: %s\n", cmd);
        }
    }
}
