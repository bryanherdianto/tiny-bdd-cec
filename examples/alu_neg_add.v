module alu_neg_add (
    input [7:0] a,
    input [7:0] b,
    output [7:0] res
);
    assign res = a + (~b) + 8'd1;
endmodule
