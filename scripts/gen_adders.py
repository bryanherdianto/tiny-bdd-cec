#!/usr/bin/env python3
"""
gen_adders.py - Parametric Verilog Adder Generator for tiny-bdd-cec
Generates behavioral spec, structural Ripple-Carry (RCA), Carry-Lookahead (CLA),
and injected-bug adders for any bit width N.
"""

import sys
import argparse
from pathlib import Path

def gen_spec(width: int) -> str:
    return f"""// Behavioral {width}-bit Adder Specification
module adder{width}(
    input  [{width-1}:0] a,
    input  [{width-1}:0] b,
    input        cin,
    output [{width-1}:0] sum,
    output       cout
);
    assign {{cout, sum}} = a + b + cin;
endmodule
"""

def gen_rca(width: int, inject_bug: bool = False, bug_bit: int = 3, or_carry: bool = False) -> str:
    lines = [
        f"// Structural {width}-bit Ripple-Carry Adder {'(WITH INJECTED BUG at bit ' + str(bug_bit) + ')' if inject_bug else ''}",
        f"module adder{width}(",
        f"    input  [{width-1}:0] a,",
        f"    input  [{width-1}:0] b,",
        f"    input        cin,",
        f"    output [{width-1}:0] sum,",
        f"    output       cout",
        f");",
        f"    wire [{width-2}:0] c;" if width > 1 else "",
        ""
    ]

    for i in range(width):
        cin_sig = "cin" if i == 0 else f"c[{i-1}]"
        cout_sig = "cout" if i == width - 1 else f"c[{i}]"

        lines.append(f"    // Bit {i}")
        lines.append(f"    xor (sum[{i}], a[{i}], b[{i}], {cin_sig});")

        if inject_bug and i == bug_bit:
            # Bug: missing generate term (a & b)
            lines.append(f"    assign {cout_sig} = ({cin_sig} & (a[{i}] ^ b[{i}])); // INJECTED BUG: omitted (a & b)")
        elif or_carry:
            # Mathematically equivalent alternative: (a & b) | (c & (a | b))
            lines.append(f"    assign {cout_sig} = (a[{i}] & b[{i}]) | ({cin_sig} & (a[{i}] | b[{i}]));")
        else:
            # Standard: (a & b) | (c & (a ^ b))
            lines.append(f"    assign {cout_sig} = (a[{i}] & b[{i}]) | ({cin_sig} & (a[{i}] ^ b[{i}]));")

    lines.append("endmodule")
    return "\n".join(lines) + "\n"

def gen_cla(width: int) -> str:
    lines = [
        f"// Structural {width}-bit Carry-Lookahead Adder (4-bit CLA blocks)",
        f"module adder{width}(",
        f"    input  [{width-1}:0] a,",
        f"    input  [{width-1}:0] b,",
        f"    input        cin,",
        f"    output [{width-1}:0] sum,",
        f"    output       cout",
        f");",
        f"    wire [{width-1}:0] p = a ^ b;",
        f"    wire [{width-1}:0] g = a & b;",
        f"    wire [{width}:0] c;",
        "    assign c[0] = cin;",
        ""
    ]

    # Generate 4-bit CLA blocks
    for blk in range(0, width, 4):
        blk_end = min(blk + 4, width)
        lines.append(f"    // CLA Block {blk // 4}: bits [{blk_end-1}:{blk}]")
        for i in range(blk, blk_end):
            terms = [f"g[{i}]"]
            # Accumulate terms backwards from i down to blk
            for j in range(i - 1, blk - 1, -1):
                p_chain = " & ".join([f"p[{k}]" for k in range(j + 1, i + 1)])
                terms.append(f"({p_chain} & g[{j}])")
            # Base carry term
            p_all = " & ".join([f"p[{k}]" for k in range(blk, i + 1)])
            terms.append(f"({p_all} & c[{blk}])")

            lines.append(f"    assign c[{i+1}] = {' | '.join(terms)};")

    lines.append("")
    lines.append("    // Sum bits")
    for i in range(width):
        lines.append(f"    xor (sum[{i}], p[{i}], c[{i}]);")

    lines.append(f"    assign cout = c[{width}];")
    lines.append("endmodule")
    return "\n".join(lines) + "\n"

def main():
    parser = argparse.ArgumentParser(description="Parametric Verilog Adder Generator")
    parser.add_argument("--width", type=int, default=8, help="Adder bit width")
    parser.add_argument("--out-dir", type=Path, default=Path("."), help="Output directory")
    args = parser.parse_args()

    w = args.width
    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)

    (out / f"adder{w}_spec.v").write_text(gen_spec(w), encoding="utf-8")
    (out / f"adder{w}_rca.v").write_text(gen_rca(w), encoding="utf-8")
    (out / f"adder{w}_cla.v").write_text(gen_cla(w), encoding="utf-8")
    (out / f"adder{w}_rca_orcarry.v").write_text(gen_rca(w, or_carry=True), encoding="utf-8")
    (out / f"adder{w}_rca_bug.v").write_text(gen_rca(w, inject_bug=True, bug_bit=w // 2), encoding="utf-8")

    print(f"Generated {w}-bit adder test suite in: {out}")

if __name__ == "__main__":
    main()
