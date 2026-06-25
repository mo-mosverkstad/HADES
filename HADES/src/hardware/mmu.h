#pragma once
#include <cstdint>
#include <cstring>

// RISC-V Sv32 MMU (Memory Management Unit)
//
// Virtual address (32-bit): [VPN1(10) | VPN0(10) | offset(12)]
// Physical address: [PPN(20) | offset(12)]
// Page size: 4KB (12-bit offset)
// Two-level page table: 1024 entries per level, 4 bytes each
//
// Page Table Entry (PTE) format:
//   [PPN(22) | RSW(2) | D(1) | A(1) | G(1) | U(1) | X(1) | W(1) | R(1) | V(1)]
//   Bits [31:10] = PPN (physical page number)
//   Bit 0 = V (valid)
//   Bit 1 = R (readable)
//   Bit 2 = W (writable)
//   Bit 3 = X (executable)
//   Bit 4 = U (user-mode accessible)
//   Bit 5 = G (global)
//   Bit 6 = A (accessed)
//   Bit 7 = D (dirty)
//
// SATP CSR (0x180):
//   [MODE(1) | ASID(9) | PPN(22)]
//   MODE=0: no translation, MODE=1: Sv32

class MMU {
public:
    static constexpr uint32_t PAGE_SIZE = 4096;       // 4KB
    static constexpr uint32_t PTE_SIZE = 4;           // 4 bytes per entry
    static constexpr uint32_t PT_ENTRIES = 1024;      // entries per page table
    static constexpr uint32_t OFFSET_BITS = 12;
    static constexpr uint32_t VPN_BITS = 10;
    static constexpr uint32_t TLB_SIZE = 16;

    // PTE flags
    static constexpr uint32_t PTE_V = 1 << 0;  // valid
    static constexpr uint32_t PTE_R = 1 << 1;  // read
    static constexpr uint32_t PTE_W = 1 << 2;  // write
    static constexpr uint32_t PTE_X = 1 << 3;  // execute
    static constexpr uint32_t PTE_U = 1 << 4;  // user
    static constexpr uint32_t PTE_G = 1 << 5;  // global
    static constexpr uint32_t PTE_A = 1 << 6;  // accessed
    static constexpr uint32_t PTE_D = 1 << 7;  // dirty

    enum FaultType { NONE = 0, LOAD_PAGE_FAULT = 13, STORE_PAGE_FAULT = 15, INST_PAGE_FAULT = 12 };

    MMU() { reset(); }

    void reset() {
        enabled_ = false;
        satp_ppn_ = 0;
        for (uint32_t i = 0; i < TLB_SIZE; i++) tlb_[i].valid = false;
        tlb_hits_ = 0;
        tlb_misses_ = 0;
        page_faults_ = 0;
        last_fault_ = NONE;
        last_fault_addr_ = 0;
    }

    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    // Set page table base (from SATP CSR write)
    void set_satp(uint32_t satp) {
        satp_ppn_ = satp & 0x003FFFFF;  // lower 22 bits = PPN
        // MODE is bit 31 (0=bare, 1=Sv32)
        enabled_ = (satp >> 31) != 0;
        flush_tlb();  // changing SATP invalidates TLB
    }

    uint32_t get_satp() const {
        return (enabled_ ? (1u << 31) : 0) | satp_ppn_;
    }

    // Translate virtual address to physical address
    // Returns physical address, or sets fault if translation fails
    // mem_read_word: function to read a 32-bit word from physical memory
    // (passed as parameter to avoid circular dependency with Memory class)
    template<typename MemReadFn>
    uint32_t translate(uint32_t vaddr, bool is_write, bool is_exec, MemReadFn mem_read_word) {
        if (!enabled_) return vaddr;  // no translation

        last_fault_ = NONE;

        // Check TLB first
        uint32_t vpn = vaddr >> OFFSET_BITS;
        uint32_t offset = vaddr & (PAGE_SIZE - 1);

        for (uint32_t i = 0; i < TLB_SIZE; i++) {
            if (tlb_[i].valid && tlb_[i].vpn == vpn) {
                tlb_hits_++;
                // Check permissions
                if (!check_permissions(tlb_[i].pte_flags, is_write, is_exec)) {
                    set_fault(is_exec ? INST_PAGE_FAULT : (is_write ? STORE_PAGE_FAULT : LOAD_PAGE_FAULT), vaddr);
                    return 0;
                }
                return (tlb_[i].ppn << OFFSET_BITS) | offset;
            }
        }

        // TLB miss: walk page table
        tlb_misses_++;

        uint32_t vpn1 = (vaddr >> 22) & 0x3FF;
        uint32_t vpn0 = (vaddr >> 12) & 0x3FF;

        // Level 1: read PTE from root page table
        uint32_t pt_base = satp_ppn_ << OFFSET_BITS;
        uint32_t pte1_addr = pt_base + vpn1 * PTE_SIZE;
        uint32_t pte1 = mem_read_word(pte1_addr);

        if (!(pte1 & PTE_V)) {
            page_faults_++;
            set_fault(is_exec ? INST_PAGE_FAULT : (is_write ? STORE_PAGE_FAULT : LOAD_PAGE_FAULT), vaddr);
            return 0;
        }

        // Check if this is a leaf PTE (superpage) or pointer to next level
        if (pte1 & (PTE_R | PTE_W | PTE_X)) {
            // Superpage (4MB) — not commonly used, treat as fault for simplicity
            page_faults_++;
            set_fault(LOAD_PAGE_FAULT, vaddr);
            return 0;
        }

        // Level 0: read PTE from second-level page table
        uint32_t pt1_base = (pte1 >> 10) << OFFSET_BITS;
        uint32_t pte0_addr = pt1_base + vpn0 * PTE_SIZE;
        uint32_t pte0 = mem_read_word(pte0_addr);

        if (!(pte0 & PTE_V)) {
            page_faults_++;
            set_fault(is_exec ? INST_PAGE_FAULT : (is_write ? STORE_PAGE_FAULT : LOAD_PAGE_FAULT), vaddr);
            return 0;
        }

        // Check permissions
        if (!check_permissions(pte0, is_write, is_exec)) {
            page_faults_++;
            set_fault(is_exec ? INST_PAGE_FAULT : (is_write ? STORE_PAGE_FAULT : LOAD_PAGE_FAULT), vaddr);
            return 0;
        }

        // Extract physical page number
        uint32_t ppn = pte0 >> 10;
        uint32_t paddr = (ppn << OFFSET_BITS) | offset;

        // Insert into TLB (LRU: replace oldest)
        insert_tlb(vpn, ppn, pte0);

        return paddr;
    }

    void flush_tlb() {
        for (uint32_t i = 0; i < TLB_SIZE; i++) tlb_[i].valid = false;
    }

    // Fault info (read by CPU to handle page fault)
    FaultType get_last_fault() const { return last_fault_; }
    uint32_t get_last_fault_addr() const { return last_fault_addr_; }
    void clear_fault() { last_fault_ = NONE; }

    // Stats
    uint64_t get_tlb_hits() const { return tlb_hits_; }
    uint64_t get_tlb_misses() const { return tlb_misses_; }
    uint64_t get_page_faults() const { return page_faults_; }

private:
    bool enabled_;
    uint32_t satp_ppn_;  // page table base physical page number

    struct TLBEntry {
        bool valid;
        uint32_t vpn;       // virtual page number
        uint32_t ppn;       // physical page number
        uint32_t pte_flags; // permission bits from PTE
        uint32_t age;       // for LRU
    };
    TLBEntry tlb_[TLB_SIZE];
    uint32_t tlb_age_counter_ = 0;

    uint64_t tlb_hits_;
    uint64_t tlb_misses_;
    uint64_t page_faults_;
    FaultType last_fault_;
    uint32_t last_fault_addr_;

    bool check_permissions(uint32_t pte, bool is_write, bool is_exec) {
        if (is_exec && !(pte & PTE_X)) return false;
        if (is_write && !(pte & PTE_W)) return false;
        if (!is_write && !is_exec && !(pte & PTE_R)) return false;
        return true;
    }

    void set_fault(FaultType type, uint32_t addr) {
        last_fault_ = type;
        last_fault_addr_ = addr;
    }

    void insert_tlb(uint32_t vpn, uint32_t ppn, uint32_t pte_flags) {
        // Find invalid entry or LRU entry
        int victim = 0;
        uint32_t min_age = UINT32_MAX;
        for (uint32_t i = 0; i < TLB_SIZE; i++) {
            if (!tlb_[i].valid) { victim = i; break; }
            if (tlb_[i].age < min_age) { min_age = tlb_[i].age; victim = i; }
        }
        tlb_[victim].valid = true;
        tlb_[victim].vpn = vpn;
        tlb_[victim].ppn = ppn;
        tlb_[victim].pte_flags = pte_flags;
        tlb_[victim].age = ++tlb_age_counter_;
    }
};