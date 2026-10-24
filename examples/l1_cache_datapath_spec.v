// L1 Cache Controller Combinational Datapath: Tag Matching & Word Extraction
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

    // Tag matching
    assign way0_hit = way0_valid && (way0_tag == cur_tag);
    assign way1_hit = way1_valid && (way1_tag == cur_tag);

    // Hit evaluation in S_IDLE (state == 3'b000)
    assign cpu_hit = (way0_hit || way1_hit) && (state == 3'b000) && cpu_req;

    // Word extraction from 128-bit line
    assign rdata = (word_offset == 2'b00) ? line_data[31:0] :
                   (word_offset == 2'b01) ? line_data[63:32] :
                   (word_offset == 2'b10) ? line_data[95:64] :
                                            line_data[127:96];

endmodule
