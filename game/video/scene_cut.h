#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace ctr {

// What the guest says scene it is drawing: main-loop state, level id and the loading bit.
struct SceneIdentity {
  uint32_t mainState = 0;
  uint32_t levelId = 0;
  bool loading = false;

  bool operator==(const SceneIdentity &) const = default;
};

// Declares a cut when the scene identity changes between two committed fields.
class SceneCut final {
public:
  [[nodiscard]] static SceneIdentity read(Core &core);

  void observe(const SceneIdentity &scene);
  [[nodiscard]] bool isCut() const;

private:
  std::optional<SceneIdentity> sealed_;
  bool cut_ = false;
};

} // namespace ctr
