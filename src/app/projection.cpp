#include "app/projection.hpp"

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

Projection Projection::build(const std::vector<Entry>& entries, Axis axis) {
    Projection p;
    for (const auto& e : entries) {
        p.by_id[e.id] = &e;
    }
    for (const auto& e : entries) {
        if (!in_axis(e, axis)) {
            continue;
        }
        const auto parent = tree_parent(e, axis);
        const Entry* parent_entry = parent ? p.find(*parent) : nullptr;
        if (parent_entry && in_axis(*parent_entry, axis)) {
            p.children[*parent].push_back(e.id);
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

} // namespace cypherarx::gui
