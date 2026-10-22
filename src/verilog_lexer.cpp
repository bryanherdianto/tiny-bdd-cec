#include "verilog_lexer.hpp"
#include "tbc/verilog.hpp"
#include <cctype>
#include <sstream>

namespace tbc {

Lexer::Lexer(std::string_view source) : src_(source) {}

Token Lexer::peek_token() {
    if (!has_peeked_) {
        peeked_ = next_token();
        has_peeked_ = true;
    }
    return peeked_;
}

void Lexer::skip_whitespace_and_comments() {
    while (pos_ < src_.size()) {
        char c = src_[pos_];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance_char();
        } else if (c == '`') {
            // Skip compiler directives, e.g. `timescale 1ns/1ps
            while (pos_ < src_.size() && src_[pos_] != '\n') {
                advance_char();
            }
        } else if (c == '/' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '/') {
            // Line comment
            while (pos_ < src_.size() && src_[pos_] != '\n') {
                advance_char();
            }
        } else if (c == '/' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '*') {
            // Block comment
            advance_char(); // /
            advance_char(); // *
            while (pos_ < src_.size()) {
                if (src_[pos_] == '*' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '/') {
                    advance_char();
                    advance_char();
                    break;
                }
                advance_char();
            }
        } else {
            break;
        }
    }
}

Token Lexer::scan_number(int start_line, int start_col) {
    std::string num_str;
    while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
        num_str += advance_char();
    }

    if (pos_ < src_.size() && src_[pos_] == '\'') {
        // Sized literal, e.g. 8'hFF, 1'b0, 4'd9
        advance_char(); // '
        if (pos_ >= src_.size()) {
            throw VerilogParseError("Unexpected EOF after base specifier in number", start_line, start_col);
        }
        char base = static_cast<char>(std::tolower(static_cast<unsigned char>(advance_char())));
        int width = std::stoi(num_str);

        std::string val_str;
        while (pos_ < src_.size() && (std::isxdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
            char vc = advance_char();
            if (vc != '_') val_str += vc;
        }

        std::vector<bool> bits(static_cast<std::size_t>(width), false);
        if (base == 'b') {
            // Binary string, e.g. "101"
            for (std::size_t i = 0; i < val_str.size() && i < static_cast<std::size_t>(width); ++i) {
                char b = val_str[val_str.size() - 1 - i];
                bits[i] = (b == '1');
            }
        } else if (base == 'h') {
            // Hex string
            std::size_t bit_idx = 0;
            for (int i = static_cast<int>(val_str.size()) - 1; i >= 0; --i) {
                char h = val_str[static_cast<std::size_t>(i)];
                int v = 0;
                if (h >= '0' && h <= '9') v = h - '0';
                else if (h >= 'a' && h <= 'f') v = 10 + (h - 'a');
                else if (h >= 'A' && h <= 'F') v = 10 + (h - 'A');
                for (int b = 0; b < 4 && bit_idx < static_cast<std::size_t>(width); ++b, ++bit_idx) {
                    bits[bit_idx] = ((v >> b) & 1) != 0;
                }
            }
        } else if (base == 'd') {
            // Decimal string
            unsigned long long val = std::stoull(val_str);
            for (std::size_t i = 0; i < static_cast<std::size_t>(width); ++i) {
                bits[i] = ((val >> i) & 1ULL) != 0;
            }
        } else {
            throw VerilogParseError(std::string("Unsupported number base: '") + base, start_line, start_col);
        }

        Token tok;
        tok.type = TokType::SizedNumber;
        tok.text = num_str + "'" + base + val_str;
        tok.line = start_line;
        tok.col = start_col;
        tok.width = width;
        tok.bits = std::move(bits);
        return tok;
    }

    Token tok;
    tok.type = TokType::DecNumber;
    tok.text = num_str;
    tok.line = start_line;
    tok.col = start_col;
    return tok;
}

Token Lexer::scan_ident_or_kw(int start_line, int start_col) {
    std::string text;
    while (pos_ < src_.size()) {
        char c = src_[pos_];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            text += advance_char();
        } else {
            break;
        }
    }

    Token tok;
    tok.line = start_line;
    tok.col = start_col;
    tok.text = text;

    if (text == "module")       tok.type = TokType::KwModule;
    else if (text == "endmodule") tok.type = TokType::KwEndmodule;
    else if (text == "input")   tok.type = TokType::KwInput;
    else if (text == "output")  tok.type = TokType::KwOutput;
    else if (text == "wire")    tok.type = TokType::KwWire;
    else if (text == "assign")  tok.type = TokType::KwAssign;
    else if (text == "and")     tok.type = TokType::KwAnd;
    else if (text == "nand")    tok.type = TokType::KwNand;
    else if (text == "or")      tok.type = TokType::KwOr;
    else if (text == "nor")     tok.type = TokType::KwNor;
    else if (text == "xor")     tok.type = TokType::KwXor;
    else if (text == "xnor")    tok.type = TokType::KwXnor;
    else if (text == "not")     tok.type = TokType::KwNot;
    else if (text == "buf")     tok.type = TokType::KwBuf;
    else                        tok.type = TokType::Ident;

    return tok;
}

Token Lexer::next_token() {
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_;
    }

    skip_whitespace_and_comments();
    if (pos_ >= src_.size()) {
        return Token{TokType::Eof, "", line_, col_, 0, {}};
    }

    int start_line = line_;
    int start_col = col_;
    char c = peek_char();

    if (std::isdigit(static_cast<unsigned char>(c))) {
        return scan_number(start_line, start_col);
    }

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scan_ident_or_kw(start_line, start_col);
    }

    advance_char();

    auto make_tok = [&](TokType t, std::string text) {
        return Token{t, std::move(text), start_line, start_col, 0, {}};
    };

    switch (c) {
        case '(': return make_tok(TokType::LParen, "(");
        case ')': return make_tok(TokType::RParen, ")");
        case '[': return make_tok(TokType::LBrack, "[");
        case ']': return make_tok(TokType::RBrack, "]");
        case '{': return make_tok(TokType::LBrace, "{");
        case '}': return make_tok(TokType::RBrace, "}");
        case ',': return make_tok(TokType::Comma, ",");
        case ';': return make_tok(TokType::Semi, ";");
        case ':': return make_tok(TokType::Colon, ":");
        case '?': return make_tok(TokType::Question, "?");
        case '+': return make_tok(TokType::Plus, "+");
        case '-': return make_tok(TokType::Minus, "-");
        case '~':
            if (peek_char() == '^') {
                advance_char();
                return make_tok(TokType::TildeCaret, "~^");
            }
            return make_tok(TokType::Tilde, "~");
        case '^':
            if (peek_char() == '~') {
                advance_char();
                return make_tok(TokType::CaretTilde, "^~");
            }
            return make_tok(TokType::Caret, "^");
        case '&':
            if (peek_char() == '&') {
                advance_char();
                return make_tok(TokType::AmpAmp, "&&");
            }
            return make_tok(TokType::Amp, "&");
        case '|':
            if (peek_char() == '|') {
                advance_char();
                return make_tok(TokType::PipePipe, "||");
            }
            return make_tok(TokType::Pipe, "|");
        case '=':
            if (peek_char() == '=') {
                advance_char();
                return make_tok(TokType::EqEq, "==");
            }
            return make_tok(TokType::Eq, "="); // assignment '='
        case '!':
            if (peek_char() == '=') {
                advance_char();
                return make_tok(TokType::ExclEq, "!=");
            }
            return make_tok(TokType::Excl, "!");
        default:
            throw VerilogParseError(std::string("Unexpected character: '") + c + "'", start_line, start_col);
    }
}

} // namespace tbc
