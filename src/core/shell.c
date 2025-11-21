#include "shell.h"
#include "fs.h"
#include "input.h"
#include "terminal.h"

#define SHELL_BUF 128
#define MAX_ARGS  16

// ---------------------------------------------------------
// Debug Flag
// ---------------------------------------------------------
static int debug_enabled = 0;

#define DBG_PRINT(...) \
    do { if (debug_enabled) terminal_printf(__VA_ARGS__); } while (0)

#define DBG_WRITE_LINE(...) \
    do { if (debug_enabled) terminal_write_line(__VA_ARGS__); } while (0)

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

// ---------------------------------------------------------
// Debug command handler
// ---------------------------------------------------------
static void cmd_debug(int argc, char **argv, const FsNode **cwd) {
    (void)cwd;

    if (argc < 2) {
        terminal_write_line("debug: missing option (on/off)");
        return;
    }

    if (str_eq(argv[1], "on")) {
        debug_enabled = 1;
        terminal_write_line("Debug mode: ON");
    }
    else if (str_eq(argv[1], "off")) {
        debug_enabled = 0;
        terminal_write_line("Debug mode: OFF");
    }
    else {
        terminal_printf("debug: unknown option: %s\n", argv[1]);
    }
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
    { "touch", cmd_touch, "touch <file>      - Create empty file" },
    { "mkdir", cmd_mkdir, "mkdir [-p] <dir>  - Create directory" },
    { "rm",    cmd_rm,    "rm <file>        - Remove file or directory" },
    { "debug", cmd_debug, "debug on/off     - Enable or disable debug output" },
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

        // Debug output -------------------------------------------------------
        DBG_PRINT("[DBG] raw buffer (%u bytes): ", (unsigned)len);
        if (debug_enabled) {
            for (size_t i = 0; i < len; ++i)
                terminal_printf("%02X ", (unsigned char)buffer[i]);
            terminal_putc('\n');
        }

        DBG_PRINT("[DBG] raw str: '%s'\n", buffer);

        // Strip newline
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
            buffer[--len] = '\0';

        DBG_PRINT("[DBG] after strip: '%s'\n", buffer);

        // Parse args ---------------------------------------------------------
        char *argv[MAX_ARGS];
        int argc = parse_args(buffer, argv, MAX_ARGS);

        DBG_PRINT("[DBG] argc = %d\n", argc);
        if (debug_enabled) {
            for (int i = 0; i < argc; ++i)
                terminal_printf("[DBG] argv[%d] = '%s'\n", i, argv[i]);
        }

        if (argc == 0) {
            DBG_WRITE_LINE("[DBG] argc == 0, continue");
            continue;
        }

        const char *cmd = argv[0];

        // Show available commands -------------------------------------------
        DBG_WRITE_LINE("[DBG] Available commands:");
        if (debug_enabled) {
            for (size_t i = 0; i < command_count; ++i)
                terminal_printf("  [%u] '%s'\n", (unsigned)i, commands[i].name);
        }

        DBG_PRINT("[DBG] comparing cmd '%s'...\n", cmd);

        // Command dispatch --------------------------------------------------
        int found = 0;
        for (size_t i = 0; i < command_count; ++i) {
            DBG_PRINT("[DBG] checking '%s' vs '%s'\n", cmd, commands[i].name);

            if (str_eq(cmd, commands[i].name)) {
                DBG_PRINT("[DBG] MATCH found at index %u\n", (unsigned)i);
                commands[i].handler(argc, argv, &cwd);
                found = 1;
                break;
            }
        }

        if (!found) {
            terminal_error();
            terminal_printf("Unknown command: %s\n", cmd);
        }

        DBG_WRITE_LINE("[DBG] loop end");
    }
}
