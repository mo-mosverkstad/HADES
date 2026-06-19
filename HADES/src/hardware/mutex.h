#pragma once
#include "io_bus.h"

// DTEK-V Hardware Mutex
// Provides atomic lock/unlock for multi-core synchronization.
//
// Registers (offsets from base):
//   0x00 MUTEX: Read: [31:1] OWNER, [0] VALUE (locked/unlocked)
//              Write: attempt to acquire (atomic test-and-set)
//   0x04 RESET: Write any value to force-unlock
//
// Lock acquisition:
//   CPU writes: OWNER=cpu_id, VALUE=1
//   CPU reads back: if OWNER matches → lock acquired
//   If already locked by another owner → write has no effect
//
// Unlock:
//   Owner writes VALUE=0 (or any write with VALUE bit = 0)

class Mutex : public IODevice {
public:
    Mutex() { reset(); }

    void reset() {
        owner_ = 0;
        locked_ = false;
        contention_count_ = 0;
    }

    uint32_t read(uint32_t offset) override {
        switch (offset) {
            case 0x00: // MUTEX register
                return (owner_ << 1) | (locked_ ? 1 : 0);
            default:
                return 0;
        }
    }

    void write(uint32_t offset, uint32_t value) override {
        switch (offset) {
            case 0x00: { // MUTEX register: atomic test-and-set
                uint32_t new_owner = value >> 1;
                bool new_value = (value & 1) != 0;

                if (!new_value) {
                    // Unlock attempt
                    if (locked_ && owner_ == new_owner) {
                        locked_ = false;
                        owner_ = 0;
                    }
                } else {
                    // Lock attempt
                    if (!locked_) {
                        // Free -> acquire
                        locked_ = true;
                        owner_ = new_owner;
                    } else if (owner_ == new_owner) {
                        // Already own it -> re-entrant (no-op)
                    } else {
                        // Contention: another owner holds it
                        contention_count_++;
                    }
                }
                break;
            }
            case 0x04: // RESET: force unlock
                locked_ = false;
                owner_ = 0;
                break;
            default:
                break;
        }
    }

    void tick() override {
        // Mutex doesn't need per-cycle tick
    }

    bool irq_pending() override {
        return false; // Mutex doesn't generate interrupts
    }

    // Stats
    bool is_locked() const { return locked_; }
    uint32_t get_owner() const { return owner_; }
    uint64_t get_contention_count() const { return contention_count_; }

private:
    uint32_t owner_;
    bool locked_;
    uint64_t contention_count_;
};