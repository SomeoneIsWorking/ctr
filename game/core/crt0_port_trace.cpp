#include "core.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string_view>
#include <system_error>

void load_exe(const char *path, Core *core);
void main_dispatch(Core *core, uint32_t address);
int rec_func_index(uint32_t address);
void shard_set_override(uint32_t address, void (*overrideFunction)(Core *));

namespace {

constexpr std::size_t kExeHeaderSize = 0x800;
constexpr std::size_t kRamSize = 0x200000;
constexpr std::size_t kEntryOffset = 0x10;
constexpr std::size_t kLoadOffset = 0x18;
constexpr std::size_t kTextSizeOffset = 0x1c;

constexpr std::array<const char *, 32> kRegisterNames = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0",   "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
};

struct ExeLayout {
  uint32_t entry;
  uint32_t load;
  uint32_t textSize;
};

struct Boundary {
  std::array<uint32_t, 32> registers{};
  uint32_t pc = 0;
  uint32_t lo = 0;
  uint32_t hi = 0;
};

struct BoundaryReached final {};

Boundary *g_boundary = nullptr;

uint32_t readLe32(const std::array<uint8_t, kExeHeaderSize> &header, std::size_t offset) {
  return static_cast<uint32_t>(header[offset]) | (static_cast<uint32_t>(header[offset + 1]) << 8U) |
         (static_cast<uint32_t>(header[offset + 2]) << 16U) | (static_cast<uint32_t>(header[offset + 3]) << 24U);
}

bool readLayout(const char *path, ExeLayout &layout) {
  std::error_code error;
  const auto fileSize = std::filesystem::file_size(path, error);
  if (error || fileSize < kExeHeaderSize) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — %s has no complete PS-X EXE header.\n", path);
    return false;
  }

  std::ifstream input(path, std::ios::binary);
  std::array<uint8_t, kExeHeaderSize> header{};
  if (!input.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()))) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — could not read the complete header from %s.\n", path);
    return false;
  }
  constexpr std::array<uint8_t, 8> kMagic = {'P', 'S', '-', 'X', ' ', 'E', 'X', 'E'};
  if (!std::equal(kMagic.begin(), kMagic.end(), header.begin())) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — %s is not a PS-X EXE.\n", path);
    return false;
  }

  layout = {readLe32(header, kEntryOffset), readLe32(header, kLoadOffset), readLe32(header, kTextSizeOffset)};
  const uint64_t payloadSize = fileSize - kExeHeaderSize;
  const uint64_t ramOffset = layout.load & (kRamSize - 1U);
  if (layout.textSize > payloadSize) {
    std::fprintf(stderr,
                 "ctr_crt0_port_trace: REFUSING — header claims 0x%X text bytes but %s carries only 0x%llX.\n",
                 layout.textSize,
                 path,
                 static_cast<unsigned long long>(payloadSize));
    return false;
  }
  if (ramOffset + layout.textSize > kRamSize) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — executable text does not fit in PSX main RAM.\n");
    return false;
  }
  if (layout.entry < layout.load || layout.entry >= layout.load + layout.textSize) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — header entry is outside the mapped executable text.\n");
    return false;
  }
  return true;
}

bool parseAddress(std::string_view text, uint32_t &value) {
  if (text.starts_with("0x") || text.starts_with("0X")) {
    text.remove_prefix(2);
  }
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

void captureBoundary(Core *core) {
  if (g_boundary == nullptr) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — boundary override fired while capture was not armed.\n");
    std::abort();
  }
  std::copy(std::begin(core->r), std::end(core->r), g_boundary->registers.begin());
  g_boundary->pc = core->pc;
  g_boundary->lo = core->lo;
  g_boundary->hi = core->hi;
  throw BoundaryReached{};
}

void printBoundary(uint32_t target, const Boundary &boundary) {
  std::printf("# PORT-CAPTURED-CALL target=0x%08X ra=0x%08X\n", target, boundary.registers[31]);
  std::printf("# PORT-CALL-BOUNDARY-REGS pc=0x%08X\n", boundary.pc);
  for (std::size_t index = 1; index < boundary.registers.size(); ++index) {
    std::printf("# PORT-CALL-BOUNDARY-REG %s=0x%08X\n", kRegisterNames[index], boundary.registers[index]);
  }
  std::printf("# PORT-CALL-BOUNDARY-REG lo=0x%08X\n", boundary.lo);
  std::printf("# PORT-CALL-BOUNDARY-REG hi=0x%08X\n", boundary.hi);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 4 || std::string_view(argv[2]) != "--target") {
    std::fprintf(stderr, "usage: ctr_crt0_port_trace <PS-X EXE> --target 0xADDR\n");
    return 2;
  }

  ExeLayout layout{};
  uint32_t target = 0;
  if (!readLayout(argv[1], layout)) {
    return 2;
  }
  if (!parseAddress(argv[3], target) || target == 0) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — invalid nonzero hexadecimal call target.\n");
    return 2;
  }
  if (rec_func_index(layout.entry) < 0 || rec_func_index(target) < 0) {
    std::fprintf(stderr,
                 "ctr_crt0_port_trace: REFUSING — entry 0x%08X and observed target 0x%08X must both exist in the "
                 "shipping generated registry.\n",
                 layout.entry,
                 target);
    return 2;
  }

  auto core = std::make_unique<Core>();
  load_exe(argv[1], core.get());
  Boundary boundary{};
  g_boundary = &boundary;
  shard_set_override(target, captureBoundary);
  bool reached = false;
  try {
    main_dispatch(core.get(), layout.entry);
  } catch (const BoundaryReached &) {
    reached = true;
  }
  shard_set_override(target, nullptr);
  g_boundary = nullptr;

  if (!reached || boundary.pc != target) {
    std::fprintf(stderr,
                 "ctr_crt0_port_trace: REFUSING — generated execution did not reach the independently observed "
                 "first-call boundary.\n");
    return 2;
  }
  printBoundary(target, boundary);
  return 0;
}
