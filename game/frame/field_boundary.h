#pragma once

#include "presentation_owner.h"
#include "render_list_boundary_diagnostic.h"

#include <cstdint>

class Core;

namespace ctr {

// When one CTR field ends, and how an ended field is accounted.
//
// A field ends for exactly one reason: the guest asked to leave, through the typed psxport executor
// exit or through a title owner that knows the field is finished without running guest code again.
// This owner owns both halves so they cannot disagree — `pending()` reads the title's own request flag
// OR the framework's typed exit, and `finishField()` consumes whichever arrived, finishes the field's
// presentation fence, and counts the field.
class FieldBoundary final {
public:
  FieldBoundary(RenderListBoundaryDiagnostic &renderListDiagnostic, PresentationOwner &presentation);

  // Clears the title's own request. Called once at the top of every field, BEFORE any owner can
  // raise a new request, so a request can never be inherited by the next field.
  void beginField();

  // The title knows the field is finished. Records that and asks the executor to stop at the next
  // safe boundary. Never throws and never unwinds through JIT frames.
  void request(Core &core);

  // True when the field is finished, by either route. A typed exit that is NOT a frame boundary is
  // a defect in whichever owner raised it, so it is refused here rather than silently treated as a
  // completed field.
  [[nodiscard]] bool pending(Core &core) const;

  // Commits one finished field: consume the typed exit, run the debug render-list field teardown and
  // the presentation fence, and count the field. Called only from a validated boundary.
  void finishField(Core &core, uint32_t field);

  [[nodiscard]] uint32_t completedFields() const;

private:
  RenderListBoundaryDiagnostic &renderListDiagnostic_;
  PresentationOwner &presentation_;
  uint32_t completedFields_ = 0;
  bool requested_ = false;
};

} // namespace ctr
