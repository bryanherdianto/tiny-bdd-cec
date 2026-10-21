#include "verilog_parser.hpp"
#include "tbc/verilog.hpp"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cmath>

namespace tbc {

namespace {

struct WireMeta {
    std::string base_name;
    bool is_bus = false;
    int msb = 0;
    int lsb = 0;
    PortDir dir = PortDir::Wire;
};

// Forward declaration of elaborator helper class
class Elaborator {
public:
    Elaborator(const ModuleAst& mod, Netlist& nl) : mod_(mod), nl_(nl) {}

    void elaborate();

private:
    const ModuleAst& mod_;
    Netlist& nl_;

    std::unordered_map<std::string, WireMeta> wire_decls_;
    std::unordered_map<std::string, SignalID> wire_signals_;

    // Pending assignments / gate drivers
    struct Driver {
        std::vector<std::string> lhs_bits; // LSB first
        ExprPtr rhs_expr;
        GateKind gate_kind = GateKind::Buf;
        bool is_gate = false;
        std::vector<ExprPtr> gate_args;
    };
    std::vector<Driver> drivers_;
    std::unordered_map<std::string, std::size_t> bit_to_driver_;
    std::unordered_set<std::string> evaluating_bits_;

    SignalID resolve_bit(const std::string& bit_name);
    std::vector<SignalID> eval_expr(const ExprPtr& expr, int context_width = 0);
    std::vector<std::string> flatten_lhs(const ExprPtr& lhs);
    int get_expr_width(const ExprPtr& expr);

    SignalID get_const(bool val) {
        return nl_.add_const(val);
    }

    SignalID or_reduce(const std::vector<SignalID>& bits) {
        if (bits.empty()) return get_const(false);
        SignalID acc = bits[0];
        for (std::size_t i = 1; i < bits.size(); ++i) {
            acc = nl_.add_gate(GateKind::Or, {acc, bits[i]});
        }
        return acc;
    }

    SignalID and_reduce(const std::vector<SignalID>& bits) {
        if (bits.empty()) return get_const(true);
        SignalID acc = bits[0];
        for (std::size_t i = 1; i < bits.size(); ++i) {
            acc = nl_.add_gate(GateKind::And, {acc, bits[i]});
        }
        return acc;
    }

    SignalID xor_reduce(const std::vector<SignalID>& bits) {
        if (bits.empty()) return get_const(false);
        SignalID acc = bits[0];
        for (std::size_t i = 1; i < bits.size(); ++i) {
            acc = nl_.add_gate(GateKind::Xor, {acc, bits[i]});
        }
        return acc;
    }
};

int Elaborator::get_expr_width(const ExprPtr& e) {
    if (!e) return 0;
    switch (e->kind) {
        case ExprKind::Literal:
            return e->width;
        case ExprKind::BitSelect:
            return 1;
        case ExprKind::PartSelect:
            return std::abs(e->msb - e->lsb) + 1;
        case ExprKind::Var: {
            auto it = wire_decls_.find(e->name);
            if (it != wire_decls_.end()) {
                return it->second.is_bus ? (std::abs(it->second.msb - it->second.lsb) + 1) : 1;
            }
            return 1;
        }
        case ExprKind::Unary:
            if (e->op == TokType::Excl || e->op == TokType::Amp || e->op == TokType::Pipe || e->op == TokType::Caret) {
                return 1; // reduction or boolean not
            }
            return get_expr_width(e->lhs);
        case ExprKind::Binary:
            if (e->op == TokType::AmpAmp || e->op == TokType::PipePipe || e->op == TokType::EqEq || e->op == TokType::ExclEq) {
                return 1;
            }
            return std::max(get_expr_width(e->lhs), get_expr_width(e->rhs));
        case ExprKind::Ternary:
            return std::max(get_expr_width(e->lhs), get_expr_width(e->rhs));
        case ExprKind::Concat: {
            int w = 0;
            for (const auto& elem : e->elements) w += get_expr_width(elem);
            return w;
        }
        case ExprKind::Replicate: {
            int w = 0;
            for (const auto& elem : e->elements) w += get_expr_width(elem);
            return w * e->repeat_count;
        }
    }
    return 1;
}

std::vector<std::string> Elaborator::flatten_lhs(const ExprPtr& lhs) {
    std::vector<std::string> bits;
    if (lhs->kind == ExprKind::Var) {
        auto it = wire_decls_.find(lhs->name);
        if (it != wire_decls_.end() && it->second.is_bus) {
            int low = std::min(it->second.msb, it->second.lsb);
            int high = std::max(it->second.msb, it->second.lsb);
            for (int i = low; i <= high; ++i) {
                bits.push_back(lhs->name + "[" + std::to_string(i) + "]");
            }
        } else {
            bits.push_back(lhs->name);
        }
    } else if (lhs->kind == ExprKind::BitSelect) {
        bits.push_back(lhs->name + "[" + std::to_string(lhs->index) + "]");
    } else if (lhs->kind == ExprKind::PartSelect) {
        int low = std::min(lhs->msb, lhs->lsb);
        int high = std::max(lhs->msb, lhs->lsb);
        for (int i = low; i <= high; ++i) {
            bits.push_back(lhs->name + "[" + std::to_string(i) + "]");
        }
    } else if (lhs->kind == ExprKind::Concat) {
        // Concatenation on LHS: {cout, sum}
        // Elements are listed MSB chunk to LSB chunk.
        // We want bits ordered LSB first, so we reverse the chunks.
        for (int i = static_cast<int>(lhs->elements.size()) - 1; i >= 0; --i) {
            auto chunk_bits = flatten_lhs(lhs->elements[static_cast<std::size_t>(i)]);
            bits.insert(bits.end(), chunk_bits.begin(), chunk_bits.end());
        }
    } else {
        throw VerilogParseError("Invalid expression on left-hand side of assignment", lhs->line, lhs->col);
    }
    return bits;
}

SignalID Elaborator::resolve_bit(const std::string& bit_name) {
    auto it = wire_signals_.find(bit_name);
    if (it != wire_signals_.end()) {
        return it->second;
    }

    if (evaluating_bits_.contains(bit_name)) {
        throw VerilogParseError("Combinational loop detected involving wire: " + bit_name);
    }
    evaluating_bits_.insert(bit_name);

    auto d_it = bit_to_driver_.find(bit_name);
    if (d_it == bit_to_driver_.end()) {
        // Unconnected net, tie to 0
        SignalID zero = get_const(false);
        wire_signals_[bit_name] = zero;
        evaluating_bits_.erase(bit_name);
        return zero;
    }

    const Driver& driver = drivers_[d_it->second];
    if (driver.is_gate) {
        // Gate driver: args[0] is output, args[1..] are inputs
        std::vector<SignalID> fanin_sigs;
        for (std::size_t a = 1; a < driver.gate_args.size(); ++a) {
            auto in_bits = eval_expr(driver.gate_args[a], 1);
            if (in_bits.empty()) fanin_sigs.push_back(get_const(false));
            else fanin_sigs.push_back(in_bits[0]);
        }
        SignalID out_sig = nl_.add_gate(driver.gate_kind, std::move(fanin_sigs), bit_name);
        wire_signals_[bit_name] = out_sig;
    } else {
        // Continuous assign driver
        int target_width = static_cast<int>(driver.lhs_bits.size());
        auto rhs_bits = eval_expr(driver.rhs_expr, target_width);
        for (std::size_t i = 0; i < driver.lhs_bits.size(); ++i) {
            SignalID s = (i < rhs_bits.size()) ? rhs_bits[i] : get_const(false);
            wire_signals_[driver.lhs_bits[i]] = s;
        }
    }

    evaluating_bits_.erase(bit_name);
    return wire_signals_.at(bit_name);
}

std::vector<SignalID> Elaborator::eval_expr(const ExprPtr& e, int context_width) {
    if (!e) return {get_const(false)};

    int natural_width = get_expr_width(e);
    int effective_width = std::max(natural_width, context_width);

    std::vector<SignalID> res;

    switch (e->kind) {
        case ExprKind::Literal: {
            for (std::size_t i = 0; i < e->bits.size(); ++i) {
                res.push_back(get_const(e->bits[i]));
            }
            break;
        }
        case ExprKind::Var: {
            auto it = wire_decls_.find(e->name);
            if (it != wire_decls_.end() && it->second.is_bus) {
                int low = std::min(it->second.msb, it->second.lsb);
                int high = std::max(it->second.msb, it->second.lsb);
                for (int i = low; i <= high; ++i) {
                    res.push_back(resolve_bit(e->name + "[" + std::to_string(i) + "]"));
                }
            } else {
                res.push_back(resolve_bit(e->name));
            }
            break;
        }
        case ExprKind::BitSelect: {
            res.push_back(resolve_bit(e->name + "[" + std::to_string(e->index) + "]"));
            break;
        }
        case ExprKind::PartSelect: {
            int low = std::min(e->msb, e->lsb);
            int high = std::max(e->msb, e->lsb);
            for (int i = low; i <= high; ++i) {
                res.push_back(resolve_bit(e->name + "[" + std::to_string(i) + "]"));
            }
            break;
        }
        case ExprKind::Unary: {
            if (e->op == TokType::Tilde) {
                auto in_bits = eval_expr(e->lhs, effective_width);
                for (SignalID b : in_bits) {
                    res.push_back(nl_.add_gate(GateKind::Not, {b}));
                }
            } else if (e->op == TokType::Excl) {
                auto in_bits = eval_expr(e->lhs);
                SignalID any_one = or_reduce(in_bits);
                res.push_back(nl_.add_gate(GateKind::Not, {any_one}));
            } else if (e->op == TokType::Amp) {
                auto in_bits = eval_expr(e->lhs);
                res.push_back(and_reduce(in_bits));
            } else if (e->op == TokType::Pipe) {
                auto in_bits = eval_expr(e->lhs);
                res.push_back(or_reduce(in_bits));
            } else if (e->op == TokType::Caret) {
                auto in_bits = eval_expr(e->lhs);
                res.push_back(xor_reduce(in_bits));
            }
            break;
        }
        case ExprKind::Binary: {
            if (e->op == TokType::AmpAmp) {
                SignalID l = or_reduce(eval_expr(e->lhs));
                SignalID r = or_reduce(eval_expr(e->rhs));
                res.push_back(nl_.add_gate(GateKind::And, {l, r}));
            } else if (e->op == TokType::PipePipe) {
                SignalID l = or_reduce(eval_expr(e->lhs));
                SignalID r = or_reduce(eval_expr(e->rhs));
                res.push_back(nl_.add_gate(GateKind::Or, {l, r}));
            } else if (e->op == TokType::EqEq || e->op == TokType::ExclEq) {
                int cmp_width = std::max(get_expr_width(e->lhs), get_expr_width(e->rhs));
                auto l_bits = eval_expr(e->lhs, cmp_width);
                auto r_bits = eval_expr(e->rhs, cmp_width);
                std::vector<SignalID> eq_bits;
                for (std::size_t i = 0; i < l_bits.size(); ++i) {
                    eq_bits.push_back(nl_.add_gate(GateKind::Xnor, {l_bits[i], r_bits[i]}));
                }
                SignalID all_eq = and_reduce(eq_bits);
                if (e->op == TokType::ExclEq) {
                    res.push_back(nl_.add_gate(GateKind::Not, {all_eq}));
                } else {
                    res.push_back(all_eq);
                }
            } else if (e->op == TokType::Plus || e->op == TokType::Minus) {
                int add_width = effective_width;
                auto l_bits = eval_expr(e->lhs, add_width);
                auto r_bits = eval_expr(e->rhs, add_width);

                SignalID carry = (e->op == TokType::Minus) ? get_const(true) : get_const(false);
                for (int i = 0; i < add_width; ++i) {
                    SignalID a = (i < static_cast<int>(l_bits.size())) ? l_bits[static_cast<std::size_t>(i)] : get_const(false);
                    SignalID b = (i < static_cast<int>(r_bits.size())) ? r_bits[static_cast<std::size_t>(i)] : get_const(false);

                    if (e->op == TokType::Minus) {
                        b = nl_.add_gate(GateKind::Not, {b}); // Two's complement ~b + 1
                    }

                    // S_i = A_i ^ B_i ^ C_i
                    SignalID axorb = nl_.add_gate(GateKind::Xor, {a, b});
                    SignalID sum_bit = nl_.add_gate(GateKind::Xor, {axorb, carry});
                    res.push_back(sum_bit);

                    if (i + 1 < add_width || context_width > add_width) {
                        // C_{i+1} = (A_i & B_i) | (carry & (A_i ^ B_i))
                        SignalID aandb = nl_.add_gate(GateKind::And, {a, b});
                        SignalID candx = nl_.add_gate(GateKind::And, {carry, axorb});
                        carry = nl_.add_gate(GateKind::Or, {aandb, candx});
                    }
                }
            } else if (e->op == TokType::Amp || e->op == TokType::Pipe || e->op == TokType::Caret ||
                       e->op == TokType::TildeCaret || e->op == TokType::CaretTilde) {
                int bit_width = effective_width;
                auto l_bits = eval_expr(e->lhs, bit_width);
                auto r_bits = eval_expr(e->rhs, bit_width);

                GateKind gk = GateKind::And;
                if (e->op == TokType::Pipe) gk = GateKind::Or;
                else if (e->op == TokType::Caret) gk = GateKind::Xor;
                else if (e->op == TokType::TildeCaret || e->op == TokType::CaretTilde) gk = GateKind::Xnor;

                for (int i = 0; i < bit_width; ++i) {
                    SignalID a = (i < static_cast<int>(l_bits.size())) ? l_bits[static_cast<std::size_t>(i)] : get_const(false);
                    SignalID b = (i < static_cast<int>(r_bits.size())) ? r_bits[static_cast<std::size_t>(i)] : get_const(false);
                    res.push_back(nl_.add_gate(gk, {a, b}));
                }
            }
            break;
        }
        case ExprKind::Ternary: {
            SignalID c = or_reduce(eval_expr(e->cond));
            int mux_width = effective_width;
            auto t_bits = eval_expr(e->lhs, mux_width);
            auto f_bits = eval_expr(e->rhs, mux_width);
            for (int i = 0; i < mux_width; ++i) {
                SignalID t_b = (i < static_cast<int>(t_bits.size())) ? t_bits[static_cast<std::size_t>(i)] : get_const(false);
                SignalID f_b = (i < static_cast<int>(f_bits.size())) ? f_bits[static_cast<std::size_t>(i)] : get_const(false);
                res.push_back(nl_.add_gate(GateKind::Mux, {c, f_b, t_b}));
            }
            break;
        }
        case ExprKind::Concat: {
            for (int i = static_cast<int>(e->elements.size()) - 1; i >= 0; --i) {
                auto elem_bits = eval_expr(e->elements[static_cast<std::size_t>(i)]);
                res.insert(res.end(), elem_bits.begin(), elem_bits.end());
            }
            break;
        }
        case ExprKind::Replicate: {
            std::vector<SignalID> rep_pattern;
            for (int i = static_cast<int>(e->elements.size()) - 1; i >= 0; --i) {
                auto elem_bits = eval_expr(e->elements[static_cast<std::size_t>(i)]);
                rep_pattern.insert(rep_pattern.end(), elem_bits.begin(), elem_bits.end());
            }
            for (int r = 0; r < e->repeat_count; ++r) {
                res.insert(res.end(), rep_pattern.begin(), rep_pattern.end());
            }
            break;
        }
    }

    // Zero-extend if context width is wider than natural width
    while (static_cast<int>(res.size()) < context_width) {
        res.push_back(get_const(false));
    }

    return res;
}

void Elaborator::elaborate() {
    nl_.module_name = mod_.name;

    // Index all declarations
    for (const auto& decl : mod_.declarations) {
        wire_decls_[decl.name] = WireMeta{decl.name, decl.is_bus, decl.msb, decl.lsb, decl.dir};
        if (decl.is_bus) {
            int low = std::min(decl.msb, decl.lsb);
            int high = std::max(decl.msb, decl.lsb);
            nl_.buses.push_back(BusInfo{decl.name, high, low, decl.dir == PortDir::Input});
        }
    }

    // Register primary inputs in order
    for (const auto& decl : mod_.declarations) {
        if (decl.dir == PortDir::Input) {
            if (decl.is_bus) {
                int low = std::min(decl.msb, decl.lsb);
                int high = std::max(decl.msb, decl.lsb);
                for (int i = low; i <= high; ++i) {
                    std::string bname = decl.name + "[" + std::to_string(i) + "]";
                    SignalID sig = nl_.add_input(bname);
                    wire_signals_[bname] = sig;
                }
            } else {
                SignalID sig = nl_.add_input(decl.name);
                wire_signals_[decl.name] = sig;
            }
        }
    }

    // Register gate drivers
    for (const auto& g : mod_.gate_instances) {
        auto out_bits = flatten_lhs(g.args[0]);
        if (out_bits.size() != 1) {
            throw VerilogParseError("Gate instance output must be 1 bit", 0, 0);
        }
        std::size_t driver_idx = drivers_.size();
        Driver d;
        d.lhs_bits = out_bits;
        d.gate_kind = g.kind;
        d.is_gate = true;
        d.gate_args = g.args;
        drivers_.push_back(std::move(d));
        bit_to_driver_[out_bits[0]] = driver_idx;
    }

    // Register assign drivers
    for (const auto& a : mod_.assigns) {
        auto lhs_bits = flatten_lhs(a.lhs);
        std::size_t driver_idx = drivers_.size();
        Driver d;
        d.lhs_bits = lhs_bits;
        d.rhs_expr = a.rhs;
        d.is_gate = false;
        drivers_.push_back(std::move(d));
        for (const auto& b : lhs_bits) {
            bit_to_driver_[b] = driver_idx;
        }
    }

    // Resolve primary outputs
    for (const auto& decl : mod_.declarations) {
        if (decl.dir == PortDir::Output) {
            if (decl.is_bus) {
                int low = std::min(decl.msb, decl.lsb);
                int high = std::max(decl.msb, decl.lsb);
                for (int i = low; i <= high; ++i) {
                    std::string bname = decl.name + "[" + std::to_string(i) + "]";
                    SignalID sig = resolve_bit(bname);
                    nl_.add_output(bname, sig);
                }
            } else {
                SignalID sig = resolve_bit(decl.name);
                nl_.add_output(decl.name, sig);
            }
        }
    }
}

} // namespace

Netlist parse_verilog_string(const std::string& source, const std::string& top_module) {
    Lexer lex(source);
    Parser parser(lex);
    ModuleAst ast = parser.parse_module(top_module);
    Netlist nl;
    Elaborator elab(ast, nl);
    elab.elaborate();
    return nl;
}

Netlist parse_verilog_file(const std::string& filepath, const std::string& top_module) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open Verilog file: " + filepath);
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return parse_verilog_string(ss.str(), top_module);
}

} // namespace tbc
