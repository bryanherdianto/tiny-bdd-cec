// Golden Reference Specification for 16-bit CPU Instruction Decoder
module decoder (
    input cpu_clk,
    input cpu_enable,
    input [15:0] instruction,
    output [3:0] opcode,
    output address_flag,
    output negative_flag,
    output [3:0] operand1,
    output [5:0] operand2
);

    assign opcode = cpu_enable ? instruction[15:12] : 4'b0000;
    assign address_flag = cpu_enable ? instruction[11] : 1'b0;
    assign negative_flag = cpu_enable ? instruction[10] : 1'b0;
    assign operand1 = cpu_enable ? instruction[9:6] : 4'b0000;

    // When address_flag is 1: bits [5:2] are source register address shifted left by 2
    // When address_flag is 0: bits [5:0] are 6-bit immediate
    assign operand2 = cpu_enable ? (instruction[11] ? {instruction[5:2], 2'b00} : instruction[5:0]) : 6'b000000;

endmodule
