#include "ctr_catalog.h"

#include "ctr_runtime.h"
#include "executable_identity.h"
#include "native_ownership.h"
#include "single_title_catalog.h"

namespace ctr {
namespace {

constexpr psx::host::TitleIdentity kTitleIdentity{
    .displayName = "CTR: Crash Team Racing",
    .serial = "SCUS_944.26",
    .slug = "ctr",
    .fileSize = identity::kExecutableBytes,
    .sha256 = identity::kExecutableSha256,
    .entry = identity::kEntry,
    .globalPointer = identity::kGlobalPointer,
    .textAddress = identity::kTextAddress,
    .textSize = identity::kTextBytes,
    .stackAddress = identity::kStackAddress,
    .stackOffset = identity::kStackOffset,
    .discEnvVar = "PSXPORT_CTR_DISC",
    .discExecutablePath = identity::kDiscExecutablePath,
};

} // namespace

std::unique_ptr<psx::host::TitleCatalog> createCatalog() {
  return std::make_unique<psx::host::SingleTitleCatalog>(
      kTitleIdentity, "scratch/raw", std::make_unique<CtrRuntime>(native::kExecutableEntry));
}

} // namespace ctr
