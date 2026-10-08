#include "scene_cut.h"

#include "core.h"
#include "native_ownership.h"

namespace ctr {

SceneIdentity SceneCut::read(Core &core) {
  const uint32_t tracker = core.mem_r32(core.r[28] + native::kGameStateGpOffset);
  SceneIdentity scene{.mainState = core.mem_r32(core.r[28] + native::kMainStateGpOffset)};
  // Before the guest allocates its tracker only the main state exists.
  if (tracker != 0u) {
    scene.levelId = core.mem_r32(tracker + native::kTrackerLevelIdOffset);
    scene.loading = (core.mem_r32(tracker + native::kTrackerGameModeOffset) & native::kGameModeLoading) != 0u;
  }
  return scene;
}

void SceneCut::observe(const SceneIdentity &scene) {
  cut_ = !sealed_.has_value() || *sealed_ != scene;
  sealed_ = scene;
}

bool SceneCut::isCut() const {
  return cut_;
}

} // namespace ctr
