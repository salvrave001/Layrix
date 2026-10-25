#pragma once

#include <string>
#include <string_view>

namespace layrix {

enum class UiLanguage { Ru, En };

UiLanguage ParseUiLanguage(std::wstring_view text);
std::wstring_view UiLanguageCode(UiLanguage language);
UiLanguage CurrentUiLanguage();
void SetCurrentUiLanguage(UiLanguage language);

std::wstring Tr(std::wstring_view key);
std::wstring ReplaceText(std::wstring text, std::wstring_view token, std::wstring_view value);
std::wstring TrReplace(std::wstring_view key, std::wstring_view token, std::wstring_view value);
std::wstring BuiltinLayoutTitle(std::wstring_view id);

}  // namespace layrix
