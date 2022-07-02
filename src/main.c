
#include <ncurses.h>

int main() {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, 1);

    clear();
    refresh();
    getch();

    endwin();
}
