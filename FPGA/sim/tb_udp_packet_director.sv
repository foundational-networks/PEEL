`timescale 1ns/1ps
import rm_proto_pkg::*;

module tb_udp_packet_director;

  logic        clk;
  logic        rst_n;
  logic        in_valid;
  logic        in_is_ipv4;
  logic        in_is_udp;
  logic [5:0]  in_ip_dscp;
  logic [31:0] in_dst_ip;
  logic [31:0] in_src_ip;
  logic [15:0] in_dst_port;
  logic [15:0] in_src_port;
  logic        in_rm_hdr_valid;
  logic        in_rm_checksum_ok;
  logic [15:0] in_rm_flags;
  logic [31:0] in_rm_seq;
  logic [7:0]  in_rm_retrans_id;
  logic [7:0]  in_rm_receiver_id;
  logic [31:0] in_rm_tsval;
  logic [31:0] in_rm_tsecr;

  logic        out_multicast_valid;
  logic        out_unicast_valid;
  logic        out_host_valid;
  logic        out_drop_valid;
  logic [5:0]  out_ip_dscp;
  logic [31:0] out_src_ip;
  logic [15:0] out_src_port;
  logic [31:0] out_dst_ip;
  logic [15:0] out_dst_port;
  logic        out_rm_hdr_valid;
  logic        out_rm_checksum_ok;
  logic [15:0] out_rm_flags;
  logic [31:0] out_rm_seq;
  logic [7:0]  out_rm_retrans_id;
  logic [7:0]  out_rm_receiver_id;
  logic [31:0] out_rm_tsval;
  logic [31:0] out_rm_tsecr;

  udp_packet_director dut (
    .clk,
    .rst_n,
    .in_valid,
    .in_is_ipv4,
    .in_is_udp,
    .in_ip_dscp,
    .in_dst_ip,
    .in_src_ip,
    .in_dst_port,
    .in_src_port,
    .in_rm_hdr_valid,
    .in_rm_checksum_ok,
    .in_rm_flags,
    .in_rm_seq,
    .in_rm_retrans_id,
    .in_rm_receiver_id,
    .in_rm_tsval,
    .in_rm_tsecr,
    .out_multicast_valid,
    .out_unicast_valid,
    .out_host_valid,
    .out_drop_valid,
    .out_ip_dscp,
    .out_src_ip,
    .out_src_port,
    .out_dst_ip,
    .out_dst_port,
    .out_rm_hdr_valid,
    .out_rm_checksum_ok,
    .out_rm_flags,
    .out_rm_seq,
    .out_rm_retrans_id,
    .out_rm_receiver_id,
    .out_rm_tsval,
    .out_rm_tsecr
  );

  always #5 clk = ~clk;

  task automatic clear_inputs;
    begin
      in_valid          = 1'b0;
      in_is_ipv4        = 1'b0;
      in_is_udp         = 1'b0;
      in_ip_dscp        = '0;
      in_dst_ip         = '0;
      in_src_ip         = '0;
      in_dst_port       = '0;
      in_src_port       = '0;
      in_rm_hdr_valid   = 1'b0;
      in_rm_checksum_ok = 1'b0;
      in_rm_flags       = '0;
      in_rm_seq         = '0;
      in_rm_retrans_id  = '0;
      in_rm_receiver_id = '0;
      in_rm_tsval       = '0;
      in_rm_tsecr       = '0;
    end
  endtask

  task automatic check_onehot(
    input logic exp_mc,
    input logic exp_uc,
    input logic exp_host,
    input logic exp_drop,
    input string name
  );
    begin
      #1;
      if (out_multicast_valid !== exp_mc ||
          out_unicast_valid   !== exp_uc ||
          out_host_valid      !== exp_host ||
          out_drop_valid      !== exp_drop) begin
        $error("%s failed: mc=%0b uc=%0b host=%0b drop=%0b", name,
               out_multicast_valid, out_unicast_valid, out_host_valid, out_drop_valid);
        $fatal(1);
      end
    end
  endtask

  initial begin
    clk = 1'b0;
    rst_n = 1'b0;
    clear_inputs();

    repeat (3) @(posedge clk);
    rst_n = 1'b1;
    @(posedge clk);

    // Case 1: DSCP=7 + multicast destination => local multicast control path.
    in_valid          = 1'b1;
    in_is_ipv4        = 1'b1;
    in_is_udp         = 1'b1;
    in_ip_dscp        = 6'd7;
    in_dst_ip         = 32'hE0010203;
    in_src_ip         = 32'h0A000001;
    in_dst_port       = 16'd5000;
    in_src_port       = 16'd4000;
    in_rm_hdr_valid   = 1'b1;
    in_rm_checksum_ok = 1'b1;
    in_rm_flags       = FLG_SYN;
    in_rm_seq         = 32'd11;
    in_rm_retrans_id  = 8'd3;
    in_rm_receiver_id = 8'd5;
    in_rm_tsval       = 32'h1234;
    in_rm_tsecr       = 32'h5678;
    check_onehot(1'b1, 1'b0, 1'b0, 1'b0, "dscp7 multicast");

    if (out_rm_seq != 32'd11 || out_rm_retrans_id != 8'd3 || out_rm_tsval != 32'h1234) begin
      $error("pass-through metadata mismatch on multicast case");
      $fatal(1);
    end

    // Case 2: DSCP=7 + unicast destination => local unicast control path.
    in_dst_ip = 32'h0A000002;
    check_onehot(1'b0, 1'b1, 1'b0, 1'b0, "dscp7 unicast");

    // Case 3: UDP but DSCP!=7 => normal host path.
    in_ip_dscp = 6'd0;
    check_onehot(1'b0, 1'b0, 1'b1, 1'b0, "non-control udp to host");

    // Case 4: non-UDP packet => drop path in default configuration.
    in_is_udp = 1'b0;
    check_onehot(1'b0, 1'b0, 1'b0, 1'b1, "non-udp drop");

    // Case 5: idle => no outputs asserted.
    clear_inputs();
    check_onehot(1'b0, 1'b0, 1'b0, 1'b0, "idle");

    $display("tb_udp_packet_director PASS");
    $finish;
  end

endmodule
