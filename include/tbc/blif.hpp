#pragma once

#include "tbc/netlist.hpp"
#include <string>
#include <stdexcept>

namespace tbc {

class BlifParseError : public std::runtime_error {
public:
    explicit BlifParseError(const std::string& msg, int line = 0)
        : std::runtime_error("BLIF parse error" + (line > 0 ? " at line " + std::to_string(line) : "") + ": " + msg),
          line_(line) {}

    [[nodiscard]] int line() const noexcept { return line_; }
private:
    int line_;
};

struct BlifOptions {
    bool cut_latches = true; // Cut DFF latches into pseudo-inputs (Q) and pseudo-outputs (D)
};

Netlist parse_blif_string(const std::string& source, const BlifOptions& opts = {});
Netlist parse_blif_file(const std::string& filepath, const BlifOptions& opts = {});

} // namespace tbc
