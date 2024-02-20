#include "pch.h"
#include "Helpers.h"

std::wstring Helpers::Normalize(std::wstring const& str, bool denormalize) {
    auto find = L'?', replace = L'!';
    if (denormalize)
        std::swap(find, replace);

    auto name(str);
    for (auto& ch : name)
        if (ch == find)
            ch = replace;
    return name;
}
