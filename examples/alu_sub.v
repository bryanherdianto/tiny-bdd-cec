module alu_sub (
    input [7:0] a,
    input [7:0] b,
    output [7:0] res
);
    assign res = a - b;
endmodule
