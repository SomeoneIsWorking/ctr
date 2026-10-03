#include "field_boundary.h"

#include "core.h"
#include "execution_control.h"
#include "execution_exit.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ctr {

FieldBoundary::FieldBoundary(RenderListBoundaryDiagnostic &renderListDiagnostic, PresentationOwner &presentation)
    : renderListDiagnostic_(renderListDiagnostic), presentation_(presentation) {}

void FieldBoundary::beginField() {
  requested_ = false;
}

void FieldBoundary::request(Core &core) {
  requested_ = true;
  psx::cpu::requestExecutionExit(core, psx::cpu::ExecutionExitReason::FrameBoundary);
}

bool FieldBoundary::pending(Core &core) const {
  const auto &typed = core.executionControl().pending();
  if (typed && typed->reason != psx::cpu::ExecutionExitReason::FrameBoundary) {
    lucent::error("ctr-frame", "unexpected pending {} at field completion", psx::cpu::executionExitName(typed->reason));
    std::abort();
  }
  return requested_ || typed.has_value();
}

void FieldBoundary::finishField(Core &core, uint32_t field) {
  (void)core.executionControl().consume();
  renderListDiagnostic_.finishField(core, field);
  presentation_.finishField(core);
  ++completedFields_;
}

uint32_t FieldBoundary::completedFields() const {
  return completedFields_;
}

} // namespace ctr
