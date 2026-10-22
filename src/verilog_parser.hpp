#pragma once

#include "verilog_lexer.hpp"
#include "tbc/netlist.hpp"
#include <memory>
#include <vector>
#include <string>

namespace tbc {

enum class ExprKind {
    Var,
    BitSelect,
    PartSelect,
    Literal,
    Unary,
    Binary,
    Ternary,
    Concat,
    Replicate
};

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

struct Expr {
    ExprKind kind;
    int line = 0;
    int col = 0;

    std::string name;
    int index = 0;
    int msb = 0;
    int lsb = 0;

    int width = 0;
    std::vector<bool> bits; // bit 0 = LSB

    TokType op = TokType::Eof;
    ExprPtr lhs;
    ExprPtr rhs;
    ExprPtr cond;

    int repeat_count = 1;
    std::vector<ExprPtr> elements;
};

enum class PortDir {
    Input,
    Output,
    Wire
};

struct Decl {
    PortDir dir;
    std::string name;
    bool is_bus = false;
    int msb = 0;
    int lsb = 0;
};

struct GateInst {
    GateKind kind;
    std::string instance_name;
    std::vector<ExprPtr> args; // args[0] is output pin, args[1..] are input pins
};

struct AssignStmt {
    ExprPtr lhs;
    ExprPtr rhs;
};

struct ModuleAst {
    std::string name;
    std::vector<std::string> port_names;
    std::vector<Decl> declarations;
    std::vector<GateInst> gate_instances;
    std::vector<AssignStmt> assigns;
};

class Parser {
public:
    explicit Parser(Lexer& lexer);
    ModuleAst parse_module(const std::string& expected_name = "");

private:
    Lexer& lex_;
    Token curr_;

    void advance();
    bool match(TokType t);
    void expect(TokType t, const std::string& ctx);

    void parse_port_declaration(ModuleAst& mod, PortDir dir);
    void parse_gate_instance(ModuleAst& mod, GateKind kind);
    void parse_assign(ModuleAst& mod);

    ExprPtr parse_expression();
    ExprPtr parse_ternary();
    ExprPtr parse_logical_or();
    ExprPtr parse_logical_and();
    ExprPtr parse_bitwise_or();
    ExprPtr parse_bitwise_xor();
    ExprPtr parse_bitwise_and();
    ExprPtr parse_equality();
    ExprPtr parse_addition();
    ExprPtr parse_unary();
    ExprPtr parse_primary();
};

} // namespace tbc
