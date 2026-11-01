#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace metavm { namespace machines {
class Chip8 {
public:
    enum class Status { running, waiting, invalid_opcode, bounds, stack_overflow, stack_underflow, waiting_vblank };
    enum class Profile { legacy, cosmac_vip, chip48 };
    enum class KeyWait { held, press, release };
    struct Quirks {
        // Preserve the original four aggregate fields and their defaults.
        bool shift_uses_y=false, increment_i=false, jump_uses_x=false, clip_sprites=false;
        bool logic_resets_vf=false, increment_i_by_x=false, wait_for_vblank=false;
        KeyWait key_wait=KeyWait::held;
    } quirks;
    std::array<uint8_t,4096> memory{};
    std::array<uint8_t,16> v{};
    std::array<uint16_t,16> stack{};
    std::array<bool,16> keys{};
    std::array<uint8_t,64*32> display{};
    uint16_t pc=0x200,i=0;
    uint8_t sp=0,delay=0,sound=0;
    uint32_t random_state=0x12345678;
    Chip8();
    explicit Chip8(Profile profile);
    // Selected classic-opcode semantics, not full hardware emulation. Invalid
    // profile values throw std::invalid_argument; see docs/CHIP8.md.
    static Quirks preset(Profile profile);
    bool load(const std::vector<uint8_t>& rom);
    Status step();
    // Host-scheduled timer/vblank tick, including while step() is waiting.
    void tick_60hz();
private:
    uint16_t blocked_pc_=0,blocked_opcode_=0;
    bool drawing_=false,draw_ready_=false,key_waiting_=false;
    KeyWait blocked_mode_=KeyWait::held;
    int selected_key_=-1;
    std::array<bool,16> previous_keys_{};
    void cancel_wait();
};
}}
