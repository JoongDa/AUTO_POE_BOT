#include <poebot/item/affix_matcher.hpp>

#include <spdlog/spdlog.h>

namespace poebot::item {

AffixMatcher::AffixMatcher(std::string_view pattern) {
    if (pattern.empty()) return;
    try {
        regex_ = std::regex(pattern.data(), pattern.size(),
                            std::regex::ECMAScript | std::regex::icase | std::regex::optimize);
        valid_ = true;
    } catch (const std::regex_error& e) {
        spdlog::warn("invalid affix regex '{}': {}", pattern, e.what());
    }
}

bool AffixMatcher::matches(std::string_view itemText) const {
    if (!valid_ || itemText.empty()) return false;
    return std::regex_search(itemText.begin(), itemText.end(), regex_);
}

}  // namespace poebot::item
