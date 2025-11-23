// --- Input port --- //
static unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// --- VGA constants --- //
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define CELL_CHAR 'O'
#define FOOD_CHAR '*'
#define EMPTY_CHAR ' '

typedef struct {
    int x, y;
} Point;

// Snake storage max size
static Point snake[400];
static int snake_len = 3;

// Directions
enum { UP, DOWN, LEFT, RIGHT };
static int dir = RIGHT;

// Simple RNG
static unsigned int seed = 1234567;
static int rand_next() {
    seed = seed * 1664525 + 1013904223;
    return (seed >> 16) & 0x7FFF;
}

// VGA write
static inline void putc_xy(int x, int y, char c, unsigned char color) {
    volatile unsigned char *vga = (unsigned char*)0xB8000;
    int idx = (y * VGA_WIDTH + x) * 2;
    vga[idx] = c;
    vga[idx + 1] = color;
}

// Keyboard → return direction
static void poll_key() {
    unsigned char sc = inb(0x60);

    if (sc == 0x01) {
        // ESC
        snake_len = -1;
        return;
    }

    switch (sc) {
        case 0x48: if (dir != DOWN) dir = UP; break;    // ↑
        case 0x50: if (dir != UP) dir = DOWN; break;    // ↓
        case 0x4B: if (dir != RIGHT) dir = LEFT; break; // ←
        case 0x4D: if (dir != LEFT) dir = RIGHT; break; // →
    }
}

// Draw snake + food
static void render(Point food) {
    // Clear screen
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            putc_xy(x, y, EMPTY_CHAR, 0x07);

    // Draw food
    putc_xy(food.x, food.y, FOOD_CHAR, 0x0C);

    // Draw snake
    for (int i = 0; i < snake_len; i++)
        putc_xy(snake[i].x, snake[i].y, CELL_CHAR, 0x0A);
}

static void delay() {
    for (volatile int i = 0; i < 10000000; i++);
}

// Move snake 1 step
static int move_snake(Point *food) {
    // Move body
    for (int i = snake_len - 1; i > 0; i--)
        snake[i] = snake[i - 1];

    // Move head
    if (dir == UP) snake[0].y--;
    if (dir == DOWN) snake[0].y++;
    if (dir == LEFT) snake[0].x--;
    if (dir == RIGHT) snake[0].x++;

    // Collision with walls
    if (snake[0].x < 0 || snake[0].x >= VGA_WIDTH ||
        snake[0].y < 0 || snake[0].y >= VGA_HEIGHT)
        return 0;

    // Collision with self
    for (int i = 1; i < snake_len; i++)
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y)
            return 0;

    // Eat food
    if (snake[0].x == food->x && snake[0].y == food->y) {
        snake_len++;
        // Generate new food
        food->x = rand_next() % VGA_WIDTH;
        food->y = rand_next() % VGA_HEIGHT;
    }

    return 1;
}

// ===================== MAIN ENTRY ===================== //

void _start(void) {

    // Init snake in center
    int cx = VGA_WIDTH / 2;
    int cy = VGA_HEIGHT / 2;

    snake[0] = (Point){cx, cy};
    snake[1] = (Point){cx - 1, cy};
    snake[2] = (Point){cx - 2, cy};

    Point food = { rand_next() % VGA_WIDTH, rand_next() % VGA_HEIGHT };

    // Main loop
    while (1) {

        poll_key();
        if (snake_len < 0) return; // ESC quit

        if (!move_snake(&food)) break; // Died

        render(food);
        delay();
    }

    // Game over
    const char *msg = "Game Over! Press ESC...";
    int x = (VGA_WIDTH - 24) / 2;
    int y = VGA_HEIGHT / 2;

    for (int i = 0; msg[i]; i++)
        putc_xy(x + i, y, msg[i], 0x0F);

    // Wait ESC
    while (1) {
        unsigned char sc = inb(0x60);
        if (sc == 0x01) return;
    }
}
