#include "tbc/cec.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <filesystem>

namespace tbc {

void export_dot(const Manager& mgr,
                std::span<const NodeID> roots,
                std::span<const std::string> root_names,
                const std::string& filepath) {
    auto parent = std::filesystem::path(filepath).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream out(filepath);
    if (!out.is_open()) {
        throw std::runtime_error("Could not open file for DOT export: " + filepath);
    }

    out << "digraph ROBDD {\n";
    out << "  rankdir=TB;\n";
    out << "  node [fontname=\"Helvetica\", fontsize=10];\n";
    out << "  edge [fontname=\"Helvetica\", fontsize=9];\n\n";

    // Terminals
    out << "  node_0 [shape=box, label=\"0\", style=\"filled,bold\", fillcolor=\"#ffebee\", color=\"#c62828\"];\n";
    out << "  node_1 [shape=box, label=\"1\", style=\"filled,bold\", fillcolor=\"#e8f5e9\", color=\"#2e7d32\"];\n\n";

    std::map<VarID, std::vector<NodeID>> level_nodes;
    std::set<NodeID> visited;

    mgr.for_each_reachable(roots, [&](NodeID id, const BDDNode& node) {
        visited.insert(id);
        if (id > 1) {
            level_nodes[node.var_id].push_back(id);
        }
    });

    // Group nodes at the same variable level on the same rank
    for (const auto& [var_id, nodes] : level_nodes) {
        out << "  { rank=same; ";
        std::string vname(mgr.var_name(var_id));
        for (NodeID nid : nodes) {
            out << "node_" << nid << "; ";
        }
        out << "}\n";
    }
    out << "\n";

    // Print internal nodes
    for (const auto& [var_id, nodes] : level_nodes) {
        std::string vname(mgr.var_name(var_id));
        for (NodeID nid : nodes) {
            out << "  node_" << nid << " [shape=circle, style=\"filled\", fillcolor=\"#f5f5f5\", label=\""
                << vname << " (#" << nid << ")\"];\n";
        }
    }
    out << "\n";

    // Print edges
    for (NodeID nid : visited) {
        if (nid > 1) {
            const auto& n = mgr.node(nid);
            // Low edge (dashed red)
            out << "  node_" << nid << " -> node_" << n.low
                << " [style=dashed, color=\"#e53935\", label=\"0\"];\n";
            // High edge (solid blue)
            out << "  node_" << nid << " -> node_" << n.high
                << " [style=solid, color=\"#1e88e5\", label=\"1\"];\n";
        }
    }
    out << "\n";

    // Print root pointers
    for (std::size_t i = 0; i < roots.size() && i < root_names.size(); ++i) {
        std::string rlabel = root_names[i];
        out << "  root_" << i << " [shape=box, style=\"filled,rounded\", fillcolor=\"#e3f2fd\", color=\"#1565c0\", label=\""
            << rlabel << "\"];\n";
        out << "  root_" << i << " -> node_" << roots[i] << " [style=bold, color=\"#0d47a1\"];\n";
    }

    out << "}\n";
}

} // namespace tbc
