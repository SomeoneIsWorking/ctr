"""GDB half of CTR's exact budget-slice diagnostic; loaded by ctr_budget_probe.py."""

from __future__ import annotations

import json

import gdb

TARGETS = (0x8006A610, 0x8006A57C, 0x8006A6B0)
RESUME_PC = 0x8006A57C
NEGATIVE_PC = 0xDEADBEEF
BF_BEGIN = 0x000AB9F0
BF_END = 0x000B97FC
BF_PROLOGUE = 0x800ABDE0
BF_PROLOGUE_WORD = 0xAFB3002C
FIRST_ENTRIES = 8
MAX_BLOCK_ENTRIES = 4096


def number(expression: str) -> int:
    return int(gdb.parse_and_eval(expression))


def image_admission() -> tuple[int, int]:
    """Inspect the active published BF0233 range, without calling GDB's broken optional ABI."""
    entries = gdb.parse_and_eval("core.imageCatalog_->entries_")
    begin = entries["_M_impl"]["_M_start"]
    end = entries["_M_impl"]["_M_finish"]
    scanned = 0
    matched = 0
    cursor = begin
    while cursor != end:
        entry = cursor.dereference()
        scanned += 1
        if str(entry["name"]) == '"BF0233"' and bool(entry["active"]):
            ranges = entry["ranges"]["_M_impl"]
            if ranges["_M_finish"] - ranges["_M_start"] == 1:
                span = ranges["_M_start"].dereference()
                if int(span["begin"]) == BF_BEGIN and int(span["end"]) == BF_END:
                    matched += 1
        cursor += 1
    if number(f"core.mem_r32({BF_PROLOGUE})") != BF_PROLOGUE_WORD:
        matched = 0
    return scanned, matched


class BlockObserver(gdb.Breakpoint):
    """Observe shipping Lightrec block entries during only the continued budget slice."""

    def __init__(self) -> None:
        super().__init__("lightrec_run_block_boundary", internal=True)
        self.silent = True
        self.scanned = 0
        self.hits = {pc: 0 for pc in (*TARGETS, NEGATIVE_PC)}
        self.first: list[tuple[int, int, int]] = []
        self.changes: list[tuple[int, int, int]] = []
        self.last_state: tuple[int, int] | None = None
        self.truncated = False

    def stop(self) -> bool:
        self.scanned += 1
        pc = number("pc")
        registers = gdb.parse_and_eval("lightrec_get_registers(state)").dereference()["gpr"]
        t9 = int(registers[25])
        t3 = int(registers[11])
        if pc in self.hits:
            self.hits[pc] += 1
        if len(self.first) < FIRST_ENTRIES:
            self.first.append((pc, t9, t3))
        state = (t9, t3)
        if state != self.last_state:
            self.changes.append((pc, t9, t3))
            self.last_state = state
        if self.scanned == MAX_BLOCK_ENTRIES:
            self.truncated = True
            self.enabled = False
        return False


observer: BlockObserver | None = None
before_t9 = 0
before_t3 = 0
before_blocks = 0
before_instructions = 0


def admit() -> None:
    global before_t9, before_t3, before_blocks, before_instructions
    gdb.execute("set $ctr_probe_ready = 0")
    if probe_mode == "product":
        if not bool(gdb.parse_and_eval("execution._M_payload._M_engaged")):
            print("CTR_PROBE_ADMISSION " + json.dumps({"status": "refused", "reason": "no execution result"}))
            return
        result = gdb.parse_and_eval("execution._M_payload._M_payload._M_value")
    else:
        result = gdb.parse_and_eval("first")
    reason = str(result["reason"])
    result_pc = int(result["guestPc"])
    core_pc = number("core.pc")
    before_t9 = number("core.r[25]")
    before_t3 = number("core.r[11]")
    before_blocks = number("core.lightrecExecutor().counters().executedBlocks")
    before_instructions = number("core.lightrecExecutor().counters().executedInstructions")
    scanned, matched = image_admission()
    accepted = (
        reason.endswith("::BudgetExhausted")
        and result_pc == core_pc == RESUME_PC
        and matched == 1
    )
    print(
        "CTR_PROBE_ADMISSION "
        + json.dumps(
            {
                "status": "accepted" if accepted else "refused",
                "reason": reason,
                "result_pc": result_pc,
                "core_pc": core_pc,
                "first_cycles": int(result["cycles"]),
                "image_entries_scanned": scanned,
                "published_bf0233_matches": matched,
                "t9": before_t9,
                "t3": before_t3,
            },
            sort_keys=True,
        )
    )
    if accepted:
        gdb.execute("set $ctr_probe_ready = 1")


def arm() -> None:
    global observer
    observer = BlockObserver()


def finish() -> None:
    if observer is None:
        raise RuntimeError("budget observer was never armed")
    observer.enabled = False
    result = gdb.history(0)
    after_t9 = number("core.r[25]")
    after_t3 = number("core.r[11]")
    after_blocks = number("core.lightrecExecutor().counters().executedBlocks")
    after_instructions = number("core.lightrecExecutor().counters().executedInstructions")
    for ordinal, (pc, t9, t3) in enumerate(observer.changes):
        print(f"CTR_PROBE_CHANGE ordinal={ordinal} pc=0x{pc:08X} t9=0x{t9:08X} t3=0x{t3:08X}")
    summary = {
        "status": "progress" if (after_t9, after_t3) != (before_t9, before_t3) or len(observer.changes) > 1 else "net-unchanged",
        "second_reason": str(result["reason"]),
        "second_pc": int(result["guestPc"]),
        "second_cycles": int(result["cycles"]),
        "block_entries_scanned": observer.scanned,
        "block_entries_observation_capped": observer.truncated,
        "executed_blocks_delta": after_blocks - before_blocks,
        "block_entries_unobserved_after_cap": max(0, after_blocks - before_blocks - observer.scanned),
        "executed_instructions_delta": after_instructions - before_instructions,
        "first_entries_retained": len(observer.first),
        "state_changes_retained": len(observer.changes),
        "target_hits": {f"0x{pc:08X}": observer.hits[pc] for pc in TARGETS},
        "negative_hits": observer.hits[NEGATIVE_PC],
        "before_t9": before_t9,
        "after_t9": after_t9,
        "before_t3": before_t3,
        "after_t3": after_t3,
    }
    for ordinal, (pc, t9, t3) in enumerate(observer.first):
        print(f"CTR_PROBE_FIRST ordinal={ordinal} pc=0x{pc:08X} t9=0x{t9:08X} t3=0x{t3:08X}")
    print("CTR_PROBE_RESULT " + json.dumps(summary, sort_keys=True))
