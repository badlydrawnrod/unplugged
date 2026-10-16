// Compile-time boundary checks through the supported document target.
#include <concepts>
#include <type_traits>

#include "document/document.h"
#include "document/document_view.h"
#include "document/logical_line_range.h"
#include "document/wrapped_row_range.h"

#if defined(DBC_PRE) || defined(DBC_ASSERT) || defined(DBC_INVARIANT) || \
    defined(DBC_POST) || defined(DBC_GUARD_CLASS_INVARIANTS)
#error "Document API headers must not expose contract-check machinery"
#endif

// Storage declarations are transitively visible for private inline members,
// but must stay in their owning namespace without global aliases.
struct GapBuffer {};
struct LineStarts {};

static_assert(
    std::same_as<Document::LineNumber, unplugged::document::LineNumber>);
static_assert(std::same_as<LogicalLineRange::LineNumber,
                           unplugged::document::LineNumber>);
static_assert(std::is_copy_constructible_v<Document>);
static_assert(std::is_copy_assignable_v<Document>);
static_assert(std::is_nothrow_move_constructible_v<Document>);
static_assert(std::is_nothrow_move_assignable_v<Document>);
