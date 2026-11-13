#pragma once

// Configuration parsing is serialized by ControllerContext::mutex. Nested
// imports and controller variants must finish before touching the virtual bus.
namespace ProfileOutput {
inline thread_local unsigned depth = 0;
inline thread_local bool dirty = false;
// Studio transitions can apply hundreds of assignments. Repainting the hidden
// Windows console for each one holds the mapping lock against mouse input.
// Keep warnings/errors and the final transition summary, omit assignment echo.
inline thread_local unsigned quietDepth = 0;
// Studio manages the mapping session; keep an unused output neutral and ready
// until its physical controller/session closes instead of hotplugging per slot.
inline bool studioTransitions = false;
struct QuietAssignments {
    QuietAssignments() { ++quietDepth; }
    ~QuietAssignments() { --quietDepth; }
};
inline bool defer() {
    if (!depth) return false;
    dirty = true;
    return true;
}
class Transaction {
    void (*commit_)();
public:
    explicit Transaction(void (*commit)()) : commit_(commit) { ++depth; }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction() {
        if (--depth == 0 && dirty) {
            dirty = false;
            commit_();
        }
    }
};
}
