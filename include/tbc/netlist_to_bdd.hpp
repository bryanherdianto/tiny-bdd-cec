#pragma once

#include "tbc/bdd.hpp"
#include "tbc/netlist.hpp"
#include <unordered_map>
#include <vector>

namespace tbc {

// Compiles a Netlist into BDD roots within the given Manager.
// Var mapping must map each input bit_name to a VarID registered in mgr.
std::vector<NodeID> netlist_to_bdd(Manager& mgr,
                                   const Netlist& netlist,
                                   const std::unordered_map<std::string, VarID>& input_var_map);

} // namespace tbc
