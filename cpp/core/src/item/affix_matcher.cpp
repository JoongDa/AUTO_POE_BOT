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

std::vector<AffixMatch> AffixMatcher::findAll(std::string_view itemText) const {
    std::vector<AffixMatch> out;
    if (!valid_ || itemText.empty()) return out;

    using It = std::string_view::const_iterator;
    std::match_results<It> m;
    auto begin = itemText.begin();
    const auto end   = itemText.end();

    while (std::regex_search(begin, end, m, regex_)) {
        const auto matchBegin = m[0].first;
        const auto matchEnd   = m[0].second;
        const std::size_t offset = static_cast<std::size_t>(matchBegin - itemText.begin());
        const std::size_t length = static_cast<std::size_t>(matchEnd - matchBegin);
        if (length == 0) {
            // Zero-width match (e.g., pattern "a*"). Advance one byte to
            // avoid an infinite loop — this isn't a realistic affix pattern
            // but we guard against user typos anyway.
            if (matchBegin == end) break;
            begin = matchBegin + 1;
            continue;
        }
        out.push_back({offset, length});
        begin = matchEnd;
    }
    return out;
}

}  // namespace poebot::item
