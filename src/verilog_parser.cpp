#include "verilog_parser.hpp"
#include "tbc/verilog.hpp"
#include <iostream>

namespace tbc {

Parser::Parser(Lexer& lexer) : lex_(lexer) {
    curr_ = lex_.next_token();
}

void Parser::advance() {
    curr_ = lex_.next_token();
}

bool Parser::match(TokType t) {
    if (curr_.type == t) {
        advance();
        return true;
    }
    return false;
}

void Parser::expect(TokType t, const std::string& ctx) {
    if (curr_.type != t) {
        throw VerilogParseError("Expected " + ctx + ", got '" + curr_.text + "'", curr_.line, curr_.col);
    }
    advance();
}

ModuleAst Parser::parse_module(const std::string& expected_name) {
    // Find 'module'
    while (curr_.type != TokType::KwModule && curr_.type != TokType::Eof) {
        advance();
    }
    if (curr_.type == TokType::Eof) {
        throw VerilogParseError("No module declaration found in file", curr_.line, curr_.col);
    }
    advance(); // consume 'module'

    if (curr_.type != TokType::Ident) {
        throw VerilogParseError("Expected module name identifier", curr_.line, curr_.col);
    }
    std::string mod_name = curr_.text;
    advance();

    ModuleAst mod;
    mod.name = mod_name;

    // Optional port list: ( [port_name or full declaration], ... )
    if (match(TokType::LParen)) {
        PortDir current_dir = PortDir::Input;
        bool has_active_dir = false;
        bool current_is_bus = false;
        int current_msb = 0, current_lsb = 0;

        while (curr_.type != TokType::RParen && curr_.type != TokType::Eof) {
            if (curr_.type == TokType::KwInput || curr_.type == TokType::KwOutput) {
                current_dir = (curr_.type == TokType::KwInput) ? PortDir::Input : PortDir::Output;
                has_active_dir = true;
                advance();
                match(TokType::KwWire); // optional 'wire'

                current_is_bus = false;
                current_msb = 0;
                current_lsb = 0;
                if (match(TokType::LBrack)) {
                    current_is_bus = true;
                    if (curr_.type != TokType::DecNumber) {
                        throw VerilogParseError("Expected bus MSB number", curr_.line, curr_.col);
                    }
                    current_msb = std::stoi(curr_.text);
                    advance();
                    expect(TokType::Colon, "':' in bus range");
                    if (curr_.type != TokType::DecNumber) {
                        throw VerilogParseError("Expected bus LSB number", curr_.line, curr_.col);
                    }
                    current_lsb = std::stoi(curr_.text);
                    advance();
                    expect(TokType::RBrack, "']' after bus range");
                }
            }

            if (curr_.type == TokType::Ident) {
                std::string pname = curr_.text;
                advance();
                mod.port_names.push_back(pname);
                if (has_active_dir) {
                    mod.declarations.push_back(Decl{current_dir, pname, current_is_bus, current_msb, current_lsb});
                }
            } else {
                throw VerilogParseError("Unexpected token in port list: '" + curr_.text + "'", curr_.line, curr_.col);
            }

            if (!match(TokType::Comma)) {
                break;
            }
        }
        expect(TokType::RParen, "')' closing module port list");
    }
    expect(TokType::Semi, "';' ending module header");

    // Module body items until 'endmodule'
    while (curr_.type != TokType::KwEndmodule && curr_.type != TokType::Eof) {
        if (curr_.type == TokType::KwInput) {
            advance();
            parse_port_declaration(mod, PortDir::Input);
        } else if (curr_.type == TokType::KwOutput) {
            advance();
            parse_port_declaration(mod, PortDir::Output);
        } else if (curr_.type == TokType::KwWire) {
            advance();
            parse_port_declaration(mod, PortDir::Wire);
        } else if (curr_.type == TokType::KwAssign) {
            advance();
            parse_assign(mod);
        } else if (curr_.type == TokType::KwAnd) {
            advance();
            parse_gate_instance(mod, GateKind::And);
        } else if (curr_.type == TokType::KwNand) {
            advance();
            parse_gate_instance(mod, GateKind::Nand);
        } else if (curr_.type == TokType::KwOr) {
            advance();
            parse_gate_instance(mod, GateKind::Or);
        } else if (curr_.type == TokType::KwNor) {
            advance();
            parse_gate_instance(mod, GateKind::Nor);
        } else if (curr_.type == TokType::KwXor) {
            advance();
            parse_gate_instance(mod, GateKind::Xor);
        } else if (curr_.type == TokType::KwXnor) {
            advance();
            parse_gate_instance(mod, GateKind::Xnor);
        } else if (curr_.type == TokType::KwNot) {
            advance();
            parse_gate_instance(mod, GateKind::Not);
        } else if (curr_.type == TokType::KwBuf) {
            advance();
            parse_gate_instance(mod, GateKind::Buf);
        } else if (curr_.type == TokType::Ident && (curr_.text == "always" || curr_.text == "initial")) {
            throw VerilogParseError("Unsupported sequential/procedural block '" + curr_.text + "' in combinational subset", curr_.line, curr_.col);
        } else {
            throw VerilogParseError("Unexpected statement or declaration: '" + curr_.text + "'", curr_.line, curr_.col);
        }
    }

    expect(TokType::KwEndmodule, "'endmodule'");

    if (!expected_name.empty() && mod.name != expected_name) {
        // If this module didn't match the expected top, and there's more input, caller can handle or we could continue.
    }

    return mod;
}

void Parser::parse_port_declaration(ModuleAst& mod, PortDir dir) {
    match(TokType::KwWire); // e.g. "input wire [7:0] a;"

    bool is_bus = false;
    int msb = 0, lsb = 0;
    if (match(TokType::LBrack)) {
        is_bus = true;
        if (curr_.type != TokType::DecNumber) {
            throw VerilogParseError("Expected bus MSB number", curr_.line, curr_.col);
        }
        msb = std::stoi(curr_.text);
        advance();
        expect(TokType::Colon, "':' in bus range");
        if (curr_.type != TokType::DecNumber) {
            throw VerilogParseError("Expected bus LSB number", curr_.line, curr_.col);
        }
        lsb = std::stoi(curr_.text);
        advance();
        expect(TokType::RBrack, "']' after bus range");
    }

    // Comma-separated list of names
    while (true) {
        if (curr_.type != TokType::Ident) {
            throw VerilogParseError("Expected net/port identifier", curr_.line, curr_.col);
        }
        std::string name = curr_.text;
        advance();
        mod.declarations.push_back(Decl{dir, name, is_bus, msb, lsb});

        if (curr_.type == TokType::Eq) {
            advance();
            ExprPtr rhs = parse_expression();
            auto lhs = std::make_shared<Expr>();
            lhs->kind = ExprKind::Var;
            lhs->name = name;
            mod.assigns.push_back(AssignStmt{lhs, rhs});
        }

        if (!match(TokType::Comma)) {
            break;
        }
    }
    expect(TokType::Semi, "';' ending declaration");
}

void Parser::parse_gate_instance(ModuleAst& mod, GateKind kind) {
    std::string inst_name;
    // Optional instance name
    if (curr_.type == TokType::Ident) {
        inst_name = curr_.text;
        advance();
    }

    expect(TokType::LParen, "'(' for gate port connections");
    std::vector<ExprPtr> args;
    while (curr_.type != TokType::RParen && curr_.type != TokType::Eof) {
        args.push_back(parse_expression());
        if (!match(TokType::Comma)) {
            break;
        }
    }
    expect(TokType::RParen, "')' closing gate port connections");
    expect(TokType::Semi, "';' ending gate instantiation");

    if (args.size() < 2) {
        throw VerilogParseError("Gate instance requires at least 1 output and 1 input argument", curr_.line, curr_.col);
    }

    mod.gate_instances.push_back(GateInst{kind, inst_name, std::move(args)});
}

void Parser::parse_assign(ModuleAst& mod) {
    ExprPtr lhs = parse_primary();
    expect(TokType::Eq, "'=' in continuous assign");
    ExprPtr rhs = parse_expression();
    expect(TokType::Semi, "';' ending assign statement");

    mod.assigns.push_back(AssignStmt{lhs, rhs});
}

ExprPtr Parser::parse_expression() {
    return parse_ternary();
}

ExprPtr Parser::parse_ternary() {
    ExprPtr cond = parse_logical_or();
    if (match(TokType::Question)) {
        ExprPtr then_expr = parse_expression();
        expect(TokType::Colon, "':' in ternary conditional expression");
        ExprPtr else_expr = parse_ternary();

        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Ternary;
        e->cond = cond;
        e->lhs = then_expr;
        e->rhs = else_expr;
        return e;
    }
    return cond;
}

ExprPtr Parser::parse_logical_or() {
    ExprPtr left = parse_logical_and();
    while (curr_.type == TokType::PipePipe) {
        TokType op = curr_.type;
        advance();
        ExprPtr right = parse_logical_and();
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Binary;
        e->op = op;
        e->lhs = left;
        e->rhs = right;
        left = e;
    }
    return left;
}

ExprPtr Parser::parse_logical_and() {
    ExprPtr left = parse_bitwise_or();
    while (curr_.type == TokType::AmpAmp) {
        TokType op = curr_.type;
        advance();
        ExprPtr right = parse_bitwise_or();
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Binary;
        e->op = op;
        e->lhs = left;
        e->rhs = right;
        left = e;
    }
    return left;
}

ExprPtr Parser::parse_bitwise_or() {
    ExprPtr left = parse_bitwise_xor();
    while (curr_.type == TokType::Pipe) {
        TokType op = curr_.type;
        advance();
        ExprPtr right = parse_bitwise_xor();
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Binary;
        e->op = op;
        e->lhs = left;
        e->rhs = right;
        left = e;
    }
    return left;
}

ExprPtr Parser::parse_bitwise_xor() {
    ExprPtr left = parse_bitwise_and();
    while (curr_.type == TokType::Caret || curr_.type == TokType::TildeCaret || curr_.type == TokType::CaretTilde) {
        TokType op = curr_.type;
        advance();
        ExprPtr right = parse_bitwise_and();
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Binary;
        e->op = op;
        e->lhs = left;
        e->rhs = right;
        left = e;
    }
    return left;
}

ExprPtr Parser::parse_bitwise_and() {
    ExprPtr left = parse_equality();
    while (curr_.type == TokType::Amp) {
        TokType op = curr_.type;
        advance();
        ExprPtr right = parse_equality();
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Binary;
        e->op = op;
        e->lhs = left;
        e->rhs = right;
        left = e;
    }
    return left;
}

ExprPtr Parser::parse_equality() {
    ExprPtr left = parse_addition();
    while (curr_.type == TokType::EqEq || curr_.type == TokType::ExclEq) {
        TokType op = curr_.type;
        advance();
        ExprPtr right = parse_addition();
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Binary;
        e->op = op;
        e->lhs = left;
        e->rhs = right;
        left = e;
    }
    return left;
}

ExprPtr Parser::parse_addition() {
    ExprPtr left = parse_unary();
    while (curr_.type == TokType::Plus || curr_.type == TokType::Minus) {
        TokType op = curr_.type;
        advance();
        ExprPtr right = parse_unary();
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Binary;
        e->op = op;
        e->lhs = left;
        e->rhs = right;
        left = e;
    }
    return left;
}

ExprPtr Parser::parse_unary() {
    if (curr_.type == TokType::Tilde || curr_.type == TokType::Excl ||
        curr_.type == TokType::Amp || curr_.type == TokType::Pipe || curr_.type == TokType::Caret) {
        TokType op = curr_.type;
        advance();
        ExprPtr arg = parse_unary();
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Unary;
        e->op = op;
        e->lhs = arg;
        return e;
    }
    return parse_primary();
}

ExprPtr Parser::parse_primary() {
    if (match(TokType::LParen)) {
        ExprPtr inner = parse_expression();
        expect(TokType::RParen, "')' closing grouped expression");
        return inner;
    }

    if (curr_.type == TokType::SizedNumber) {
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Literal;
        e->width = curr_.width;
        e->bits = std::move(curr_.bits);
        advance();
        return e;
    }

    if (curr_.type == TokType::DecNumber) {
        unsigned long long val = std::stoull(curr_.text);
        advance();
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Literal;
        e->width = 32; // Standard unsized integer width in Verilog
        e->bits.resize(32, false);
        for (int i = 0; i < 32; ++i) {
            e->bits[static_cast<std::size_t>(i)] = ((val >> i) & 1ULL) != 0;
        }
        return e;
    }

    if (match(TokType::LBrace)) {
        // Concatenation or Replication:
        // Check if first element is replication count: { count { expr, ... } }
        // If curr is DecNumber and followed by LBrace, it's replication
        if (curr_.type == TokType::DecNumber) {
            int count = std::stoi(curr_.text);
            advance();
            if (match(TokType::LBrace)) {
                // Replication
                std::vector<ExprPtr> rep_elems;
                while (curr_.type != TokType::RBrace && curr_.type != TokType::Eof) {
                    rep_elems.push_back(parse_expression());
                    if (!match(TokType::Comma)) break;
                }
                expect(TokType::RBrace, "'}' closing replication elements");
                expect(TokType::RBrace, "'}' closing outer replication braces");

                auto e = std::make_shared<Expr>();
                e->kind = ExprKind::Replicate;
                e->repeat_count = count;
                e->elements = std::move(rep_elems);
                return e;
            }
            // Not replication, backtracking single number in concat
            // Reconstruct literal
            auto lit = std::make_shared<Expr>();
            lit->kind = ExprKind::Literal;
            lit->width = 32;
            lit->bits.resize(32, false);
            for (int i = 0; i < 32; ++i) lit->bits[static_cast<std::size_t>(i)] = ((count >> i) & 1) != 0;

            std::vector<ExprPtr> concat_elems;
            concat_elems.push_back(lit);
            if (match(TokType::Comma)) {
                while (curr_.type != TokType::RBrace && curr_.type != TokType::Eof) {
                    concat_elems.push_back(parse_expression());
                    if (!match(TokType::Comma)) break;
                }
            }
            expect(TokType::RBrace, "'}' closing concatenation");
            auto e = std::make_shared<Expr>();
            e->kind = ExprKind::Concat;
            e->elements = std::move(concat_elems);
            return e;
        }

        // Standard concatenation: { e1, e2, ... }
        std::vector<ExprPtr> concat_elems;
        while (curr_.type != TokType::RBrace && curr_.type != TokType::Eof) {
            concat_elems.push_back(parse_expression());
            if (!match(TokType::Comma)) break;
        }
        expect(TokType::RBrace, "'}' closing concatenation");

        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Concat;
        e->elements = std::move(concat_elems);
        return e;
    }

    if (curr_.type == TokType::Ident) {
        std::string name = curr_.text;
        advance();

        if (match(TokType::LBrack)) {
            if (curr_.type != TokType::DecNumber) {
                throw VerilogParseError("Expected constant index or MSB in brackets", curr_.line, curr_.col);
            }
            int idx1 = std::stoi(curr_.text);
            advance();

            if (match(TokType::Colon)) {
                // Part select: name[msb:lsb]
                if (curr_.type != TokType::DecNumber) {
                    throw VerilogParseError("Expected constant LSB in part-select", curr_.line, curr_.col);
                }
                int idx2 = std::stoi(curr_.text);
                advance();
                expect(TokType::RBrack, "']' closing part-select");

                auto e = std::make_shared<Expr>();
                e->kind = ExprKind::PartSelect;
                e->name = name;
                e->msb = idx1;
                e->lsb = idx2;
                return e;
            } else {
                // Bit select: name[index]
                expect(TokType::RBrack, "']' closing bit-select");
                auto e = std::make_shared<Expr>();
                e->kind = ExprKind::BitSelect;
                e->name = name;
                e->index = idx1;
                return e;
            }
        }

        // Simple variable reference
        auto e = std::make_shared<Expr>();
        e->kind = ExprKind::Var;
        e->name = name;
        return e;
    }

    throw VerilogParseError("Unexpected token in primary expression: '" + curr_.text + "'", curr_.line, curr_.col);
}

} // namespace tbc
