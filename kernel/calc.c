static const char* calc_grid[4][4] = {
    {"7", "8", "9", "/"},
    {"4", "5", "6", "*"},
    {"1", "2", "3", "-"},
    {"C", "0", "=", "+"}
};

static int32_t parse_int(const char* str) {
    int32_t res = 0;
    int32_t sign = 1;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res * sign;
}

static void int_to_str(int32_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    uint32_t i = 0;
    bool is_neg = false;
    if (val < 0) {
        is_neg = true;
        val = -val;
    }
    char temp[16];
    while (val > 0) {
        temp[i++] = '0' + (val % 10);
        val /= 10;
    }
    uint32_t j = 0;
    if (is_neg) buf[j++] = '-';
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

void calc_press_btn(const char* label) {
    char c = label[0];
    if (c >= '0' && c <= '9') {
        if (calc_clear_on_next || (calc_disp_len == 1 && calc_disp[0] == '0')) {
            calc_disp[0] = c;
            calc_disp[1] = '\0';
            calc_disp_len = 1;
            calc_clear_on_next = false;
        } else if (calc_disp_len < 14) {
            calc_disp[calc_disp_len++] = c;
            calc_disp[calc_disp_len] = '\0';
        }
    } else if (c == 'C') {
        calc_disp[0] = '0';
        calc_disp[1] = '\0';
        calc_disp_len = 1;
        calc_operand = 0;
        calc_op = 0;
        calc_clear_on_next = false;
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        calc_operand = parse_int(calc_disp);
        calc_op = c;
        calc_clear_on_next = true;
    } else if (c == '=') {
        if (calc_op != 0) {
            int32_t second = parse_int(calc_disp);
            int32_t res = 0;
            if (calc_op == '+') res = calc_operand + second;
            else if (calc_op == '-') res = calc_operand - second;
            else if (calc_op == '*') res = calc_operand * second;
            else if (calc_op == '/') res = (second != 0) ? (calc_operand / second) : 0;

            int_to_str(res, calc_disp);
            calc_disp_len = 0;
            while (calc_disp[calc_disp_len]) calc_disp_len++;
            calc_op = 0;
            calc_clear_on_next = true;
        }
    }
}

void draw_calculator_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!calc_open) return;

    // window base
    draw_rect(x, y, w, h, color_menu_bg);
    draw_rect(x, y, w, 24, color_window_hdr);

    // window outline
    for (uint32_t i = 0; i < w; i++) {
        put_pixel(x + i, y, color_dark_gray);
        put_pixel(x + i, y + h - 1, color_dark_gray);
    }
    for (uint32_t j = 0; j < h; j++) {
        put_pixel(x, y + j, color_dark_gray);
        put_pixel(x + w - 1, y + j, color_dark_gray);
    }

    draw_string("Calculator", x + 8, y + 4, color_white);
    draw_rect(x + w - 24, y + 2, 20, 20, color_close_btn);
    draw_string("X", x + w - 18, y + 4, color_white);

    // display screen
    draw_rect(x + 12, y + 36, w - 24, 32, color_black);
    for (uint32_t i = 0; i < w - 24; i++) {
        put_pixel(x + 12 + i, y + 36, color_dark_gray);
        put_pixel(x + 12 + i, y + 67, color_dark_gray);
    }

    uint32_t text_x = (x + w - 24) - (calc_disp_len * 8);
    if (text_x < x + 16) text_x = x + 16;
    draw_string(calc_disp, text_x, y + 44, color_green);

    // calculator buttons
    uint32_t btn_w = 40;
    uint32_t btn_h = 36;
    uint32_t start_grid_x = x + 12;
    uint32_t start_grid_y = y + 80;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            uint32_t bx = start_grid_x + c * 46;
            uint32_t by = start_grid_y + r * 42;

            uint32_t btn_color = color_dark_gray;
            if (c == 3) btn_color = color_orange;
            if (r == 3 && c == 0) btn_color = color_close_btn;

            draw_rect(bx, by, btn_w, btn_h, btn_color);
            draw_string(calc_grid[r][c], bx + 16, by + 10, color_white);
        }
    }
}
