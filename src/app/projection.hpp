#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cypherarx/entry.hpp"

namespace cypherarx::gui {

enum class Axis { Service, Persona };

struct Projection {
    std::vector<std::int64_t> roots;
    std::unordered_map<std::int64_t, std::vector<std::int64_t>> children;
    std::unordered_map<std::int64_t, const Entry*> by_id;

    static Projection build(const std::vector<Entry>& entries, Axis axis);

    const Entry* find(std::int64_t id) const;
    std::string label_of(std::optional<std::int64_t> id) const;
    std::optional<std::int64_t> cross_parent(const Entry& e, Axis axis) const;
};

bool in_axis(const Entry& e, Axis axis);
std::optional<std::int64_t> tree_parent(const Entry& e, Axis axis);

} // namespace cypherarx::gui
