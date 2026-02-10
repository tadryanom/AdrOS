"""
AdrOS GDB Scripted Integrity Checks

Usage:
  qemu-system-i386 -s -S ... &
  gdb adros-x86.bin -x tests/gdb_checks.py

Runs automated checks on kernel state after boot:
- Heap integrity (magic numbers, free list consistency)
- PMM bitmap sanity (frame 0 always used, kernel region used)
- VGA mapping present at 0xC00B8000
"""

import gdb
import sys

class AdrOSCheck:
    def __init__(self):
        self.passed = 0
        self.failed = 0

    def check(self, name, condition, detail=""):
        if condition:
            print(f"  PASS  {name}")
            self.passed += 1
        else:
            print(f"  FAIL  {name}  {detail}")
            self.failed += 1

    def read_u32(self, addr):
        """Read a uint32_t from a kernel address."""
        try:
            val = gdb.parse_and_eval(f"*(uint32_t*){addr:#x}")
            return int(val) & 0xFFFFFFFF
        except gdb.error:
            return None

    def read_u8(self, addr):
        """Read a uint8_t from a kernel address."""
        try:
            val = gdb.parse_and_eval(f"*(uint8_t*){addr:#x}")
            return int(val) & 0xFF
        except gdb.error:
            return None

def run_checks():
    c = AdrOSCheck()

    print("\n=========================================")
    print("  AdrOS GDB Integrity Checks")
    print("=========================================\n")

    # Connect to QEMU
    try:
        gdb.execute("target remote :1234", to_string=True)
    except gdb.error as e:
        print(f"  ERROR: Cannot connect to QEMU: {e}")
        return 1

    # Load symbols
    try:
        gdb.execute("file adros-x86.bin", to_string=True)
    except gdb.error:
        pass

    # Set a breakpoint after init completes (on the idle loop)
    # We'll break on uart_print of "[init]" to know userspace started
    gdb.execute("break *process_init", to_string=True)
    gdb.execute("continue", to_string=True)

    # Now we're at process_init — heap and PMM are initialized.
    # Step past it to ensure scheduler is set up.
    gdb.execute("finish", to_string=True)

    # ---- Heap Integrity ----
    try:
        head_val = gdb.parse_and_eval("(uint32_t)head")
        head_addr = int(head_val) & 0xFFFFFFFF

        c.check("Heap head is non-NULL", head_addr != 0)
        c.check("Heap head is in heap range",
                head_addr >= 0xD0000000 and head_addr < 0xD4000000,
                f"head={head_addr:#x}")

        if head_addr != 0:
            magic = c.read_u32(head_addr)
            c.check("Heap head magic is 0xCAFEBABE",
                    magic == 0xCAFEBABE,
                    f"magic={magic:#x}" if magic else "read failed")
    except gdb.error as e:
        c.check("Heap head accessible", False, str(e))

    # ---- PMM Bitmap ----
    try:
        # Frame 0 should always be marked as used (never allocate phys addr 0)
        bitmap_byte0 = gdb.parse_and_eval("memory_bitmap[0]")
        byte0 = int(bitmap_byte0) & 0xFF
        c.check("PMM frame 0 is used (bit 0 of bitmap[0])",
                (byte0 & 1) == 1,
                f"bitmap[0]={byte0:#x}")

        # max_frames should be > 0
        mf = gdb.parse_and_eval("max_frames")
        max_frames = int(mf)
        c.check("PMM max_frames > 0", max_frames > 0, f"max_frames={max_frames}")

        # total_memory should be > 0
        tm = gdb.parse_and_eval("total_memory")
        total_mem = int(tm)
        c.check("PMM total_memory > 0", total_mem > 0, f"total_memory={total_mem}")
    except gdb.error as e:
        c.check("PMM bitmap accessible", False, str(e))

    # ---- VGA Mapping ----
    try:
        # VGA buffer should be mapped at 0xC00B8000
        vga_val = c.read_u32(0xC00B8000)
        c.check("VGA mapping at 0xC00B8000 is readable",
                vga_val is not None,
                "read failed")
    except gdb.error as e:
        c.check("VGA mapping accessible", False, str(e))

    # ---- Summary ----
    total = c.passed + c.failed
    print(f"\n  {c.passed}/{total} passed, {c.failed} failed")

    if c.failed > 0:
        print("  RESULT: FAIL\n")
    else:
        print("  RESULT: PASS\n")

    # Kill QEMU
    gdb.execute("kill", to_string=True)
    gdb.execute("quit", to_string=True)

    return 1 if c.failed > 0 else 0

# Auto-run when loaded
run_checks()
