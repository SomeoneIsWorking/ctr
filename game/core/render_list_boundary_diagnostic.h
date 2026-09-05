#pragma once

#include <array>
#include <cstdint>
#include <functional>

class Core;

namespace ctr {

struct RenderListNodeChain {
  uint32_t head = 0;
  uint32_t tail = 0;
  uint32_t tailLink = 0;
  uint32_t nodes = 0;
  bool terminated = false;
  bool cycle = false;
  bool invalidPointer = false;
  bool limitReached = false;
};

struct RenderListFirstWriter {
  uint32_t address = 0;
  uint32_t value = 0;
  uint32_t width = 0;
  uint32_t pc = 0;
  uint32_t returnAddress = 0;
  bool seen = false;
};

struct RenderListObservation {
  uint32_t gameState = 0;
  uint32_t list = 0;
  uint32_t pairReturn = 0;
  uint32_t pairDescriptor = 0;
  std::array<RenderListNodeChain, 3> sourceLists{};
  uint32_t pairStores = 0;
  RenderListFirstWriter firstWriter{};
  bool validGameState = false;
  bool validList = false;
  bool pairWatchArmed = false;
  bool pairWatchBusy = false;
  bool pairWatchConfiguredElsewhere = false;
};

// Debug-only observation of CTR's 0x8003B43C list-publication boundary. It never supplies list
// contents or changes control flow: the JIT-executed retail publisher remains the behavior body, and this
// component only reads its post-body memory and arms Core's existing narrow write observer on the
// two-word consumer pair for the rest of the current field.
class RenderListBoundaryDiagnostic final {
public:
  using RetailBody = std::function<void(Core &)>;

  explicit RenderListBoundaryDiagnostic(bool enabled = false);

  [[nodiscard]] bool enabled() const;
  [[nodiscard]] const RenderListObservation &latest() const;

  void observePublication(Core &core, const RetailBody &retailBody);
  void finishField(Core &core, uint32_t field);

private:
  static constexpr uint32_t kSourceListOffset = 0x1920u;
  static constexpr uint32_t kPublishedListOffset = 0x1C94u;
  static constexpr uint32_t kSourceListStride = 0x28u;
  static constexpr uint32_t kNodeNextOffset = 0u;
  static constexpr uint32_t kNodeListLinkOffset = 8u;
  static constexpr uint32_t kPairBytes = 8u;
  static constexpr uint32_t kNodeLimit = 128u;

  static void recordPairStore(Core *core, uint32_t address, uint32_t value, uint32_t width);

  [[nodiscard]] static bool readableRam(uint32_t address, uint32_t bytes);
  [[nodiscard]] static RenderListNodeChain scanNodeChain(Core &core, uint32_t head);
  void armPairWatch(Core &core);
  void disarmPairWatch(Core &core);
  void report(uint32_t field, const char *reason) const;

  static RenderListBoundaryDiagnostic *active_;

  bool enabled_ = false;
  bool reportedNoPublication_ = false;
  bool publishedThisField_ = false;
  Core *watchCore_ = nullptr;
  RenderListObservation latest_{};
};

} // namespace ctr
