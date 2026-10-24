// Image Convolution Saturating Accumulator Clamp Specification
module clamp_byte (
    input [35:0] in_val,
    output [7:0] out_byte
);
    wire is_neg = in_val[35];
    wire is_overflow = |in_val[34:8];

    assign out_byte = is_neg ? 8'h00 : (is_overflow ? 8'hFF : in_val[7:0]);
endmodule
