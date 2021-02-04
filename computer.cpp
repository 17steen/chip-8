#include "computer.h"
#include <bits/stdint-uintn.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <ncurses.h>

using namespace std::chrono_literals;

void
ch8::computer::load(const byte* data, size_t size) noexcept
{
    std::uninitialized_copy_n(data, size, &ram[0x200]);
}

inline void
warn(const char* what)
{
    if constexpr (DEBUG) {
        std::cerr << what << std::endl;
    }
}

ch8::computer::computer()
  : gen(ch::steady_clock::now().time_since_epoch().count())
{
    // init memory and display at 0;
    PC = 0x200;
    generate_font();
}

void
ch8::computer::generate_font() noexcept
{
    for (size_t i = 0; auto hex : { 0xF999F,
                                    0x26227,
                                    0xF1F8F,
                                    0xF1F1F,
                                    0x99F11,
                                    0xF8F1F,
                                    0xF8F9F,
                                    0xF1244,
                                    0xF9F9F,
                                    0xF9F1F,
                                    0xF9F99,
                                    0xE9E9E,
                                    0xF888F,
                                    0xE999E,
                                    0xF8F8F,
                                    0xF8F88 }) {
        for (int j = 0; j < 5; ++j) {
            // http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#2.4
            font[i][4 - j] = ((hex >> (j * 4)) & 0xF) << 4;
        }
        ++i;
    }
}

constexpr void
ch8::computer::clear_screen() noexcept
{
    // i hope my compiler knows that this memory is contiguous
    for (auto& col : display) {
        for (auto& pixel : col) {
            pixel = 0;
        }
    }
}

// might be rendering sprites upside down
constexpr void
ch8::computer::draw_sprite(byte Vx, byte Vy, byte n) noexcept
{

    for (size_t i = 0; i < n; ++i) {
        byte sprite_b = ram[I + i];
        byte y = (Vy + i) % H;
        auto& row = display[y]; // wraps around
        for (size_t j = 0; j < 8; ++j) {
            const byte x = (Vx + j) % W;

            auto& pixel = row[x];
            // big endianness
            const byte bit = (sprite_b >> (8 - j - 1)) & 1;
            if (pixel and bit)
                V[0xF] |= 1;

            pixel ^= bit;
        }
    }
}

void
ch8::computer::tick()
{

    const uint16_t opcode = (ram[PC] << 8u) | ram[PC + 1u];

    PC += 2;

    execute(opcode);

    auto now = std::chrono::steady_clock::now();

    // every 60 hertz
    constexpr auto HZ60 = (1'000'000us / 60);

    if (now - last_tick > HZ60) {
        if (delay_timer > 0) {
            --delay_timer;
        }

        if (sound_timer > 0) {
            --sound_timer;
        }
        last_tick = now;
    }
}

inline void
ch8::computer::execute(uint16_t opcode)
{
    // ram is stored as bytes, but an instructinon is 2 bytes

    // this commented code puts bytes in the wrong order
    // const uint16_t opcode = *(uint16_t *)&ram[PC];

    const uint16_t nnn = opcode & 0x0FFFu;
    const byte x = (opcode & 0x0F00u) >> 8u;
    const byte y = (opcode & 0x00F0u) >> 4u;
    const byte kk = opcode;
    const byte nibble = nnn & 0xF;
    auto& Vx = V[x];
    auto& Vy = V[y];
    auto& Vf = V[0xF];

    if (PC < 0x200)
        throw "Segfault";

    if constexpr (DEBUG) {
        printf("OP : %04X\n", opcode);
        printf("PC : [ %X ]\n", (int)PC);

        printf("V[x: %01X ] : %02X\n", (int)x, (int)Vx);
        printf("V[y: %01X ] : %02X\n", (int)y, (int)Vy);
        printf("nnn : [ %03X ], kk : [ %02X ]\n", (int)nnn, (int)kk);
        printf("STCK : [ %04X ]\n", (int)stack_ptr);
        printf("I: [ %X ]\n", (int)I);
    }

    // exact matches
    switch (opcode) {
        case 0x0000:
            throw "Program requested exit.";
        case CLS:
            warn("clear screen");
            clear_screen();
            break;
        case RET:
            warn("RETURN");
            --stack_ptr;
            if (stack_ptr >= 0x10)
                throw "Stack underflow";
            PC = stack[stack_ptr];
            break;
        default:
            // fist byte match
            switch (opcode & 0xF000) {
                case JP:
                    warn("jump");
                    PC = nnn;
                    break;
                case CALL:
                    warn("call");
                    stack[stack_ptr] = PC;
                    ++stack_ptr;
                    if (stack_ptr >= 0x10)
                        throw "Stack overflow";
                    PC = nnn;
                    break;
                case SE:
                    if constexpr (DEBUG)
                        fprintf(stderr, "%x == %x ?\n", (int)Vx, (int)kk);
                    if (Vx == kk) {
                        PC += 2;
                        warn("  YES");
                    } else {
                        warn(" NO");
                    }

                    break;
                case SNE:
                    if constexpr (DEBUG)
                        fprintf(stderr, "%x != %x ?\n", (int)Vx, (int)kk);
                    if (Vx != kk) {
                        PC += 2;
                        warn(" YES");
                    } else {
                        warn(" NO");
                    }
                    break;
                case SE_REG:
                    warn("equal to(reg)");
                    if (Vx == Vy)
                        PC += 2;
                    break;
                case LD_C:
                    warn("loading constant");
                    Vx = kk;
                    break;
                case ADD_C:
                    warn("add constant");
                    Vx += kk;
                    break;
                case REG_OP:
                    warn("reg operation");
                    // last byte match
                    switch (nibble) {
                        case LD:
                            warn("load");
                            Vx = Vy;
                            break;
                        case OR:
                            warn("or");
                            Vx |= Vy;
                            break;
                        case AND:
                            warn("and");
                            Vx &= Vy;
                            break;
                        case XOR:
                            warn("xor");
                            Vx ^= Vy;
                            break;
                        case ADD:
                            warn("and");
                            Vf = (int)Vx + Vy > 0xFF;
                            Vx += Vy;
                            break;
                        case SUB:
                            warn("sub");

                            Vf = (Vx > Vy);

                            Vx -= Vy;
                            break;
                        case SHR:
                            warn("shift right");
                            Vf = Vx & 0x1;
                            Vx = Vx >> 1;
                            break;
                        case SUBN:
                            warn("sub n");
                            Vf = (Vy > Vx);

                            Vx = Vy - Vx;
                            break;
                        case SHL:
                            warn("shift left");
                            Vf = (Vx & (1 << 7)) >> 7;
                            Vx = Vx << 1;
                            break;
                        default:
                            throw "unrecognized reg_op";
                    }
                    break;
                case SNE_REG:
                    warn("not equal (reg)");
                    if (Vx != Vy)
                        PC += 2;
                    break;
                case SET_I:
                    warn("setting I !!!");
                    I = nnn;
                    break;
                case JP_V:
                    warn("jump + V[0]");
                    PC = nnn + V[0];
                    break;
                case RND:
                    warn("random");
                    Vx = (gen() % 255) & kk;
                    break;
                case DRW:
                    warn("draw");
                    draw_sprite(Vx, Vy, nibble);
                    break;

                case KBD:
                    warn("kbd event");
                    // testing last byte
                    switch (kk) {
                            // TODO: kb input
                        case SKP:
                            warn("skip if key");
                            if (K == Vx) {
                                PC += 2;
                                K = -1;
                            }
                            break;
                        case SKPN:
                            warn("not skip if key");
                            if (K != Vx)
                                PC += 2;
                            else
                                K = -1;
                            break;
                        default:
                            throw "unknow kb event";
                            break;
                    };
                    break;
                case MISC:
                    warn("miscellaneous");
                    switch (kk) {
                        case VX_DT:
                            warn("vx = delay_timer");
                            Vx = delay_timer;
                            break;
                        case WAIT:
                            warn("wait until");
                            if (K == -1) {
                                throw "just a test";
                                mvprintw(20, 20, "waiting");
                                PC -= 2;
                            }
                            break;
                        case SET_DT:
                            warn("delay_timer = vx");
                            delay_timer = Vx;
                            break;
                        case SET_ST:
                            warn("sound_timer = vx");
                            sound_timer = Vx;
                            break;
                        case ADD_I:
                            warn("I += vx");
                            I += Vx;
                            break;
                        case SPRITE:
                            if constexpr (DEBUG)
                                fprintf(
                                  stderr, "loading letter : %02x\n", (int)Vx);

                            I = (uint16_t)((byte*)font - ram) + (Vx * 5);
                            break;

                        case BCD:
                            warn("bcd");
                            ram[I + 2] = Vx % 10;
                            ram[I + 1] = (Vx / 10) % 10;
                            ram[I + 0] = (Vx / 100) % 10;
                            break;
                        case STORE:
                            warn("store");
                            for (uint16_t i = 0; i <= x; ++i) {
                                if (I + i < 0x200) {
                                    throw "Segfault : write to system memory";
                                }
                                if (i > 0xF) {
                                    throw "REGISTER DOESN'T EXIST";
                                }
                                ram[I + i] = V[i];
                            }
                            break;
                        case READ:
                            warn("read");
                            for (uint16_t i = 0; i <= x; ++i) {
                                if (I + i < 0x200) {
                                    warn("READING SYSTEM MEM");
                                    throw "READING SYSTEM MEM";
                                }
                                if (i > 0xF) {
                                    throw "REGISTER DOESN'T EXIST";
                                }
                                V[i] = ram[I + i];
                            }
                            break;
                    }
                    break;
                default:
                    throw "unrecognized opcode";
            }
    }
}
