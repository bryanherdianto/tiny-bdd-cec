#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace tbc {

enum class TokType {
    Eof,
    Ident,
    DecNumber,       // e.g. 0, 1, 42
    SizedNumber,     // e.g. 8'hFF, 1'b0, 4'd12, 16'b0000...
    KwModule,
    KwEndmodule,
    KwInput,
    KwOutput,
    KwWire,
    KwAssign,
    KwAnd,
    KwNand,
    KwOr,
    KwNor,
    KwXor,
    KwXnor,
    KwNot,
    KwBuf,
    LParen,          // (
    RParen,          // )
    LBrack,          // [
    RBrack,          // ]
    LBrace,          // {
    RBrace,          // }
    Comma,           // ,
    Semi,            // ;
    Colon,           // :
    Question,        // ?
    Tilde,           // ~
    Amp,             // &
    Pipe,            // |
    Caret,           // ^
    TildeCaret,      // ~^
    CaretTilde,      // ^~
    Excl,            // !
    AmpAmp,          // &&
    PipePipe,        // ||
    Eq,              // =
    EqEq,            // ==
    ExclEq,          // !=
    Plus,            // +
    Minus            // -
};

struct Token {
    TokType type = TokType::Eof;
    std::string text;
    int line = 1;
    int col = 1;

    // For SizedNumber:
    int width = 0;
    std::vector<bool> bits; // bit 0 = LSB
};

class Lexer {
public:
    explicit Lexer(std::string_view source);
    Token next_token();
    [[nodiscard]] Token peek_token();

private:
    std::string_view src_;
    std::size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    Token peeked_;
    bool has_peeked_ = false;

    char peek_char() const noexcept {
        return pos_ < src_.size() ? src_[pos_] : '\0';
    }
    char advance_char() noexcept {
        if (pos_ >= src_.size()) return '\0';
        char c = src_[pos_++];
        if (c == '\n') {
            ++line_;
            col_ = 1;
        } else {
            ++col_;
        }
        return c;
    }
    void skip_whitespace_and_comments();
    Token scan_number(int start_line, int start_col);
    Token scan_ident_or_kw(int start_line, int start_col);
};

} // namespace tbc
