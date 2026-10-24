// Fast Bitwise Parallel Implementation of Clamp
module clamp_byte (
    input [35:0] in_val,
    output [7:0] out_byte
);
    wire is_neg = in_val[35];
    wire is_overflow = |in_val[34:8];

    // High-performance branchless bitwise masking
    assign out_byte = {8{~is_neg}} & ({8{is_overflow}} | in_val[7:0]);
endmodule
