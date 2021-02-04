#include <algorithm>
#include <bits/stdint-uintn.h>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <memory>
#include <random>

namespace ch8 {
namespace ch = std::chrono;
using byte = uint8_t;

using draw_routine = void (*)(uint32_t x, uint32_t y, bool);

using const_font = byte[0x10][5];

constexpr size_t W = 64, H = 32;

struct computer
{
    union
    {
        byte ram[0x1000] = {};
        // [0x0 0x200) bytes are reserved for the interpreter
        struct
        {
            const_font font;
            byte V[0x10];
            uint16_t I;
            uint16_t PC;
            uint16_t stack[0x10];
            byte stack_ptr;
            byte delay_timer;
            byte sound_timer;
            wchar_t K;
            // byte is either 1 or 0;
        };
        // static_assert(sizeof(u) <= 0x200, "using too much memory");
    };

    byte display[H][W] = {};
    ch::time_point<ch::steady_clock> last_tick = ch::steady_clock::now();

    std::mt19937 gen;

    computer();
    // http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#2.4
    void generate_font() noexcept;
    constexpr void clear_screen() noexcept;
    inline void execute(uint16_t opcode);
    void tick();

    constexpr void draw_sprite(byte Vx, byte Vy, byte n) noexcept;
    void load(const byte* data, size_t size) noexcept;
    constexpr bool end() noexcept { return not PC; }
};

enum INSTRUCTION
{
    // whole 16 bits;
    CLS = 0x00E0,
    RET = 0x00EE,

    // mask first 4;
    // set PC to nnn
    JP = 0x1000,
    CALL = 0x2000,

    // reg means second operand is a register not a constant (kk)
    SE = 0x3000,
    SNE = 0x4000,
    SE_REG = 0x5000,

    // constants, disambiguate from reg_op versions
    LD_C = 0x6000,
    ADD_C = 0x7000,

    // if reg_op, last 4 bits have to be tested for the kind of operation
    REG_OP = 0x8000,

    // test first 4 bits
    /**** REG_OP SPECIFICATION ****/
    LD = 0x0,
    OR = 0x1,
    AND = 0x2,
    XOR = 0x3,
    ADD = 0x4,
    SUB = 0x5,
    SHR = 0x6,
    SUBN = 0x7,
    SHL = 0xE,
    /**** REG_OP SPECIFICATION ****/

    SNE_REG = 0x9000,

    // LD I, addr
    SET_I = 0xA000,

    // same as jp but V0 is added to nnn
    JP_V = 0xB000,

    RND = 0xC000,

    DRW = 0xD000,

    // mask 0xF0FF
    // keyboard conditionals
    KBD = 0xE000,

    SKP = 0x9E,
    SKPN = 0xA1,
    // keyboard conditionals

    // mask 0xF0FF
    /******** MISC *******/
    MISC = 0xF000,
    // LD Vx, DT
    VX_DT = 0x07,

    // LD Vx, K
    WAIT = 0x0A,

    SET_DT = 0x15,

    SET_ST = 0x18,

    // ADD I, Vx
    ADD_I = 0x1E,

    // LD F, Vx
    SPRITE = 0x29,

    BCD = 0x33,

    STORE = 0x55,

    READ = 0x65,
    /******** MISC *******/

};

} // namespace ch8
