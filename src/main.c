
#include "editor.h"

#define TB_IMPL
#include <termbox.h>

int main(int argc, char *argv[]) {
    tb_init();

    Editor editor;
    if (argc == 2) { // argv[0] is the executable name
        editor = editor_open(argv[1]);
    } else {
        editor = editor_new();
    }

    editor_draw(&editor);
    while (editor.run) {
        struct tb_event ev;
        tb_poll_event(&ev);
        editor_update(&editor, ev);
        editor_draw(&editor);
    }
    tb_shutdown();
}
