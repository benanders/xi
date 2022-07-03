
#ifndef XI_EDITOR_H
#define XI_EDITOR_H

#include <termbox.h>

typedef struct {
    int len, max;
    char s[];
} Line;

typedef struct {
    char *path; // File we're editing, or NULL if it hasn't been saved yet
    int scroll_x, scroll_y;
    int cursor_x, cursor_y; // Absolute position within 'lines'
    Line **lines;
    int num_lines, max_lines;
    int run;
} Editor;

Editor editor_new();
Editor editor_open(char *path);
void editor_draw(Editor *e);
void editor_update(Editor *e, struct tb_event ev);

#endif
