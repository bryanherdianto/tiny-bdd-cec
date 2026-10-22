#pragma once

#include "tbc/netlist.hpp"
#include <string>
#include <stdexcept>

namespace tbc {

class VerilogParseError : public std::runtime_error {
public:
    explicit VerilogParseError(const std::string& msg, int line = 0, int col = 0)
        : std::runtime_error("Parse error" + (line > 0 ? " at line " + std::to_string(line) + ":" + std::to_string(col) : "") + ": " + msg),
          line_(line), col_(col) {}

    [[nodiscard]] int line() const noexcept { return line_; }
    [[nodiscard]] int col() const noexcept { return col_; }
private:
    int line_;
    int col_;
};

Netlist parse_verilog_file(const std::string& filepath, const std::string& top_module = "");
Netlist parse_verilog_string(const std::string& source, const std::string& top_module = "");

} // namespace tbc
