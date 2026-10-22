#pragma once

#include "../evaluation/OccurrenceVerdict.h"
#include "../occurrences/InspectedOccurrence.h"
#include "InspectorTypes.h"

namespace detection {

/*
makeVerdict

The last step of inspection: judges one inspected occurrence against the
plan's support requirements (minimum StrengthClass per InspectionTarget) and
renders the OccurrenceVerdict handed to FieldState and Behavior. Valid when
every enabled requirement is met; otherwise the first failed one is named.

Returns false, and leaves `out` untouched, when the occurrence produces no
verdict: the inspector rejected it, it is absent or invalid, it is not a
scalar occurrence, or it is empty (no duration, no strength).

No temporal or multi-occurrence logic. Pattern matching over several
occurrences, if it comes, sits downstream of the verdict.

Replaces the OccurrenceEvaluator stage (roadmap step 8, cleanup-0-plan Phase 7c;
decisions/2026-09-21-fold-evaluator-into-inspector.md): same rule, same fields,
no queue.
*/
bool makeVerdict(const InspectedOccurrence& inspected, const InspectionPlan& plan, OccurrenceVerdict& out);

}  // namespace detection
