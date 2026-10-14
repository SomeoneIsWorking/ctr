#pragma once

#include "title_catalog.h"

namespace ctr {

inline constexpr psx::host::TitleIdentity kTitleIdentity{
    .displayName = "CTR: Crash Team Racing",
    .serial = "SCUS_944.26",
    .slug = "ctr",
    .discEnvVar = "PSXPORT_CTR_DISC",
    .discExecutablePath = "SCUS_944.26",
};
// Where the launcher provisions the executable.
inline constexpr const char *kProvisioningRoot = "scratch/raw";

} // namespace ctr
