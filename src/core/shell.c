#include "shell.h"
#include "fs.h"
#include "input.h"
#include "terminal.h"
#include "string.h"

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

static void cmd_ls() {
    fs_list();
}

static void cmd_pwd() {
    terminal_write_line("/");
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

    for (uint32_t i = 0; i < r; i++)
        terminal_putc(buf[i]);

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

// ---------------------------------------------------------
// Prompt
// ---------------------------------------------------------
static void render_prompt() {
    terminal_write("visualos:/ $ ");
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
        else if (str_eq(argv[0], "ls"))      cmd_ls();
        else if (str_eq(argv[0], "pwd"))     cmd_pwd();
        else if (str_eq(argv[0], "cat"))     cmd_cat(argc, argv);
        else if (str_eq(argv[0], "touch"))   cmd_touch(argc, argv);
        else if (str_eq(argv[0], "rm"))      cmd_rm(argc, argv);
        else if (str_eq(argv[0], "write"))   cmd_write(argc, argv);
        else if (str_eq(argv[0], "clear"))   terminal_clear();

        else {
            terminal_error();
            terminal_write("Unknown command: ");
            terminal_write_line(argv[0]);
        }
    }
}
