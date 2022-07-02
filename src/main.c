
#define TB_IMPL
#include <termbox.h>
#include <math.h>

typedef struct {
    int len, max;
    char s[];
} Line;

typedef struct {
    char *path; // File we're editing, or NULL if it hasn't been saved yet
    int scroll_x, scroll_y;
    int cursor_x, cursor_y;
    Line **lines;
    int num_lines, max_lines;
} Editor;

static int next_pow2(int num) {
    num--;
    int pow = 2;
    while (num >>= 1) {
        pow <<= 1;
    }
    return pow;
}

static Line * line_empty() {
    Line *empty = malloc(sizeof(Line) + sizeof(char) * 16);
    empty->len = 0;
    empty->max = 16;
    return empty;
}

static Line * line_from(char *str, int len) {
    int capacity = next_pow2(len);
    if (capacity < 16) {
        capacity = 16; // Minimum
    }
    Line *line = malloc(sizeof(Line) + sizeof(char) * capacity);
    line->len = len;
    line->max = capacity;
    memcpy(line->s, str, sizeof(char) * len);
    return line;
}

static Editor editor_new() {
    Editor editor;
    editor.path = NULL;
    editor.scroll_x = 0;
    editor.scroll_y = 0;
    editor.cursor_x = 0;
    editor.cursor_y = 0;
    editor.num_lines = 1;
    editor.max_lines = 16;
    editor.lines = malloc(sizeof(Line) * editor.max_lines);
    editor.lines[0] = line_empty();
    return editor;
}

static void editor_set_cursor(Editor *e) {
    int rel_x = e->cursor_x - e->scroll_x;
    int rel_y = e->cursor_y - e->scroll_y;
    tb_set_cursor(rel_x, rel_y);
}

static void editor_draw_line(Editor *e, int line_idx, int y) {
    Line *line = e->lines[line_idx];
    if (line->len == 0) {
        return;
    }
    int width = tb_width();
    for (int x = 0; x < width; x++) {
        int ch_idx = x + e->scroll_x;
        if (ch_idx >= line->len) {
            break;
        }
        char c = line->s[ch_idx];
        tb_set_cell(x, y, c, TB_DEFAULT, TB_DEFAULT);
    }
}

static void editor_draw(Editor *e) {
    tb_clear();
    int height = tb_height();
    for (int y = 0; y < height; y++) {
        int line_idx = y + e->scroll_y;
        if (line_idx >= e->num_lines) {
            break; // No more lines to draw
        }
        editor_draw_line(e, line_idx, y);
    }
    editor_set_cursor(e);
    tb_present();
}

static void editor_correct_horizontal_scroll(Editor *e) {
    int width = tb_width();
    if (e->cursor_x >= width + e->scroll_x) {
        e->scroll_x = e->cursor_x - width + 1;
    } else if (e->cursor_x < e->scroll_x) {
        e->scroll_x = e->cursor_x;
    }
}

static void editor_correct_vertical_scroll(Editor *e) {
    int height = tb_height();
    if (e->cursor_y >= height + e->scroll_y) {
        e->scroll_y = e->scroll_y - height + 1;
    } else if (e->cursor_y < e->scroll_y) {
        e->scroll_y = e->cursor_y;
    }
}

static void editor_correct_scroll(Editor *e) {
    editor_correct_horizontal_scroll(e);
    editor_correct_vertical_scroll(e);
}

static void editor_cursor_start_of_line(Editor *e) {
    e->cursor_x = 0;
    editor_correct_horizontal_scroll(e);
}

static void editor_cursor_end_of_line(Editor *e) {
    Line *line = e->lines[e->cursor_y];
    e->cursor_x = line->len;
    editor_correct_horizontal_scroll(e);
}

static void editor_cursor_left(Editor *e) {
    if (e->cursor_x == 0) {
        if (e->cursor_y == 0) {
            return; // Start of source file
        }
        e->cursor_y--;
        editor_cursor_end_of_line(e);
    } else {
        e->cursor_x--;
    }
}

static void editor_cursor_right(Editor *e) {
    Line *line = e->lines[e->cursor_y];
    if (e->cursor_x >= line->len) {
        if (e->cursor_y >= e->num_lines - 1) {
            return; // End of source file
        }
        e->cursor_y++;
        editor_cursor_start_of_line(e);
    } else {
        e->cursor_x++;
    }
}

static void editor_correct_cursor_if_beyond_eol(Editor *e) {
    Line *line = e->lines[e->cursor_y];
    if (e->cursor_x > line->len) {
        e->cursor_x = line->len;
        editor_correct_horizontal_scroll(e);
    }
}

static void editor_cursor_up(Editor *e) {
    if (e->cursor_y == 0) {
        return; // First line in file
    }
    e->cursor_y--;
    editor_correct_vertical_scroll(e);
    editor_correct_cursor_if_beyond_eol(e);
}

static void editor_cursor_down(Editor *e) {
    if (e->cursor_y >= e->num_lines - 1) {
        return; // Last line in file
    }
    e->cursor_y++;
    editor_correct_vertical_scroll(e);
    editor_correct_cursor_if_beyond_eol(e);
}

static void editor_increase_line_capacity(Editor *e, int line_idx, int more) {
    Line *line = e->lines[line_idx];
    if (line->len + more > line->max) {
        while (line->len + more > line->max) {
            line->max *= 2;
        }
        line = realloc(line, sizeof(Line) + sizeof(char) * line->max);
        e->lines[line_idx] = line;
    }
}

static void editor_delete_line(Editor *e, int line_idx) {
    free(e->lines[line_idx]);
    int remaining = e->num_lines - line_idx - 1;
    if (remaining > 0) {
        Line **dst = &e->lines[line_idx];
        memcpy(dst, dst + 1, sizeof(Line *) * remaining);
    }
    e->num_lines--;
}

static void editor_insert_line(Editor *e, int after_idx, Line *to_insert) {
    if (e->num_lines >= e->max_lines) {
        e->max_lines *= 2;
        e->lines = realloc(e->lines, sizeof(Line **) * e->max_lines);
    }
    int remaining = e->num_lines - after_idx - 1;
    if (remaining > 0) {
        Line **src = &e->lines[after_idx + 1];
        memcpy(src + 1, src, sizeof(Line *) * remaining);
    }
    e->lines[after_idx + 1] = to_insert;
    e->num_lines++;
}

static void editor_insert_character(Editor *e, char ch) {
    editor_increase_line_capacity(e, e->cursor_y, 1);
    Line *line = e->lines[e->cursor_y];
    int remaining = line->len - e->cursor_x;
    if (remaining > 0) {
        char *src = &line->s[e->cursor_x];
        memcpy(src + 1, src, sizeof(char) * remaining);
    }
    line->s[e->cursor_x] = ch;
    line->len++;
    e->cursor_x++;
    editor_correct_horizontal_scroll(e);
}

static void editor_new_line(Editor *e) {
    Line *line = e->lines[e->cursor_y];
    int remaining = line->len - e->cursor_x;
    Line *to_insert;
    if (remaining > 0) {
        to_insert = line_from(&line->s[e->cursor_x], remaining);
    } else {
        to_insert = line_empty();
    }
    line->len = e->cursor_x;
    editor_insert_line(e, e->cursor_y, to_insert);
    e->cursor_y++;
    e->cursor_x = 0;
    editor_correct_scroll(e);
}

static void editor_backspace(Editor *e) {
    Line *line = e->lines[e->cursor_y];
    if (e->cursor_x == 0) { // Start of line
        if (e->cursor_y == 0) {
            return; // Start of file
        }
        editor_increase_line_capacity(e, e->cursor_y - 1, line->len);
        Line *prev = e->lines[e->cursor_y - 1];
        e->cursor_x = prev->len;
        memcpy(&prev->s[prev->len], line->s, sizeof(char) * line->len);
        prev->len += line->len;
        editor_delete_line(e, e->cursor_y);
        e->cursor_y--;
        editor_correct_scroll(e);
    } else { // Middle of line
        int remaining = line->len - e->cursor_x;
        if (remaining > 0) {
            char *dst = &line->s[e->cursor_x];
            memcpy(dst - 1, dst, sizeof(char) * remaining);
        }
        line->len--;
        e->cursor_x--;
        editor_correct_horizontal_scroll(e);
    }
}

static void editor_key(Editor *e, struct tb_event ev) {
    switch (ev.key) {
        // Movement
        case TB_KEY_ARROW_LEFT:  editor_cursor_left(e); break;
        case TB_KEY_ARROW_RIGHT: editor_cursor_right(e); break;
        case TB_KEY_ARROW_UP:    editor_cursor_up(e); break;
        case TB_KEY_ARROW_DOWN:  editor_cursor_down(e); break;

        // Editing
        case TB_KEY_ENTER:       editor_new_line(e); break;
        case TB_KEY_BACKSPACE:
        case TB_KEY_BACKSPACE2:  editor_backspace(e); break;
    }
}

static void editor_ch(Editor *e, struct tb_event ev) {
    if (ev.ch < 256) { // ASCII support only for now
        editor_insert_character(e, (char) ev.ch);
    }
}

static void editor_update(Editor *e, struct tb_event ev) {
    if (ev.type == TB_EVENT_KEY && ev.ch == 0) {
        editor_key(e, ev);
    } else if (ev.type == TB_EVENT_KEY && ev.key == 0) {
        editor_ch(e, ev);
    }
}

int main(int argc, char *argv[]) {
    tb_init();
    Editor editor = editor_new();
    editor_draw(&editor);
    while (1) {
        struct tb_event ev;
        tb_poll_event(&ev);
        if (ev.type == TB_EVENT_KEY && ev.key == TB_KEY_CTRL_Q) {
            break;
        }
        editor_update(&editor, ev);
        editor_draw(&editor);
    }
    tb_shutdown();
}
