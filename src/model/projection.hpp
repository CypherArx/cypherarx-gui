#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cypherarx/entry.hpp"

namespace cypherarx::gui {

enum class Axis { Service, Persona };

struct Filter {
    std::string query;
    std::optional<std::int64_t> category;
    std::optional<std::int64_t> tag;
    bool favorites_only = false;

    bool active() const { return !query.empty() || category || tag || favorites_only; }
    void clear() { *this = Filter{}; }
};

using EntryPredicate = std::function<bool(const Entry&)>;

struct Projection {
    std::vector<std::int64_t> roots;
    std::unordered_map<std::int64_t, std::vector<std::int64_t>> children;
    std::unordered_map<std::int64_t, const Entry*> by_id;
    std::unordered_set<std::int64_t> matched;
    std::unordered_map<std::int64_t, std::string> names;
    bool filtered = false;

    static Projection build(const std::vector<Entry>& entries, Axis axis,
                            const EntryPredicate& match = {},
                            std::unordered_map<std::int64_t, std::string> names = {});

    const Entry* find(std::int64_t id) const;
    std::string label_of(std::optional<std::int64_t> id) const;
    std::optional<std::int64_t> cross_parent(const Entry& e, Axis axis) const;
    bool is_match(std::int64_t id) const { return !filtered || matched.contains(id); }
    std::size_t visible_count() const { return filtered ? matched.size() : by_id.size(); }
};

bool in_axis(const Entry& e, Axis axis);
std::optional<std::int64_t> tree_parent(const Entry& e, Axis axis);

bool contains_ci(std::string_view haystack, std::string_view needle);

std::vector<std::pair<std::int64_t, std::string>>
flatten_categories(const std::vector<Category>& categories, const char* indent);

std::unordered_set<std::int64_t> category_subtree(const std::vector<Category>& categories,
                                                  std::int64_t root);

} // namespace cypherarx::gui
