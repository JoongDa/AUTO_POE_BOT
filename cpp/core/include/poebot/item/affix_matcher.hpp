#pragma once
#include <regex>
#include <string>
#include <string_view>

namespace poebot::item {

// Wraps a user-supplied pipe-separated regex pattern (e.g.  "life|mana|resist")
// and matches it against item text copied from the clipboard.  The regex is
// compiled once at construction so the per-item cost is just the search.
class AffixMatcher {
public:
    AffixMatcher() = default;
    explicit AffixMatcher(std::string_view pattern);

    // True if the item text contains a substring matching the pattern.
    bool matches(std::string_view itemText) const;

    // True if the pattern compiled without error.
    bool valid() const noexcept { return valid_; }

private:
    std::regex regex_;
    bool       valid_ = false;
};

}  // namespace poebot::item
