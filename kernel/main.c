// intcstOS kernel version z1.1 the CALCULATOR update
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

// multiboot definition
typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  color_info[6];
} __attribute__((packed)) multiboot_info_t;

// framebuffer
static uint32_t* framebuffer = NULL;
static uint32_t screen_width = 0;
static uint32_t screen_height = 0;
static uint32_t screen_pitch = 0;

// color palette 
#define color_desktop_bg 0x001b4d3e
#define color_taskbar    0x002d3748
#define color_button     0x003182ce
#define color_menu_bg    0x001a202c
#define color_window_hdr 0x002b6cb0
#define color_close_btn  0x00e53e3e
#define color_white      0x00ffffff
#define color_black      0x00000000
#define color_green      0x0000ff00
#define color_dark_gray  0x004a5568
#define color_light_gray 0x00718096
#define color_orange     0x00dd6b20

// i/o port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait(void) {
    outb(0x80, 0x00);
}

#include "font.c"

static inline void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= screen_width || y >= screen_height) return;
    framebuffer[(y * (screen_pitch / 4)) + x] = color;
}

static inline uint32_t get_pixel(uint32_t x, uint32_t y) {
    if (x >= screen_width || y >= screen_height) return 0;
    return framebuffer[(y * (screen_pitch / 4)) + x];
}

void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {
            put_pixel(x + j, y + i, color);
        }
    }
}

void draw_char(char c, uint32_t x, uint32_t y, uint32_t fg_color) {
    uint8_t index = (uint8_t)c;
    if (index >= 128) return;
    for (int row = 0; row < 16; row++) {
        uint8_t line = font_8x16[index][row];
        for (int col = 0; col < 8; col++) {
            if (line & (1 << (7 - col))) put_pixel(x + col, y + row, fg_color);
        }
    }
}

void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t fg_color) {
    uint32_t cur_x = x;
    while (*str) {
        if (*str == '\n') { cur_x = x; y += 16; } 
        else { draw_char(*str, cur_x, y, fg_color); cur_x += 8; }
        str++;
    }
}

// ui engine state flags
static bool start_menu_open = false;

// terminal state for example open o boot
static bool window_open = false;
static int win_x = 120;
static int win_y = 80;
static int win_w = 540;
static int win_h = 360;

// calculator state
static bool calc_open = false;
static int calc_x = 220;
static int calc_y = 100;
static int calc_w = 210;
static int calc_h = 270;

static char calc_disp[16] = "0";
static int calc_disp_len = 1;
static int32_t calc_operand = 0;
static char calc_op = 0;
static bool calc_clear_on_next = false;

// window dragging state
enum DragTarget { DRAG_NONE, DRAG_TERM, DRAG_CALC };
static enum DragTarget current_drag = DRAG_NONE;
static int drag_offset_x = 0;
static int drag_offset_y = 0;
static bool prev_left_click = false;

// terminal history
#define TERM_MAX_LINES 12
#define TERM_LINE_LEN 60
static char term_lines[TERM_MAX_LINES][TERM_LINE_LEN];
static uint32_t term_line_count = 0;

#define kb_buf_max 32
static char term_input[kb_buf_max] = {0};
static uint32_t term_input_len = 0;

// ps/2 keyboard driver
static const char scancode_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

/* Terminal Output Buffer Handler */
void term_print(const char* msg) {
    if (term_line_count >= TERM_MAX_LINES) {
        for (uint32_t i = 1; i < TERM_MAX_LINES; i++) {
            for (uint32_t j = 0; j < TERM_LINE_LEN; j++) {
                term_lines[i - 1][j] = term_lines[i][j];
            }
        }
        term_line_count = TERM_MAX_LINES - 1;
    }

    uint32_t idx = 0;
    while (msg[idx] != '\0' && idx < TERM_LINE_LEN - 1) {
        term_lines[term_line_count][idx] = msg[idx];
        idx++;
    }
    term_lines[term_line_count][idx] = '\0';
    term_line_count++;
}

void term_clear(void) {
    term_line_count = 0;
    for (uint32_t i = 0; i < TERM_MAX_LINES; i++) {
        term_lines[i][0] = '\0';
    }
}

static bool strcmp_exact(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

// terminal commands
void execute_command(const char* cmd) {
    char echo_buf[68] = "int> ";
    uint32_t idx = 19;
    uint32_t c_idx = 0;
    while (cmd[c_idx] != '\0' && idx < 67) {
        echo_buf[idx++] = cmd[c_idx++];
    }
    echo_buf[idx] = '\0';
    term_print(echo_buf);

    if (strcmp_exact(cmd, "help")) {
        term_print("Available Commands:");
        term_print("  help    - Show this manual");
        term_print("  clear   - Clear terminal output");
        term_print("  about   - intcstOS kernel details");
        term_print("  fetch   - Display system info ");
        term_print("  calc    - Launch desktop calculator");
        term_print("  reboot  - Restart the computer");
    } else if (strcmp_exact(cmd, "clear")) {
        term_clear();
    } else if (strcmp_exact(cmd, "about")) {
        term_print("32-bits, Open-source, by SondaProject");
    } else if (strcmp_exact(cmd, "fetch")) {
        term_print(" ___ _   _ _____    ");
        term_print("|_ _| \ | |_   _|    OS: intcstOS i686-32");
        term_print(" | ||  \| | | |      Input: PS/2 Dual Mouse/Keyboard Driver");
        term_print(" | || |\  | | |      Display: VBE Framebuffer Pixel Engine");
        term_print("|___|_| \_| |_|  ");
    } else if (strcmp_exact(cmd, "calc")) {
        calc_open = true;
        term_print("Launched Calculator Application.");
    } else if (strcmp_exact(cmd, "reboot")) {
        outb(0x64, 0xfe);
    }else if (strcmp_exact(cmd, "67")) {
        term_print("pak you bradar");
        outb(0x64, 0xfe);
    } else if (cmd[0] != '\0') {
        term_print("The kernel is afraid of you(unknown command)");
    }
}

void draw_desktop_icons(void) {
    // terminal Icon
    uint32_t icon_x = 30;
    uint32_t icon_y = 40;

    draw_rect(icon_x, icon_y, 48, 40, color_black);
    draw_rect(icon_x + 2, icon_y + 2, 44, 36, color_dark_gray);
    draw_string(">", icon_x + 8, icon_y + 12, color_green);
    draw_string("_", icon_x + 20, icon_y + 12, color_white);
    draw_string("Terminal", icon_x - 8, icon_y + 48, color_white);

    // calculator icon
    uint32_t calc_icon_x = 30;
    uint32_t calc_icon_y = 120;

    draw_rect(calc_icon_x, calc_icon_y, 48, 40, color_black);
    draw_rect(calc_icon_x + 2, calc_icon_y + 2, 44, 36, color_light_gray);
    draw_string("12", calc_icon_x + 8, calc_icon_y + 6, color_white);
    draw_string("+=", calc_icon_x + 8, calc_icon_y + 20, color_orange);
    draw_string("Calc", calc_icon_x + 8, calc_icon_y + 48, color_white);
}

void draw_terminal_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!window_open) return;

    // shell and background
    draw_rect(x, y, w, h, color_black);               
    draw_rect(x, y, w, 24, color_window_hdr);        

    // close button
    draw_rect(x + w - 24, y + 2, 20, 20, color_close_btn);
    draw_string("X", x + w - 18, y + 4, color_white);

    // window border
    for (uint32_t i = 0; i < w; i++) {
        put_pixel(x + i, y, color_dark_gray);
        put_pixel(x + i, y + h - 1, color_dark_gray);
    }
    for (uint32_t j = 0; j < h; j++) {
        put_pixel(x, y + j, color_dark_gray);
        put_pixel(x + w - 1, y + j, color_dark_gray);
    }

    draw_string("terminal", x + 8, y + 4, color_white);

    uint32_t start_y = y + 32;
    for (uint32_t i = 0; i < term_line_count; i++) {
        draw_string(term_lines[i], x + 12, start_y + (i * 20), color_white);
    }

    // shell line before input
    uint32_t prompt_y = start_y + (term_line_count * 20);
    if (prompt_y <= y + h - 28) {
        draw_string("intshell> ", x + 9, prompt_y, color_white);
        draw_string(term_input, x + 164, prompt_y, color_green);
        draw_rect(x + 164 + (term_input_len * 8), prompt_y, 8, 16, color_green);
    }
}

#include "calc.c"

// start menu button and start menu
void draw_start_menu(void) {
    if (!start_menu_open) return;
    
    uint32_t menu_x = 10;
    uint32_t menu_y = screen_height - 240;
    uint32_t menu_w = 160;
    uint32_t menu_h = 198;

    draw_rect(menu_x, menu_y, menu_w, menu_h, color_menu_bg);
    
    for (uint32_t i = 0; i < menu_w; i++) {
        put_pixel(menu_x + i, menu_y, color_button);
        put_pixel(menu_x + i, menu_y + menu_h - 1, color_button);
    }
    for (uint32_t j = 0; j < menu_h; j++) {
        put_pixel(menu_x, menu_y + j, color_button);
        put_pixel(menu_x + menu_w - 1, menu_y + j, color_button);
    }

    draw_string("> Terminal", menu_x + 12, menu_y + 12, color_white);
    draw_string("> Calculator", menu_x + 12, menu_y + 48, color_white);
    draw_string("> Clear Screen", menu_x + 12, menu_y + 84, color_white);
    draw_string("> About OS", menu_x + 12, menu_y + 120, color_white);
    draw_string("> Reboot System", menu_x + 12, menu_y + 156, color_white);
}

// desktop
void ui_render_desktop(void) {
    draw_rect(0, 0, screen_width, screen_height, color_desktop_bg);
    draw_desktop_icons();
    
    draw_terminal_window(win_x, win_y, win_w, win_h);
    draw_calculator_window(calc_x, calc_y, calc_w, calc_h);

    // "taskbar"
    draw_rect(0, screen_height - 40, screen_width, 40, color_taskbar);

    // start button
    draw_rect(10, screen_height - 32, 80, 24, color_button);
    draw_string("Start", 28, screen_height - 28, color_white);

    draw_start_menu();
}

// mouse pointer
#define cursor_w 12
#define cursor_h 19
static const char* cursor_sprite[cursor_h] = {
    "XX            "
    "X.X           ",
    "X..X          ",
    "X...X         ",
    "X....X        ",
    "X.....X       ",
    "X......X      ",
    "X.......X     ",
    "X........X    ",
    "X.........X   ",
    "X..........X  ",
    "X.......XXXX  ",
    "X..X..Xx       ",
    "X.X X...X      ",
    "XX   X...X     ",
    "X     X...X    ",
    "       X...X   ",
    "         XX    ",
    "               "
};

static int mouse_x = 400;
static int mouse_y = 300;
static uint32_t mouse_backbuffer[cursor_w * cursor_h];

void mouse_save_backbuffer(int x, int y) {
    for (int cy = 0; cy < cursor_h; cy++) {
        for (int cx = 0; cx < cursor_w; cx++) {
            mouse_backbuffer[cy * cursor_w + cx] = get_pixel(x + cx, y + cy);
        }
    }
}

void mouse_restore_backbuffer(int x, int y) {
    for (int cy = 0; cy < cursor_h; cy++) {
        for (int cx = 0; cx < cursor_w; cx++) {
            put_pixel(x + cx, y + cy, mouse_backbuffer[cy * cursor_w + cx]);
        }
    }
}

void mouse_draw_sprite(int x, int y) {
    for (int cy = 0; cy < cursor_h; cy++) {
        for (int cx = 0; cx < cursor_w; cx++) {
            char pixel = cursor_sprite[cy][cx];
            if (pixel == 'X') {
                put_pixel(x + cx, y + cy, color_black);
            } else if (pixel == '.') {
                put_pixel(x + cx, y + cy, color_white);
            }
        }
    }
}

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 10000;
    if (type == 0) {
        while (timeout--) {
            io_wait();
            if (inb(0x64) & 1) return;
        }
    } else {
        while (timeout--) {
            io_wait();
            if (!(inb(0x64) & 2)) return;
        }
    }
}

// mouse input
static void mouse_write(uint8_t val) {
    mouse_wait(1);
    outb(0x64, 0xd4);
    mouse_wait(1);
    outb(0x60, val);
}

// mouse read (input to output)
static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

// mouse init
void ps2_mouse_init(void) {
    uint8_t status;

    mouse_wait(1);
    outb(0x64, 0xa8); 

    mouse_wait(1);
    outb(0x64, 0x20);
    status = mouse_read();

    status |= 0x02;   
    status &= ~0x20;  

    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    mouse_write(0xf6); 
    mouse_read();

    mouse_write(0xf4); 
    mouse_read();

    while (inb(0x64) & 1) { inb(0x60); io_wait(); } 

    mouse_save_backbuffer(mouse_x, mouse_y);
    mouse_draw_sprite(mouse_x, mouse_y);
}

static bool mouse_read_byte(uint8_t* out_byte) {
    uint8_t status = inb(0x64);
    if ((status & 0x01) && (status & 0x20)) {
        *out_byte = inb(0x60);
        return true;
    }
    return false;
}

// mouse events
void handle_mouse_events(int x, int y, bool left_down) {
    bool click_event = left_down && !prev_left_click;
    bool needs_redraw = false;

    // desktop terminal icon click
    if (click_event && x >= 20 && x <= 80 && y >= 30 && y <= 100) {
        window_open = true;
        needs_redraw = true;
    }

    // desktop calculator icon click
    else if (click_event && x >= 20 && x <= 80 && y >= 110 && y <= 180) {
        calc_open = true;
        needs_redraw = true;
    }

    // toggle start menu
    else if (click_event && x >= 10 && x <= 90 && y >= (int)screen_height - 32 && y <= (int)screen_height - 8) {
        start_menu_open = !start_menu_open;
        needs_redraw = true;
    }

    // start menu items
    else if (start_menu_open && click_event) {
        uint32_t menu_x = 10;
        uint32_t menu_y = screen_height - 240;
        
        if (x >= (int)menu_x && x <= (int)(menu_x + 160)) {
            if (y >= (int)(menu_y + 10) && y <= (int)(menu_y + 40)) {
                window_open = true;
                start_menu_open = false;
                needs_redraw = true;
            } else if (y >= (int)(menu_y + 40) && y <= (int)(menu_y + 70)) {
                calc_open = true;
                start_menu_open = false;
                needs_redraw = true;
            } else if (y >= (int)(menu_y + 70) && y <= (int)(menu_y + 110)) {
                term_clear();
                start_menu_open = false;
                needs_redraw = true;
            } else if (y >= (int)(menu_y + 110) && y <= (int)(menu_y + 150)) {
                execute_command("about");
                window_open = true;
                start_menu_open = false;
                needs_redraw = true;
            } else if (y >= (int)(menu_y + 150) && y <= (int)(menu_y + 190)) {
                outb(0x64, 0xfe); 
            } else {
                start_menu_open = false;
                needs_redraw = true;
            }
        } else {
            start_menu_open = false;
            needs_redraw = true;
        }
    }

    // close terminal window button
    else if (window_open && click_event && (x >= win_x + win_w - 24 && x <= win_x + win_w - 4 && y >= win_y + 2 && y <= win_y + 22)) {
        window_open = false;
        needs_redraw = true;
    }

    // close calculator window button
    else if (calc_open && click_event && (x >= calc_x + calc_w - 24 && x <= calc_x + calc_w - 4 && y >= calc_y + 2 && y <= calc_y + 22)) {
        calc_open = false;
        needs_redraw = true;
    }

    // calculator grid clicks
    else if (calc_open && click_event && (x >= calc_x + 12 && x <= calc_x + calc_w - 12 && y >= calc_y + 80 && y <= calc_y + calc_h - 10)) {
        uint32_t start_grid_x = calc_x + 12;
        uint32_t start_grid_y = calc_y + 80;

        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                uint32_t bx = start_grid_x + c * 46;
                uint32_t by = start_grid_y + r * 42;
                if (x >= (int)bx && x <= (int)(bx + 40) && y >= (int)by && y <= (int)(by + 36)) {
                    calc_press_btn(calc_grid[r][c]);
                    needs_redraw = true;
                }
            }
        }
    }

    // drag initiation checks
    if (click_event && current_drag == DRAG_NONE) {
        if (calc_open && x >= calc_x && x <= calc_x + calc_w - 24 && y >= calc_y && y <= calc_y + 24) {
            current_drag = DRAG_CALC;
            drag_offset_x = x - calc_x;
            drag_offset_y = y - calc_y;
        } else if (window_open && x >= win_x && x <= win_x + win_w - 24 && y >= win_y && y <= win_y + 24) {
            current_drag = DRAG_TERM;
            drag_offset_x = x - win_x;
            drag_offset_y = y - win_y;
        }
    }

    // drag handling
    if (left_down && current_drag != DRAG_NONE) {
        if (current_drag == DRAG_TERM) {
            int new_win_x = x - drag_offset_x;
            int new_win_y = y - drag_offset_y;
            if (new_win_x != win_x || new_win_y != win_y) {
                win_x = new_win_x;
                win_y = new_win_y;
                needs_redraw = true;
            }
        } else if (current_drag == DRAG_CALC) {
            int new_calc_x = x - drag_offset_x;
            int new_calc_y = y - drag_offset_y;
            if (new_calc_x != calc_x || new_calc_y != calc_y) {
                calc_x = new_calc_x;
                calc_y = new_calc_y;
                needs_redraw = true;
            }
        }
    }

    if (!left_down) {
        current_drag = DRAG_NONE;
    }

    if (needs_redraw) {
        ui_render_desktop();
    }

    prev_left_click = left_down;
}

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

void mouse_poll_update(void) {
    uint8_t b;

    while (mouse_read_byte(&b)) {
        if (mouse_cycle == 0) {
            if (b & 0x08) {
                mouse_packet[0] = b;
                mouse_cycle = 1;
            }
        } else if (mouse_cycle == 1) {
            mouse_packet[1] = b;
            mouse_cycle = 2;
        } else if (mouse_cycle == 2) {
            mouse_packet[2] = b;
            mouse_cycle = 0;

            uint8_t b1 = mouse_packet[0];
            uint8_t b2 = mouse_packet[1];
            uint8_t b3 = mouse_packet[2];

            int rel_x = (int8_t)b2;
            int rel_y = (int8_t)b3;
            bool left_click = (b1 & 0x01);

            mouse_restore_backbuffer(mouse_x, mouse_y);

            mouse_x += rel_x;
            mouse_y -= rel_y; 

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= (int)screen_width - cursor_w)  mouse_x = screen_width - cursor_w;
            if (mouse_y >= (int)screen_height - cursor_h) mouse_y = screen_height - cursor_h;

            handle_mouse_events(mouse_x, mouse_y, left_click);

            mouse_save_backbuffer(mouse_x, mouse_y);
            mouse_draw_sprite(mouse_x, mouse_y);
        }
    }
}

void keyboard_poll_update(void) {
    uint8_t status = inb(0x64);
    
    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t scancode = inb(0x60);

        if (!(scancode & 0x80)) {
            char key = scancode_ascii[scancode];
            bool char_added = false;

            if (key == '\b') {
                if (term_input_len > 0) {
                    term_input_len--;
                    term_input[term_input_len] = '\0';
                    char_added = true;
                }
            } else if (key == '\n') {
                execute_command(term_input);
                term_input_len = 0;
                term_input[0] = '\0';
                char_added = true;
            } else if (key >= ' ' && key <= '~') {
                if (term_input_len < kb_buf_max - 1) {
                    term_input[term_input_len] = key;
                    term_input_len++;
                    term_input[term_input_len] = '\0';
                    char_added = true;
                }
            }

            if (window_open && char_added) {
                mouse_restore_backbuffer(mouse_x, mouse_y);
                ui_render_desktop();
                mouse_save_backbuffer(mouse_x, mouse_y);
                mouse_draw_sprite(mouse_x, mouse_y);
            }
        }
    }
}

void kernel_main(uint32_t magic, multiboot_info_t* mb_info) {
    if (magic != 0x2badb002 || !(mb_info->flags & (1 << 12))) {
        return;
    }

    framebuffer = (uint32_t*)(uintptr_t)mb_info->framebuffer_addr;
    screen_width = mb_info->framebuffer_width;
    screen_height = mb_info->framebuffer_height;
    screen_pitch = mb_info->framebuffer_pitch;

    // terminal start
    term_clear();
    term_print("Welcome to the terminal");
    term_print("Type 'help' for available commands.");

    ui_render_desktop();
    ps2_mouse_init();

    while (1) {
        mouse_poll_update();
        keyboard_poll_update();
    }
}

//thats it, to people that says its vibecoded, IT IS NOT! if you dont see grammar errors is because im trying to keep it clean and undertsandable, but if you see some, please let me notice, im italian so its pretty hard for me, thank you all for the support see you soon! <3
