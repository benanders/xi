
#ifndef XI_EDITOR_H
#define XI_EDITOR_H

#include <termbox.h>

typedef struct {
    uintattr_t text_fg;
    uintattr_t text_bg;
    uintattr_t selection_fg;
    uintattr_t selection_bg;
    int highlight_line;
    uintattr_t highlight_fg;
    uintattr_t highlight_bg;
    int show_gutter;
    uintattr_t gutter_fg;
    uintattr_t gutter_bg;
    int show_info_bar;
    uintattr_t info_bar_fg;
    uintattr_t info_bar_bg;
} Theme;

typedef struct {
    int len, max;
    char s[];
} Line;

typedef struct {
    int run;
    char *path; // File we're editing, or NULL if it hasn't been saved yet
    int scroll_x, scroll_y;
    int cursor_x, cursor_y; // Absolute position within 'lines'
    int prev_cursor_x; // Used when moving cursor up/down lines
    int select_x, select_y;
    Line **lines;
    int num_lines, max_lines;
    Theme theme;
} Editor;

Editor editor_new();
Editor editor_open(char *path);
void editor_draw(Editor *e);
void editor_update(Editor *e, struct tb_event ev);

#endif
