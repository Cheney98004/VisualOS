#include "shell.h"
#include "fs.h"
#include "input.h"
#include "terminal.h"
#include "string.h"
#include "fat16.h"
#include "pmm.h"

#define SHELL_BUF 128
#define MAX_ARGS  16

// ---------------------------------------------------------
// Basic helpers
// ---------------------------------------------------------
static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static char *skip_spaces(char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static int parse_args(char *input, char **argv, int max) {
    int argc = 0;
    char *p = skip_spaces(input);

    while (*p && argc < max) {
        argv[argc++] = p;

        while (*p && *p != ' ' && *p != '\t')
            p++;

        if (!*p) break;

        *p++ = '\0';
        p = skip_spaces(p);
    }
    return argc;
}

// ---------------------------------------------------------
// Commands
// ---------------------------------------------------------

static void cmd_help() {
    terminal_write_line("Commands:");
    terminal_write_line("  help           - Show this help");
    terminal_write_line("  ls             - List files");
    terminal_write_line("  cat <file>     - Print file contents");
    terminal_write_line("  touch <file>   - Create empty file");
    terminal_write_line("  rm <file>      - Delete file");
    terminal_write_line("  write <f> <t>  - Write text to file");
    terminal_write_line("  pwd            - Show current directory");
    terminal_write_line("  clear          - Clear screen");
}

static void cmd_ls(int argc, char **argv) {
    int opt_long = 0;   // -l
    int opt_all  = 0;   // -a

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            const char *p = argv[i] + 1;

            while (*p) {
                if (*p == 'l') opt_long = 1;
                else if (*p == 'a') opt_all = 1;
                p++;
            }
        }
    }

    if (opt_long) {
        fs_list_long(opt_all);
    } else {
        fs_list(opt_all);
    }
}

static void cmd_pwd() {
    char path[128];
    k_memset(path, 0, sizeof(path));
    fat16_get_path(path);
    terminal_write_line(path);
}

static void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        terminal_error();
        terminal_write_line("cat: missing filename");
        return;
    }

    char buf[2048];
    uint32_t r = fs_read(argv[1], buf, sizeof(buf));

    if (r == 0) {
        terminal_error();
        terminal_write("cat: cannot read ");
        terminal_write(argv[1]);
        terminal_putc('\n');
        return;
    }

    for (uint32_t i = 0; i < r; i++) {
        if (buf[i] == '\r') continue;
        terminal_putc(buf[i]);
    }

    terminal_putc('\n');
}

static void cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        terminal_error();
        terminal_write_line("touch: missing file");
        return;
    }

    if (!fs_create(argv[1])) {
        terminal_error();
        terminal_write("touch: cannot create ");
        terminal_write(argv[1]);
        terminal_putc('\n');
    }
}

static void cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        terminal_error();
        terminal_write_line("rm: missing filename");
        return;
    }

    if (!fs_delete(argv[1])) {
        terminal_error();
        terminal_write("rm: cannot delete ");
        terminal_write(argv[1]);
        terminal_putc('\n');
    }
}

static void cmd_write(int argc, char **argv) {
    if (argc < 3) {
        terminal_error();
        terminal_write_line("write: usage: write <file> <text>");
        return;
    }

    const char *filename = argv[1];
    const char *text     = argv[2];

    if (!fs_write(filename, text, k_strlen(text))) {
        terminal_error();
        terminal_write("write: cannot write to ");
        terminal_write(filename);
        terminal_putc('\n');
    }
}

static void cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        terminal_error();
        terminal_write_line("mkdir: missing operand");
        return;
    }

    if (!fs_mkdir(argv[1])) {
        terminal_error();
        terminal_write("mkdir: cannot create ");
        terminal_write(argv[1]);
        terminal_putc('\n');
    }
}

static void cmd_cd(int argc, char **argv) {
    if (argc < 2) {
        terminal_error();
        terminal_write_line("cd: missing directory");
        return;
    }

    if (!fs_cd(argv[1])) {
        terminal_error();
        terminal_write("cd: no such directory: ");
        terminal_write(argv[1]);
        terminal_putc('\n');
    }
}

static void cmd_chmod(int argc, char **argv) {
    if (argc < 3) {
        terminal_error();
        terminal_write_line("chmod: usage: chmod [+rwxhsi] <file>");
        return;
    }

    char op = argv[1][0];
    const char *flags = argv[1] + 1;
    const char *filename = argv[2];

    Fat16DirEntry e;
    uint32_t lba;
    int index;

    if (!fs_get_entry(filename, &e, &lba, &index)) {
        terminal_error();
        terminal_write("chmod: no such file: ");
        terminal_write_line(filename);
        return;
    }

    if (e.flags & PERM_I) {
        terminal_error();
        terminal_write("chmod: cannot modify immutable file: ");
        terminal_write_line(filename);
        return;
    }

    while (*flags) {
        uint8_t bit = 0;
        switch (*flags) {
            case 'r': bit = PERM_R; break;
            case 'w': bit = PERM_W; break;
            case 'x': bit = PERM_X; break;
            case 'h': bit = PERM_H; break;
            case 's': bit = PERM_S; break;
            case 'i': bit = PERM_I; break;
        }

        if (op == '+')
            e.flags |= bit;
        else
            e.flags &= ~bit;

        flags++;
    }

    fat16_set_entry(lba, index, &e);
}

static void cmd_mem() {
    uint32_t total = pmm_total_frames() * 4;
    uint32_t used = pmm_used_frames() * 4;
    uint32_t free = pmm_free_frames() * 4;

    terminal_printf(" total: %u KB\n", total);
    terminal_printf(" used : %u KB\n", used);
    terminal_printf(" free : %u KB\n", free);
}

// ---------------------------------------------------------
// Prompt
// ---------------------------------------------------------
static void render_prompt() {
    char path[128];
    k_memset(path, 0, sizeof(path));
    fat16_get_path(path);

    terminal_write("visualos:");
    terminal_write(path);
    terminal_write(" $ ");
}

// ---------------------------------------------------------
// Shell loop
// ---------------------------------------------------------
void shell_run() {
    char buffer[SHELL_BUF];

    terminal_write_line("Shell ready. Type 'help' for commands.");

    for (;;) {
        render_prompt();

        size_t len = input_read_line(buffer, sizeof(buffer));
        if (len == 0) continue;

        // trim newline
        while (len && (buffer[len-1] == '\n' || buffer[len-1] == '\r'))
            buffer[--len] = '\0';

        char *argv[MAX_ARGS];
        int argc = parse_args(buffer, argv, MAX_ARGS);
        if (argc == 0) continue;

        // dispatch
        if (str_eq(argv[0], "help"))         cmd_help();
        else if (str_eq(argv[0], "ls"))      cmd_ls(argc, argv);
        else if (str_eq(argv[0], "pwd"))     cmd_pwd();
        else if (str_eq(argv[0], "cat"))     cmd_cat(argc, argv);
        else if (str_eq(argv[0], "touch"))   cmd_touch(argc, argv);
        else if (str_eq(argv[0], "rm"))      cmd_rm(argc, argv);
        else if (str_eq(argv[0], "write"))   cmd_write(argc, argv);
        else if (str_eq(argv[0], "mkdir"))   cmd_mkdir(argc, argv);
        else if (str_eq(argv[0], "cd"))      cmd_cd(argc, argv);
        else if (str_eq(argv[0], "clear"))   terminal_clear();
        else if (str_eq(argv[0], "chmod"))   cmd_chmod(argc, argv);
        else if (str_eq(argv[0], "mem"))     cmd_mem();

        else {
            terminal_error();
            terminal_write("Unknown command: ");
            terminal_write_line(argv[0]);
        }
    }
}
