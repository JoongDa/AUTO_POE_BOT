#pragma once
#include <string_view>

namespace poebot::i18n {

// Switch the active language. Accepts "en" and "zh"; any other input is
// treated as "en" (a safer fallback than silently ignoring).
void setLanguage(std::string_view lang);

// Currently active language — "en" or "zh".
std::string_view language();

// Look up a UI string by key. Always returns a valid, null-terminated
// string pointer with program-lifetime storage:
//   1. active language's table if the key exists there,
//   2. otherwise the English table,
//   3. otherwise the key itself (so a missing translation shows up
//      visibly in the UI during development instead of rendering blank).
const char* tr(const char* key);

}  // namespace poebot::i18n
