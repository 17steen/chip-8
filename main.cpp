#include "./computer.h"
#include <chrono>
#include <cwchar>
#include <fstream>
#include <iostream>
#include <thread>

#include <ncurses.h>

void
draw_pixel(ch8::byte display[ch8::H][ch8::W])
{
    for (std::size_t h = 0; h < ch8::H; ++h) {
        move(h, 0);
        for (std::size_t w = 0; w < ch8::W; ++w) {
            char ch = display[h][w] ? (char)219 : ' ';
            addch(ch | A_BOLD);
        }
    }
    refresh();
}

using namespace std::chrono_literals;

int
main(int, char*[])
{
    auto chip = ch8::computer();

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, true);

    auto file =
      std::ifstream("./invaders.ch8", std::ios::binary | std::ios::ate);
    if (!file.is_open())
        std::terminate();

    auto size = file.tellg();
    ch8::byte* buffer = new ch8::byte[size];
    file.seekg(0, std::ios::beg);
    file.read((char*)buffer, size);
    file.close();
    chip.load(buffer, size);
    delete[] buffer;

    wchar_t keys[] = { 'g', '+', '-', '/', 'v', 'd', 'l', 't',
                       's', 'r', 'q', 'h', '*', 'j', 'n', 'f' };

    while (not chip.end()) {
        std::this_thread::sleep_for(1'000'000us / 240);
        chip.tick();

        draw_pixel(chip.display);

        int ch = getch();
        if (ch != ERR) {
            int i = 0;
            for (auto key : keys) {
                if (ch == key) {
                    mvprintw(20, 20, "pressed %c ", ch);
                    // overwrites K, otherwise we wait for K to be consumed by
                    // the emulator
                    chip.K = i;
                    break;
                }
                ++i;
            }
            if (i == 0x10) {
                mvprintw(25, 20, "nothing pressed %c ", ch);
            }
        }
    }
    endwin();

    std::cout << "clean exit" << std::endl;

    return 0;
}
