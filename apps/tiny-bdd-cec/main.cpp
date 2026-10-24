#include "tbc/bdd.hpp"
#include "tbc/netlist.hpp"
#include "tbc/verilog.hpp"
#include "tbc/blif.hpp"
#include "tbc/cec.hpp"
#include "tbc/netlist_to_bdd.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <unordered_set>

namespace {

tbc::Netlist load_circuit(const std::string& path, const std::string& top) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".blif") {
        return tbc::parse_blif_file(path);
    }
    return tbc::parse_verilog_file(path, top);
}

void print_help() {
    std::cout << R"(tiny-bdd-cec - Lightweight C++20 ROBDD Combinational Equivalence Checker

USAGE:
  tiny-bdd-cec --cec <circuit_a.[v|blif]> <circuit_b.[v|blif]> [OPTIONS]
  tiny-bdd-cec --help | -h
  tiny-bdd-cec --version | -v

OPTIONS:
  --top <name>            Specify top module name to elaborate (default: first module)
  --order <mode>          Variable ordering strategy:
                            interleave : (default) Bit-interleave input buses
                            decl       : Declaration order
                            file:<path>: Custom order file
  --dump-dot <file.dot>   Export shared ROBDD DAG to Graphviz DOT
  --dot-outputs <list>    Comma-separated list of outputs to include in DOT export
  --profile               Report node counts, unique table peak, cache hit rate & timing
  --max-nodes <N>         Maximum nodes safety cap (default: 50,000,000)
  --cache-log2 <K>        Computed cache size = 2^K entries (default: 18 = 262,144)
  --quiet                 Suppress per-output status lines

EXIT CODES:
  0 : Circuits are mathematically EQUIVALENT
  1 : Circuits are NOT EQUIVALENT (counterexample reported)
  2 : Usage error, parse error, or file not found
  3 : Node limit exceeded
)";
}

void print_version() {
    std::cout << "tiny-bdd-cec version 0.1.0 (C++20 ROBDD Engine)\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 2;
    }

    std::string file_a;
    std::string file_b;
    std::string top_name;
    std::string dot_file;
    std::string dot_outputs_str;
    tbc::CecOptions opts;
    bool do_profile = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            print_version();
            return 0;
        } else if (arg == "--cec") {
            if (i + 2 >= argc) {
                std::cerr << "Error: --cec requires two file arguments <circuit_a> <circuit_b>\n";
                return 2;
            }
            file_a = argv[++i];
            file_b = argv[++i];
        } else if (arg == "--top") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --top requires a module name\n";
                return 2;
            }
            top_name = argv[++i];
        } else if (arg == "--order") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --order requires a mode (interleave, decl, or file:<path>)\n";
                return 2;
            }
            std::string o = argv[++i];
            if (o == "interleave") {
                opts.order_mode = tbc::OrderMode::Interleave;
            } else if (o == "decl") {
                opts.order_mode = tbc::OrderMode::Decl;
            } else if (o.rfind("file:", 0) == 0) {
                opts.order_mode = tbc::OrderMode::Explicit;
                opts.order_file = o.substr(5);
            } else {
                std::cerr << "Error: Unknown order mode '" << o << "'\n";
                return 2;
            }
        } else if (arg == "--dump-dot") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --dump-dot requires a filename\n";
                return 2;
            }
            dot_file = argv[++i];
        } else if (arg == "--dot-outputs") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --dot-outputs requires a comma-separated output list\n";
                return 2;
            }
            dot_outputs_str = argv[++i];
        } else if (arg == "--profile") {
            do_profile = true;
        } else if (arg == "--quiet") {
            opts.quiet = true;
        } else if (arg == "--max-nodes") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --max-nodes requires an integer argument\n";
                return 2;
            }
            opts.max_nodes = std::stoull(argv[++i]);
        } else if (arg == "--cache-log2") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --cache-log2 requires an integer argument\n";
                return 2;
            }
            opts.cache_log2_size = std::stoul(argv[++i]);
        } else {
            // Positional fallback for circuits
            if (file_a.empty()) file_a = arg;
            else if (file_b.empty()) file_b = arg;
            else {
                std::cerr << "Error: Unexpected argument '" << arg << "'\n";
                return 2;
            }
        }
    }

    if (file_a.empty() || file_b.empty()) {
        std::cerr << "Error: Both Circuit A and Circuit B must be specified.\n";
        print_help();
        return 2;
    }

    try {
        if (!opts.quiet) {
            std::cout << "[tiny-bdd-cec] Combinational Equivalence Checking\n";
            std::cout << "  Circuit A: " << file_a << "\n";
            std::cout << "  Circuit B: " << file_b << "\n";
        }

        tbc::Netlist netlist_a = load_circuit(file_a, top_name);
        tbc::Netlist netlist_b = load_circuit(file_b, top_name);

        if (!opts.quiet) {
            std::cout << "  Parsed [A] module '" << netlist_a.module_name << "': "
                      << netlist_a.inputs.size() << " inputs, " << netlist_a.outputs.size() << " outputs, "
                      << netlist_a.gates.size() << " gates\n";
            std::cout << "  Parsed [B] module '" << netlist_b.module_name << "': "
                      << netlist_b.inputs.size() << " inputs, " << netlist_b.outputs.size() << " outputs, "
                      << netlist_b.gates.size() << " gates\n";
            std::cout << "  Variable order: ";
            if (opts.order_mode == tbc::OrderMode::Interleave) std::cout << "interleave (bus bit-interleaved)\n";
            else if (opts.order_mode == tbc::OrderMode::Decl) std::cout << "decl (declaration order)\n";
            else std::cout << "explicit (" << opts.order_file << ")\n";
            std::cout << "------------------------------------------------------------\n";
        }

        tbc::CecResult cec_res = tbc::run_cec(netlist_a, netlist_b, opts);

        std::size_t mismatches = 0;
        const tbc::OutputComparison* first_mismatch = nullptr;

        for (const auto& out_cmp : cec_res.outputs) {
            if (out_cmp.equivalent) {
                if (!opts.quiet) {
                    std::cout << "  " << std::left << std::setw(20) << out_cmp.bit_name << " : EQUIVALENT\n";
                }
            } else {
                ++mismatches;
                if (!first_mismatch) first_mismatch = &out_cmp;
                std::cout << "  " << std::left << std::setw(20) << out_cmp.bit_name << " : NOT EQUIVALENT";
                if (out_cmp.failing_vector_count > 0.0) {
                    std::cout << " (differs on " << static_cast<unsigned long long>(out_cmp.failing_vector_count) << " input vectors)";
                }
                std::cout << "\n";
            }
        }

        if (first_mismatch) {
            std::cout << "------------------------------------------------------------\n";
            std::cout << tbc::format_counterexample(*first_mismatch, netlist_a, netlist_b) << "\n";
        }

        std::cout << "============================================================\n";
        if (cec_res.equivalent) {
            std::cout << "RESULT: EQUIVALENT (All " << cec_res.outputs.size() << " primary outputs match canonical BDDs)\n";
        } else {
            std::cout << "RESULT: NOT EQUIVALENT (" << mismatches << " of " << cec_res.outputs.size() << " outputs differ)\n";
        }

        if (do_profile) {
            std::cout << "------------------------------------------------------------\n";
            std::cout << "[Profile Stats]\n";
            std::cout << "  Time Elapsed        : " << (static_cast<double>(cec_res.elapsed.count()) / 1000.0) << " ms\n";
            std::cout << "  Total BDD Nodes     : " << cec_res.bdd_stats.nodes_allocated << "\n";
            std::cout << "  Peak Unique Table   : " << cec_res.bdd_stats.unique_peak << "\n";
            std::cout << "  Unique Table Hits   : " << cec_res.bdd_stats.unique_hits << "\n";
            std::cout << "  ITE Operations      : " << cec_res.bdd_stats.ite_calls << "\n";
            std::cout << "  Cache Lookups       : " << cec_res.bdd_stats.cache_lookups << "\n";
            std::cout << "  Cache Hits          : " << cec_res.bdd_stats.cache_hits << "\n";
            std::cout << "  Cache Hit Rate      : " << std::fixed << std::setprecision(2) << cec_res.bdd_stats.cache_hit_rate() << "%\n";
        }

        // Export DOT if requested
        if (!dot_file.empty()) {
            // Build manager for dot dump
            tbc::Manager dot_mgr;
            std::unordered_map<std::string, tbc::VarID> dot_var_map;
            for (const auto& v : cec_res.var_order) {
                dot_var_map[v] = dot_mgr.new_var(v);
            }
            auto roots_a = tbc::netlist_to_bdd(dot_mgr, netlist_a, dot_var_map);
            auto roots_b = tbc::netlist_to_bdd(dot_mgr, netlist_b, dot_var_map);

            std::vector<tbc::NodeID> roots;
            std::vector<std::string> root_names;

            std::unordered_set<std::string> filter_set;
            if (!dot_outputs_str.empty()) {
                std::stringstream ss(dot_outputs_str);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    filter_set.insert(item);
                }
            }

            for (std::size_t i = 0; i < netlist_a.outputs.size(); ++i) {
                if (filter_set.empty() || filter_set.contains(netlist_a.outputs[i].bit_name)) {
                    roots.push_back(roots_a[i]);
                    root_names.push_back("A: " + netlist_a.outputs[i].bit_name);
                }
            }
            for (std::size_t i = 0; i < netlist_b.outputs.size(); ++i) {
                if (filter_set.empty() || filter_set.contains(netlist_b.outputs[i].bit_name)) {
                    roots.push_back(roots_b[i]);
                    root_names.push_back("B: " + netlist_b.outputs[i].bit_name);
                }
            }

            tbc::export_dot(dot_mgr, roots, root_names, dot_file);
            if (!opts.quiet) {
                std::cout << "Exported BDD DAG to: " << dot_file << " (" << roots.size() << " roots)\n";
            }
        }

        return cec_res.equivalent ? 0 : 1;
    } catch (const tbc::NodeLimitExceeded& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 3;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}
