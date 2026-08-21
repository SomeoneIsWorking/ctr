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
Boundary *g_preModel = nullptr;
Boundary *g_modeledReturn = nullptr;
bool g_modeledReturnReached = false;

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

bool parseResidentState(std::string_view text, std::array<uint32_t, 33> &state) {
  for (uint32_t &value : state) {
    const std::size_t separator = text.find(',');
    const std::string_view field = text.substr(0, separator);
    if (field.empty() || !parseAddress(field, value)) {
      return false;
    }
    if (separator == std::string_view::npos) {
      text = {};
    } else {
      text.remove_prefix(separator + 1);
    }
  }
  return text.empty();
}

void restoreResidentState(Core &core, const std::array<uint32_t, 33> &state, uint32_t resumeTarget) {
  core.r[0] = 0;
  std::copy_n(state.begin(), 31, std::begin(core.r) + 1);
  core.lo = state[31];
  core.hi = state[32];
  core.pc = resumeTarget;
}

void captureCore(Core *core, Boundary &boundary) {
  std::copy(std::begin(core->r), std::end(core->r), boundary.registers.begin());
  boundary.pc = core->pc;
  boundary.lo = core->lo;
  boundary.hi = core->hi;
}

void captureBoundary(Core *core) {
  if (g_boundary == nullptr) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — boundary override fired while capture was not armed.\n");
    std::abort();
  }
  captureCore(core, *g_boundary);
  throw BoundaryReached{};
}

void modelInitHeapReturn(Core *core) {
  if (g_preModel == nullptr || g_modeledReturn == nullptr || g_modeledReturnReached) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — InitHeap model fired outside its one-shot boundary.\n");
    std::abort();
  }

  captureCore(core, *g_preModel);

  // CTR's independently observed three-instruction thunk selects A(39h):
  //   addiu t2,zero,0xA0; jr t2; addiu t1,zero,0x39
  // The shipping framework's explicit leaf contract sets v0=0 and preserves v1. This consumer-owned
  // policy corresponds to oracle_trace's modeled return; it is not a claim that vector code executed.
  core->r[10] = 0xA0u;
  core->r[9] = 0x39u;
  core->r[2] = 0;
  core->pc = core->r[31];
  captureCore(core, *g_modeledReturn);
  g_modeledReturnReached = true;
}

void printRegisterBlock(const char *tag, const Boundary &boundary) {
  std::printf("# %s-REGS pc=0x%08X\n", tag, boundary.pc);
  for (std::size_t index = 1; index < boundary.registers.size(); ++index) {
    std::printf("# %s-REG %s=0x%08X\n", tag, kRegisterNames[index], boundary.registers[index]);
  }
  std::printf("# %s-REG lo=0x%08X\n", tag, boundary.lo);
  std::printf("# %s-REG hi=0x%08X\n", tag, boundary.hi);
}

void printTaggedBoundary(const char *captureTag, const char *registerTag, uint32_t target, const Boundary &boundary) {
  std::printf("# %s target=0x%08X ra=0x%08X\n", captureTag, target, boundary.registers[31]);
  printRegisterBlock(registerTag, boundary);
}

void printBoundary(uint32_t target, const Boundary &boundary) {
  printTaggedBoundary("PORT-CAPTURED-CALL", "PORT-CALL-BOUNDARY", target, boundary);
}

} // namespace

int main(int argc, char **argv) {
  const bool firstBoundaryOnly = argc == 4 && std::string_view(argv[2]) == "--target";
  const bool postInitHeap = argc == 7 && std::string_view(argv[2]) == "--target" &&
                            std::string_view(argv[4]) == "--model-init-heap-return" &&
                            std::string_view(argv[5]) == "--post-target";
  const bool residentReplay = argc == 8 && std::string_view(argv[2]) == "--resume-target" &&
                              std::string_view(argv[4]) == "--capture-target" && std::string_view(argv[6]) == "--state";
  if (!firstBoundaryOnly && !postInitHeap && !residentReplay) {
    std::fprintf(stderr,
                 "usage: ctr_crt0_port_trace <PS-X EXE> --target 0xADDR "
                 "[--model-init-heap-return --post-target 0xADDR]\n"
                 "       ctr_crt0_port_trace <PS-X EXE> --resume-target 0xADDR "
                 "--capture-target 0xADDR --state AT,...,RA,LO,HI\n");
    return 2;
  }

  ExeLayout layout{};
  uint32_t target = 0;
  uint32_t postTarget = 0;
  if (!readLayout(argv[1], layout)) {
    return 2;
  }
  if (!parseAddress(argv[3], target) || target == 0) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — invalid nonzero hexadecimal call target.\n");
    return 2;
  }
  if ((postInitHeap || residentReplay) &&
      (!parseAddress(residentReplay ? argv[5] : argv[6], postTarget) || postTarget == 0 || postTarget == target)) {
    std::fprintf(stderr, "ctr_crt0_port_trace: REFUSING — invalid distinct hexadecimal post-return target.\n");
    return 2;
  }
  std::array<uint32_t, 33> residentState{};
  if (residentReplay && !parseResidentState(argv[7], residentState)) {
    std::fprintf(stderr,
                 "ctr_crt0_port_trace: REFUSING — --state must contain exactly 33 hexadecimal "
                 "AT,...,RA,LO,HI values.\n");
    return 2;
  }
  if ((!residentReplay && rec_func_index(layout.entry) < 0) || rec_func_index(target) < 0 ||
      ((postInitHeap || residentReplay) && rec_func_index(postTarget) < 0)) {
    std::fprintf(stderr,
                 "ctr_crt0_port_trace: REFUSING — entry 0x%08X, observed target 0x%08X, and any post-return "
                 "target 0x%08X must exist in the shipping generated registry.\n",
                 layout.entry,
                 target,
                 postTarget);
    return 2;
  }

  auto core = std::make_unique<Core>();
  load_exe(argv[1], core.get());
  Boundary boundary{};
  Boundary preModel{};
  Boundary modeledReturn{};
  g_boundary = &boundary;
  if (residentReplay) {
    restoreResidentState(*core, residentState, target);
    shard_set_override(postTarget, captureBoundary);
  } else if (postInitHeap) {
    g_preModel = &preModel;
    g_modeledReturn = &modeledReturn;
    g_modeledReturnReached = false;
    shard_set_override(target, modelInitHeapReturn);
    shard_set_override(postTarget, captureBoundary);
  } else {
    shard_set_override(target, captureBoundary);
  }
  bool reached = false;
  try {
    main_dispatch(core.get(), residentReplay ? target : layout.entry);
  } catch (const BoundaryReached &) {
    reached = true;
  }
  shard_set_override(residentReplay ? postTarget : target, nullptr);
  if (postInitHeap) {
    shard_set_override(postTarget, nullptr);
  }
  g_boundary = nullptr;
  g_preModel = nullptr;
  g_modeledReturn = nullptr;

  const uint32_t expectedBoundary = (postInitHeap || residentReplay) ? postTarget : target;
  if (!reached || boundary.pc != expectedBoundary || (postInitHeap && !g_modeledReturnReached)) {
    std::fprintf(stderr,
                 "ctr_crt0_port_trace: REFUSING — generated execution did not reach the independently observed "
                 "%s boundary.\n",
                 postInitHeap ? "modeled return and subsequent call"
                              : (residentReplay ? "resident subsequent call" : "first-call"));
    return 2;
  }
  if (firstBoundaryOnly || residentReplay) {
    printBoundary(expectedBoundary, boundary);
  } else {
    printBoundary(target, preModel);
    std::printf("# PORT-MODELED-BIOS-RETURN table=A function=0x39 target=0x000000A0 ra=0x%08X "
                "v0=0x00000000 v1=0x%08X\n",
                modeledReturn.registers[31],
                modeledReturn.registers[3]);
    printRegisterBlock("PORT-MODELED-RETURN", modeledReturn);
    printTaggedBoundary("PORT-POST-RETURN-CAPTURED-CALL", "PORT-POST-RETURN-CALL-BOUNDARY", postTarget, boundary);
  }
  return 0;
}
