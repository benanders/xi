
#include "editor.h"

#define WORD_SEPARATORS "./\\()\"'-:,.;<>~!@#$%^&*|+=[]{}`~?"

static Line * line_empty() {
    Line *empty = malloc(sizeof(Line) + sizeof(char) * 16);
    empty->len = 0;
    empty->max = 16;
    return empty;
}

static int next_pow2(int num) {
    num--;
    int pow = 2;
    while (num >>= 1) {
        pow <<= 1;
    }
    return pow;
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

static void line_increase_capacity(Editor *e, int line_idx, int more) {
    Line *line = e->lines[line_idx];
    if (line->len + more > line->max) {
        while (line->len + more > line->max) {
            line->max *= 2;
        }
        line = realloc(line, sizeof(Line) + sizeof(char) * line->max);
        e->lines[line_idx] = line;
    }
}

Editor editor_new() {
    Editor editor;
    editor.path = NULL;
    editor.scroll_x = 0;
    editor.scroll_y = 0;
    editor.cursor_x = 0;
    editor.cursor_y = 0;
    editor.num_lines = 0;
    editor.max_lines = 16;
    editor.lines = malloc(sizeof(Line) * editor.max_lines);
    editor.lines[editor.num_lines++] = line_empty();
    editor.run = 1;
    return editor;
}

static void editor_increase_lines_capacity(Editor *e, int more) {
    if (e->num_lines + more > e->max_lines) {
        while (e->num_lines + more > e->max_lines) {
            e->max_lines *= 2;
        }
        e->lines = realloc(e->lines, sizeof(Line **) * e->max_lines);
    }
}

Editor editor_open(char *path) {
    Editor e = editor_new();
    e.path = path;

    FILE *file = fopen(path, "r");
    if (!file) { // File hasn't been created yet
        return e;
    }

    char line_buf[256];
    while (fgets(line_buf, sizeof(line_buf), file)) {
        int len = (int) strlen(line_buf);
        line_increase_capacity(&e, e.num_lines - 1, len);
        Line *line = e.lines[e.num_lines - 1];
        if (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r') { // EOL
            strncpy(&line->s[line->len], line_buf, len - 1); // Skip newline
            line->len += len - 1;
            editor_increase_lines_capacity(&e, 1); // Next line
            e.lines[e.num_lines++] = line_empty();
        } else { // More line to come
            strncpy(&line->s[line->len], line_buf, len);
            line->len += len;
        }
    }
    fclose(file);
    e.num_lines--; // Delete the last new line added
    return e;
}


// ---- Drawing ---------------------------------------------------------------

static void editor_set_cursor(Editor *e) {
    int rel_x = e->cursor_x - e->scroll_x;
    int rel_y = e->cursor_y - e->scroll_y;
    tb_set_cursor(rel_x, rel_y);
}

static void editor_draw_line(Editor *e, int y) {
    int line_idx = y + e->scroll_y;
    if (line_idx >= e->num_lines) {
        return;
    }
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

void editor_draw(Editor *e) {
    tb_clear();
    int height = tb_height();
    for (int y = 0; y < height; y++) {
        editor_draw_line(e, y);
    }
    editor_set_cursor(e);
    tb_present();
}


// ---- Events ----------------------------------------------------------------

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
        e->scroll_y = e->cursor_y - height + 1;
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

static void editor_cursor_start_of_file(Editor *e) {
    e->cursor_x = 0;
    e->cursor_y = 0;
    editor_correct_scroll(e);
}

static void editor_cursor_end_of_file(Editor *e) {
    e->cursor_y = e->num_lines - 1;
    Line *line = e->lines[e->cursor_y];
    e->cursor_x = line->len;
    editor_correct_scroll(e);
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
        editor_correct_horizontal_scroll(e);
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
        editor_correct_horizontal_scroll(e);
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

static int is_word_sep(char ch) {
    for (char *sep = WORD_SEPARATORS; *sep != '\0'; sep++) {
        if (ch == *sep) {
            return 1;
        }
    }
    return 0;
}

static int editor_find_prev_word_on_line(Editor *e) {
    // 1. Go back one character
    // 2. Skip all whitespace
    // 3. If the character is not a word separator, keep going back until we
    //    find a word separator or whitespace, then add one
    // 4. If the character is a word separator, keep going back until we find
    //    a not word separator or whitespace, then add one
    if (e->cursor_x == 0) {
        return 0;
    }
    Line *line = e->lines[e->cursor_y];
    int x = e->cursor_x - 1; // 1
    while (isspace(line->s[x]) && x >= 0) { // 2
        x--;
    }
    int sep = is_word_sep(line->s[x]);
    while (is_word_sep(line->s[x]) == sep &&
            !isspace(line->s[x]) &&
            x >= 0) {
        x--; // 3 + 4
    }
    x++;
    return x;
}

static void editor_cursor_prev_word(Editor *e) {
    if (e->cursor_x > 0) {
        e->cursor_x = editor_find_prev_word_on_line(e);
        editor_correct_horizontal_scroll(e);
    } else {
        if (e->cursor_y == 0) {
            return; // Start of file
        }
        e->cursor_y--; // Previous word on the line above
        Line *line = e->lines[e->cursor_y];
        e->cursor_x = line->len;
        e->cursor_x = editor_find_prev_word_on_line(e);
        editor_correct_scroll(e);
    }
}

static int editor_find_next_word_on_line(Editor *e) {
    // 1. Skip all whitespace
    // 2. If the character is not a word separator, keep going forward until we
    //    find a word separator or whitespace
    // 3. If the character is a word separator, keep going forward until we find
    //    a not word separator or whitespace
    Line *line = e->lines[e->cursor_y];
    if (e->cursor_x >= line->len) {
        return line->len;
    }
    int x = e->cursor_x; // 1
    while (isspace(line->s[x]) && x < line->len) { // 2
        x++;
    }
    int sep = is_word_sep(line->s[x]);
    while (is_word_sep(line->s[x]) == sep &&
            !isspace(line->s[x]) &&
            x < line->len) {
        x++; // 3 + 4
    }
    return x;
}

static void editor_cursor_next_word(Editor *e) {
    Line *line = e->lines[e->cursor_y];
    if (e->cursor_x < line->len) {
        e->cursor_x = editor_find_next_word_on_line(e);
        editor_correct_horizontal_scroll(e);
    } else {
        if (e->cursor_y >= e->num_lines - 1) {
            return; // End of file
        }
        e->cursor_y++; // Next word on the line below
        e->cursor_x = 0;
        e->cursor_x = editor_find_next_word_on_line(e);
        editor_correct_scroll(e);
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
    editor_increase_lines_capacity(e, 1);
    int remaining = e->num_lines - after_idx - 1;
    if (remaining > 0) {
        Line **src = &e->lines[after_idx + 1];
        memcpy(src + 1, src, sizeof(Line *) * remaining);
    }
    e->lines[after_idx + 1] = to_insert;
    e->num_lines++;
}

static void editor_insert_character(Editor *e, char ch) {
    line_increase_capacity(e, e->cursor_y, 1);
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
        line_increase_capacity(e, e->cursor_y - 1, line->len);
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

static void editor_shift_line_up(Editor *e) {
    if (e->cursor_y == 0) {
        return; // First line
    }
    Line *swap = e->lines[e->cursor_y - 1];
    e->lines[e->cursor_y - 1] = e->lines[e->cursor_y];
    e->lines[e->cursor_y] = swap;
    e->cursor_y--;
}

static void editor_shift_line_down(Editor *e) {
    if (e->cursor_y >= e->num_lines - 1) {
        return; // Last line
    }
    Line *swap = e->lines[e->cursor_y + 1];
    e->lines[e->cursor_y + 1] = e->lines[e->cursor_y];
    e->lines[e->cursor_y] = swap;
    e->cursor_y++;
}

static void editor_key(Editor *e, struct tb_event ev) {
    if (ev.mod == TB_MOD_ALT) {
        switch (ev.key) {
            // Movement
            case TB_KEY_ARROW_LEFT:  editor_cursor_prev_word(e); return;
            case TB_KEY_ARROW_RIGHT: editor_cursor_next_word(e); return;

            // Editing
            case TB_KEY_ARROW_UP:    editor_shift_line_up(e); return;
            case TB_KEY_ARROW_DOWN:  editor_shift_line_down(e); return;
        }
    } else if (ev.mod == TB_MOD_CTRL) {
        switch (ev.key) {
            // Movement
            case TB_KEY_ARROW_LEFT:  editor_cursor_start_of_line(e); return;
            case TB_KEY_ARROW_RIGHT: editor_cursor_end_of_line(e); return;
            case TB_KEY_ARROW_UP:    editor_cursor_start_of_file(e); return;
            case TB_KEY_ARROW_DOWN:  editor_cursor_end_of_file(e); return;
        }
    } // Otherwise, fall through to the non-alt command...
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

        // Quit
        case TB_KEY_CTRL_Q:      e->run = 0; break;
    }
}

static void editor_ch(Editor *e, struct tb_event ev) {
    if (ev.ch < 256) { // ASCII support only for now
        editor_insert_character(e, (char) ev.ch);
    }
}

void editor_update(Editor *e, struct tb_event ev) {
    if (ev.type == TB_EVENT_KEY && ev.ch == 0) {
        editor_key(e, ev);
    } else if (ev.type == TB_EVENT_KEY && ev.key == 0) {
        editor_ch(e, ev);
    }
}