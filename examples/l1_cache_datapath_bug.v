// Mutated L1 Cache Datapath: Injected bug in Way 1 tag comparator
module l1_cache_datapath (
    input [21:0] cur_tag,
    input [21:0] way0_tag,
    input [21:0] way1_tag,
    input        way0_valid,
    input        way1_valid,
    input        cpu_req,
    input [2:0]  state,
    input [1:0]  word_offset,
    input [127:0] line_data,
    output       way0_hit,
    output       way1_hit,
    output       cpu_hit,
    output [31:0] rdata
);

    assign way0_hit = way0_valid && (way0_tag == cur_tag);
    // BUG: Checks way0_tag instead of way1_tag
    assign way1_hit = way1_valid && (way0_tag == cur_tag);

    assign cpu_hit = (way0_hit || way1_hit) && (state == 3'b000) && cpu_req;

    assign rdata = (word_offset == 2'b00) ? line_data[31:0] :
                   (word_offset == 2'b01) ? line_data[63:32] :
                   (word_offset == 2'b10) ? line_data[95:64] :
                                            line_data[127:96];

endmodule
