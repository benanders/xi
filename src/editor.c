
#include "editor.h"

#define WORD_SEPARATORS "./\\()\"'-:,.;<>~!@#$%^&*|+=[]{}`~?"

static Line * empty_line() {
    Line *empty = malloc(sizeof(Line) + sizeof(char) * 16);
    empty->len = 0;
    empty->max = 16;
    return empty;
}

static Theme default_theme() {
    Theme t;
    t.text_fg = TB_DEFAULT;
    t.text_bg = TB_DEFAULT;
    t.selection_fg = TB_BLACK;
    t.selection_bg = TB_WHITE;
    t.highlight_line = 1;
    t.highlight_fg = TB_BLACK;
    t.highlight_bg = TB_WHITE;
    t.show_gutter = 1;
    t.gutter_fg = TB_YELLOW;
    t.gutter_bg = TB_DEFAULT;
    t.show_info_bar = 1;
    t.info_bar_fg = TB_BLUE;
    t.info_bar_bg = TB_DEFAULT;
    return t;
}

Editor editor_new() {
    Editor e;
    e.run = 1;
    e.path = NULL;
    e.scroll_x = 0;
    e.scroll_y = 0;
    e.cursor_x = 0;
    e.cursor_y = 0;
    e.prev_cursor_x = -1;
    e.select_x = -1;
    e.select_y = -1;
    e.num_lines = 0;
    e.max_lines = 16;
    e.lines = malloc(sizeof(Line) * e.max_lines);
    e.lines[e.num_lines++] = empty_line();
    e.theme = default_theme();
    return e;
}

static void increase_line_capacity(Editor *e, int line_idx, int more) {
    Line *line = e->lines[line_idx];
    if (line->len + more > line->max) {
        while (line->len + more > line->max) {
            line->max *= 2;
        }
        line = realloc(line, sizeof(Line) + sizeof(char) * line->max);
        e->lines[line_idx] = line;
    }
}

static void increase_lines_capacity(Editor *e, int more) {
    if (e->num_lines + more > e->max_lines) {
        while (e->num_lines + more > e->max_lines) {
            e->max_lines *= 2;
        }
        e->lines = realloc(e->lines, sizeof(Line **) * e->max_lines);
    }
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
        increase_line_capacity(&e, e.num_lines - 1, len);
        Line *line = e.lines[e.num_lines - 1];
        if (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r') { // EOL
            strncpy(&line->s[line->len], line_buf, len - 1); // Skip newline
            line->len += len - 1;
            increase_lines_capacity(&e, 1); // Next line
            e.lines[e.num_lines++] = empty_line();
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

static int has_selection(Editor *e) {
    return e->select_x != -1 && e->select_y != -1;
}

static void selection_range(Editor *e,
                            int *min_x, int *min_y,
                            int *max_x, int *max_y) {
    if (!has_selection(e)) {
        if (min_x) { *min_x = -1; }
        if (min_y) { *min_y = -1; }
        if (max_x) { *max_x = -1; }
        if (max_y) { *max_y = -1; }
        return;
    }
    int y1 = e->select_y < e->cursor_y ? e->select_y : e->cursor_y;
    int y2 = e->select_y > e->cursor_y ? e->select_y : e->cursor_y;
    int x1, x2;
    if (y1 == y2) { // Selection all on one line
        x1 = e->select_x < e->cursor_x ? e->select_x : e->cursor_x;
        x2 = e->select_x > e->cursor_x ? e->select_x : e->cursor_x;
    } else { // Selection on multiple lines
        x1 = y1 == e->select_y ? e->select_x : e->cursor_x;
        x2 = y2 == e->select_y ? e->select_x : e->cursor_x;
    }
    if (min_x) { *min_x = x1; }
    if (min_y) { *min_y = y1; }
    if (max_x) { *max_x = x2; }
    if (max_y) { *max_y = y2; }
}

static int is_in_selection(Editor *e, int ch_idx, int line_idx) {
    if (!has_selection(e)) { // No selection
        return 0;
    }
    int min_x, min_y, max_x, max_y;
    selection_range(e, &min_x, &min_y, &max_x, &max_y);
    if (line_idx < min_y || line_idx > max_y) { // Not in the selection
        return 0;
    } else if (min_y == max_y) { // Selection all on one line
        return line_idx == min_y && ch_idx >= min_x && ch_idx < max_x;
    } else if (line_idx == min_y) { // On first line of selection
        return ch_idx >= min_x;
    } else if (line_idx == max_y) { // On last line of selection
        return ch_idx < max_x;
    } else { // In the middle of the selection
        return 1;
    }
}

static void draw_cursor(Editor *e) {
    if (has_selection(e)) {
        tb_hide_cursor(); // Don't draw the cursor in selection mode
        return;
    }
    int rel_x = e->cursor_x - e->scroll_x;
    int rel_y = e->cursor_y - e->scroll_y;
    tb_set_cursor(rel_x, rel_y);
}

static void draw_line(Editor *e, int y) {
    int line_idx = y + e->scroll_y;
    Line *line = e->lines[line_idx];
    if (line->len == 0) {
        return;
    }
    int width = tb_width();
    for (int x = 0; x < width; x++) {
        int ch_idx = x + e->scroll_x;
        if (ch_idx > line->len) {
            break; // Don't draw beyond the line
        }
        char ch;
        if (ch_idx < line->len) {
            ch = line->s[ch_idx];
        } else {
            ch = ' ';
        }
        uintattr_t fg, bg;
        if (is_in_selection(e, ch_idx, line_idx)) {
            fg = e->theme.selection_fg;
            bg = e->theme.selection_bg;
        } else {
            fg = e->theme.text_fg;
            bg = e->theme.text_bg;
        }
        tb_set_cell(x, y, ch, fg, bg);
    }
}

void editor_draw(Editor *e) {
    tb_clear();
    int height = tb_height();
    for (int y = 0; y < height; y++) {
        if (y + e->scroll_y >= e->num_lines) {
            break; // Last line
        }
        draw_line(e, y);
    }
    draw_cursor(e);
    tb_present();
}


// ---- Movement and Selection ------------------------------------------------

static void set_cursor_x(Editor *e, int x) {
    e->cursor_x = x;
    e->prev_cursor_x = -1;
}

static void correct_horizontal_scroll(Editor *e) {
    int width = tb_width();
    if (e->cursor_x >= width + e->scroll_x) {
        e->scroll_x = e->cursor_x - width + 1;
    } else if (e->cursor_x < e->scroll_x) {
        e->scroll_x = e->cursor_x;
    }
}

static void correct_vertical_scroll(Editor *e) {
    int height = tb_height();
    if (e->cursor_y >= height + e->scroll_y) {
        e->scroll_y = e->cursor_y - height + 1;
    } else if (e->cursor_y < e->scroll_y) {
        e->scroll_y = e->cursor_y;
    }
}

static void correct_scroll(Editor *e) {
    correct_horizontal_scroll(e);
    correct_vertical_scroll(e);
}

static void move_start_of_line(Editor *e) {
    set_cursor_x(e, 0);
    correct_horizontal_scroll(e);
}

static void move_end_of_line(Editor *e) {
    Line *line = e->lines[e->cursor_y];
    set_cursor_x(e, line->len);
    correct_horizontal_scroll(e);
}

static void move_start_of_file(Editor *e) {
    set_cursor_x(e, 0);
    e->cursor_y = 0;
    correct_scroll(e);
}

static void move_end_of_file(Editor *e) {
    e->cursor_y = e->num_lines - 1;
    Line *line = e->lines[e->cursor_y];
    set_cursor_x(e, line->len);
    correct_scroll(e);
}

static void move_left(Editor *e) {
    if (e->cursor_x == 0) {
        if (e->cursor_y == 0) {
            return; // Start of source file
        }
        e->cursor_y--;
        move_end_of_line(e);
    } else {
        set_cursor_x(e, e->cursor_x - 1);
        correct_horizontal_scroll(e);
    }
}

static void move_right(Editor *e) {
    Line *line = e->lines[e->cursor_y];
    if (e->cursor_x >= line->len) {
        if (e->cursor_y >= e->num_lines - 1) {
            return; // End of source file
        }
        e->cursor_y++;
        move_start_of_line(e);
    } else {
        set_cursor_x(e, e->cursor_x + 1);
        correct_horizontal_scroll(e);
    }
}

static void correct_cursor_on_line_movement(Editor *e) {
    if (e->prev_cursor_x == -1) {
        e->prev_cursor_x = e->cursor_x;
    } else {
        e->cursor_x = e->prev_cursor_x;
    }
    Line *line = e->lines[e->cursor_y];
    if (e->cursor_x > line->len) {
        e->cursor_x = line->len;
    }
    correct_scroll(e);
}

static void move_up(Editor *e) {
    if (e->cursor_y == 0) {
        return; // First line in file
    }
    e->cursor_y--;
    correct_cursor_on_line_movement(e);
}

static void move_down(Editor *e) {
    if (e->cursor_y >= e->num_lines - 1) {
        return; // Last line in file
    }
    e->cursor_y++;
    correct_cursor_on_line_movement(e);
}

static int is_word_sep(char ch) {
    for (char *sep = WORD_SEPARATORS; *sep != '\0'; sep++) {
        if (ch == *sep) {
            return 1;
        }
    }
    return 0;
}

static int find_prev_word(Editor *e) {
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

static void move_prev_word(Editor *e) {
    if (e->cursor_x > 0) {
        set_cursor_x(e, find_prev_word(e));
        correct_horizontal_scroll(e);
    } else {
        if (e->cursor_y == 0) {
            return; // Start of file
        }
        e->cursor_y--; // Previous word on the line above
        Line *line = e->lines[e->cursor_y];
        set_cursor_x(e, line->len);
        set_cursor_x(e, find_prev_word(e));
        correct_scroll(e);
    }
}

static int find_next_word(Editor *e) {
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

static void move_next_word(Editor *e) {
    Line *line = e->lines[e->cursor_y];
    if (e->cursor_x < line->len) {
        set_cursor_x(e, find_next_word(e));
        correct_horizontal_scroll(e);
    } else {
        if (e->cursor_y >= e->num_lines - 1) {
            return; // End of file
        }
        e->cursor_y++; // Next word on the line below
        set_cursor_x(e, 0);
        set_cursor_x(e, find_next_word(e));
        correct_scroll(e);
    }
}

static void end_selection(Editor *e) {
    e->select_x = -1;
    e->select_y = -1;
}

static void start_selection(Editor *e) {
    if (!has_selection(e)) { // If nothing already selected
        e->select_x = e->cursor_x; // Start selection at cursor
        e->select_y = e->cursor_y;
    }
}

static void end_selection_left(Editor *e) {
    int min_x, min_y;
    selection_range(e, &min_x, &min_y, NULL, NULL);
    end_selection(e);
    e->cursor_y = min_y;
    set_cursor_x(e, min_x);
}

static void end_selection_right(Editor *e) {
    int max_x, max_y;
    selection_range(e, NULL, NULL, &max_x, &max_y);
    end_selection(e);
    e->cursor_y = max_y;
    set_cursor_x(e, max_x);
}

static void check_for_empty_selection(Editor *e) {
    // End selection if nothing selected
    if (e->select_x == e->cursor_x && e->select_y == e->cursor_y) {
        end_selection(e);
    }
}


// ---- Editing ---------------------------------------------------------------

static void delete_line(Editor *e, int line_idx) {
    free(e->lines[line_idx]);
    int remaining = e->num_lines - line_idx - 1;
    if (remaining > 0) {
        Line **dst = &e->lines[line_idx];
        memcpy(dst, dst + 1, sizeof(Line *) * remaining);
    }
    e->num_lines--;
}

static void delete_range(Editor *e,
                         int min_x, int min_y,
                         int max_x, int max_y) {
    if (min_y == max_y) { // All on one line
        Line *line = e->lines[min_y];
        int remaining = line->len - max_x;
        if (remaining > 0) {
            char *dst = &line->s[min_x];
            char *src = &line->s[max_x];
            memcpy(dst, src, sizeof(char) * remaining);
        }
        line->len -= max_x - min_x;
    } else { // Across multiple lines
        Line *first = e->lines[min_y]; // First line
        first->len = min_x; // Delete to end of line

        for (int y = min_y + 1; y < max_y; y++) { // Lines in between
            delete_line(e, y);
        }

        Line *last = e->lines[max_y]; // Last line
        int remaining = last->len - max_x;
        increase_line_capacity(e, min_y, remaining);
        first = e->lines[min_y]; // In case 'realloc' is called
        if (remaining > 0) {
            // Copy remaining text onto the end of the first line
            char *dst = &first->s[min_x];
            char *src = &last->s[max_x];
            memcpy(dst, src, sizeof(char) * remaining);
            first->len += remaining;
        }
        delete_line(e, max_y);
    }
}

static void backspace_selection(Editor *e) {
    int min_x, min_y, max_x, max_y;
    selection_range(e, &min_x, &min_y, &max_x, &max_y);
    delete_range(e, min_x, min_y, max_x, max_y);

    end_selection(e);
    set_cursor_x(e, min_x);
    e->cursor_y = min_y;
    correct_scroll(e);
}

static void backspace_char(Editor *e) {
    if (e->cursor_x == 0) { // Start of line
        if (e->cursor_y == 0) { // Start of file
            return;
        }
        Line *prev = e->lines[e->cursor_y - 1];
        set_cursor_x(e, prev->len);
        delete_range(e, prev->len, e->cursor_y - 1, 0, e->cursor_y);
        e->cursor_y--;
        correct_scroll(e);
    } else { // Middle of line
        delete_range(e, e->cursor_x - 1, e->cursor_y, e->cursor_x, e->cursor_y);
        set_cursor_x(e, e->cursor_x - 1);
        correct_horizontal_scroll(e);
    }
}

static void backspace(Editor *e) {
    if (has_selection(e)) {
        backspace_selection(e);
    } else {
        backspace_char(e);
    }
}

static void type_char(Editor *e, char ch) {
    if (has_selection(e)) {
        backspace_selection(e);
    }
    increase_line_capacity(e, e->cursor_y, 1);
    Line *line = e->lines[e->cursor_y];
    int remaining = line->len - e->cursor_x;
    if (remaining > 0) {
        char *src = &line->s[e->cursor_x];
        memcpy(src + 1, src, sizeof(char) * remaining);
    }
    line->s[e->cursor_x] = ch;
    line->len++;
    set_cursor_x(e, e->cursor_x + 1);
    correct_horizontal_scroll(e);
}

static void insert_line(Editor *e, int after_idx, Line *to_insert) {
    increase_lines_capacity(e, 1);
    int remaining = e->num_lines - after_idx - 1;
    if (remaining > 0) {
        Line **src = &e->lines[after_idx + 1];
        memcpy(src + 1, src, sizeof(Line *) * remaining);
    }
    e->lines[after_idx + 1] = to_insert;
    e->num_lines++;
}

static void new_line(Editor *e) {
    if (has_selection(e)) {
        backspace_selection(e);
    }
    Line *line = e->lines[e->cursor_y];
    int remaining = line->len - e->cursor_x;
    Line *to_insert;
    if (remaining > 0) {
        to_insert = line_from(&line->s[e->cursor_x], remaining);
    } else {
        to_insert = empty_line();
    }
    line->len = e->cursor_x;
    insert_line(e, e->cursor_y, to_insert);
    e->cursor_y++;
    set_cursor_x(e, 0);
    correct_scroll(e);
}

static void shift_line_up(Editor *e) {
    if (e->cursor_y == 0) {
        return; // First line
    }
    Line *swap = e->lines[e->cursor_y - 1];
    e->lines[e->cursor_y - 1] = e->lines[e->cursor_y];
    e->lines[e->cursor_y] = swap;
    e->cursor_y--;
}

static void shift_line_down(Editor *e) {
    if (e->cursor_y >= e->num_lines - 1) {
        return; // Last line
    }
    Line *swap = e->lines[e->cursor_y + 1];
    e->lines[e->cursor_y + 1] = e->lines[e->cursor_y];
    e->lines[e->cursor_y] = swap;
    e->cursor_y++;
}


// ---- Event Handling --------------------------------------------------------

static void handle_key(Editor *e, struct tb_event ev) {
    if (ev.mod & TB_MOD_SHIFT) { // Start selection
        switch (ev.key) {
            case TB_KEY_ARROW_LEFT:
            case TB_KEY_ARROW_RIGHT:
            case TB_KEY_ARROW_UP:
            case TB_KEY_ARROW_DOWN: start_selection(e); break;
        } // Fall through to movement commands...
    } else if (has_selection(e)) { // End selection
        switch (ev.key) {
            case TB_KEY_ARROW_LEFT:  end_selection_left(e); return;
            case TB_KEY_ARROW_RIGHT: end_selection_right(e); return;
            case TB_KEY_ARROW_UP:    end_selection_left(e); break;  // Fall
            case TB_KEY_ARROW_DOWN:  end_selection_right(e); break; // through
        }
    }

    if (ev.mod & TB_MOD_CTRL) { // Ctrl takes precedence over alt
        switch (ev.key) {
            // Movement
            case TB_KEY_ARROW_LEFT:  move_start_of_line(e); return;
            case TB_KEY_ARROW_RIGHT: move_end_of_line(e); return;
            case TB_KEY_ARROW_UP:    move_start_of_file(e); return;
            case TB_KEY_ARROW_DOWN:  move_end_of_file(e); return;
        }
    } else if (ev.mod & TB_MOD_ALT) {
         switch (ev.key) {
             // Movement
             case TB_KEY_ARROW_LEFT:  move_prev_word(e); return;
             case TB_KEY_ARROW_RIGHT: move_next_word(e); return;

             // Editing
             case TB_KEY_ARROW_UP:   shift_line_up(e); return;
             case TB_KEY_ARROW_DOWN: shift_line_down(e); return;
         }
    } // Otherwise, fall through to the non-modifier command...

    switch (ev.key) {
        // Movement
        case TB_KEY_ARROW_LEFT:  move_left(e); break;
        case TB_KEY_ARROW_RIGHT: move_right(e); break;
        case TB_KEY_ARROW_UP:    move_up(e); break;
        case TB_KEY_ARROW_DOWN:  move_down(e); break;

        // Editing
        case TB_KEY_ENTER:      new_line(e); break;
        case TB_KEY_BACKSPACE:
        case TB_KEY_BACKSPACE2: backspace(e); break;

        // Quit
        case TB_KEY_CTRL_Q: e->run = 0; break;
    }
    check_for_empty_selection(e);
}

static void handle_char(Editor *e, struct tb_event ev) {
    if (ev.ch < 256) { // ASCII support only for now
        type_char(e, (char) ev.ch);
    }
}

void editor_update(Editor *e, struct tb_event ev) {
    if (ev.type == TB_EVENT_KEY && ev.key != 0) {
        handle_key(e, ev);
    } else if (ev.type == TB_EVENT_KEY && ev.ch != 0) {
        handle_char(e, ev);
    }
}