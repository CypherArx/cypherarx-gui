#include "model/projection.hpp"

#include <cctype>

namespace cypherarx::gui {

bool in_axis(const Entry& e, Axis axis) {
    const char* hidden_root = axis == Axis::Persona ? "service" : "persona";
    return e.entry_type != hidden_root;
}

std::optional<std::int64_t> tree_parent(const Entry& e, Axis axis) {
    if (e.entry_type == "account") {
        return axis == Axis::Persona ? e.persona_id : e.parent_id;
    }
    if (e.entry_type == "child") {
        return e.parent_id;
    }
    return std::nullopt;
}

bool contains_ci(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > haystack.size()) {
        return false;
    }
    auto lower = [](unsigned char c) { return c < 0x80 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c); };
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool eq = true;
        for (std::size_t j = 0; j < needle.size() && eq; ++j) {
            eq = lower(static_cast<unsigned char>(haystack[i + j])) ==
                 lower(static_cast<unsigned char>(needle[j]));
        }
        if (eq) {
            return true;
        }
    }
    return false;
}

Projection Projection::build(const std::vector<Entry>& entries, Axis axis,
                             const EntryPredicate& match) {
    Projection p;
    for (const auto& e : entries) {
        p.by_id[e.id] = &e;
    }

    auto visible_parent = [&](const Entry& e) -> const Entry* {
        const auto parent = tree_parent(e, axis);
        const Entry* pe = parent ? p.find(*parent) : nullptr;
        return pe && in_axis(*pe, axis) ? pe : nullptr;
    };

    std::unordered_set<std::int64_t> visible;
    if (match) {
        p.filtered = true;
        for (const auto& e : entries) {
            if (in_axis(e, axis) && match(e)) {
                p.matched.insert(e.id);
            }
        }
        for (const auto id : p.matched) {
            for (const Entry* cur = p.find(id); cur; cur = visible_parent(*cur)) {
                visible.insert(cur->id);
            }
        }
        bool grew = true;
        while (grew) {
            grew = false;
            for (const auto& e : entries) {
                if (visible.contains(e.id) || !in_axis(e, axis)) {
                    continue;
                }
                const Entry* pe = visible_parent(e);
                if (pe && visible.contains(pe->id) && p.matched.contains(pe->id)) {
                    visible.insert(e.id);
                    p.matched.insert(e.id);
                    grew = true;
                }
            }
        }
    }

    for (const auto& e : entries) {
        if (!in_axis(e, axis) || (p.filtered && !visible.contains(e.id))) {
            continue;
        }
        const Entry* pe = visible_parent(e);
        if (pe && (!p.filtered || visible.contains(pe->id))) {
            p.children[pe->id].push_back(e.id);
        } else {
            p.roots.push_back(e.id);
        }
    }
    return p;
}

const Entry* Projection::find(std::int64_t id) const {
    const auto it = by_id.find(id);
    return it == by_id.end() ? nullptr : it->second;
}

std::string Projection::label_of(std::optional<std::int64_t> id) const {
    if (!id) {
        return {};
    }
    const Entry* e = find(*id);
    return e ? e->label : std::string("#") + std::to_string(*id);
}

std::optional<std::int64_t> Projection::cross_parent(const Entry& e, Axis axis) const {
    if (e.entry_type != "account") {
        return std::nullopt;
    }
    return axis == Axis::Persona ? e.parent_id : e.persona_id;
}

namespace {

void flatten_into(const std::vector<Category>& all, std::optional<std::int64_t> parent, int depth,
                  const char* indent, std::vector<std::pair<std::int64_t, std::string>>& out) {
    for (const auto& c : all) {
        if (c.parent_id != parent) {
            continue;
        }
        std::string name;
        for (int i = 0; i < depth; ++i) {
            name += indent;
        }
        name += c.name;
        out.emplace_back(c.id, std::move(name));
        if (depth < 32) {
            flatten_into(all, c.id, depth + 1, indent, out);
        }
    }
}

} // namespace

std::vector<std::pair<std::int64_t, std::string>>
flatten_categories(const std::vector<Category>& categories, const char* indent) {
    std::vector<std::pair<std::int64_t, std::string>> out;
    flatten_into(categories, std::nullopt, 0, indent, out);
    for (const auto& c : categories) {
        bool present = false;
        for (const auto& [id, _] : out) {
            present = present || id == c.id;
        }
        if (!present) {
            out.emplace_back(c.id, c.name);
        }
    }
    return out;
}

std::unordered_set<std::int64_t> category_subtree(const std::vector<Category>& categories,
                                                  std::int64_t root) {
    std::unordered_set<std::int64_t> out{root};
    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto& c : categories) {
            if (!out.contains(c.id) && c.parent_id && out.contains(*c.parent_id)) {
                out.insert(c.id);
                grew = true;
            }
        }
    }
    return out;
}

} // namespace cypherarx::gui
