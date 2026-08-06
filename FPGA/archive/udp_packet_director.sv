`timescale 1ns/1ps
import rm_proto_pkg::*;

module udp_packet_director #(
  parameter bit         DROP_NON_UDP = 1'b1,
  parameter logic [5:0] CTRL_DSCP    = RM_CTRL_DSCP
)(
  input  logic        clk,
  input  logic        rst_n,

  // Parsed L3/L4 metadata
  input  logic        in_valid,
  input  logic        in_is_ipv4,
  input  logic        in_is_udp,
  input  logic [5:0]  in_ip_dscp,
  input  logic [31:0] in_dst_ip,
  input  logic [31:0] in_src_ip,
  input  logic [15:0] in_dst_port,
  input  logic [15:0] in_src_port,

  // Parsed RM header fields (normalized)
  input  logic        in_rm_hdr_valid,
  input  logic        in_rm_checksum_ok,
  input  logic [15:0] in_rm_flags,
  input  logic [31:0] in_rm_seq,
  input  logic [7:0]  in_rm_retrans_id,
  input  logic [7:0]  in_rm_receiver_id,
  input  logic [31:0] in_rm_tsval,
  input  logic [31:0] in_rm_tsecr,

  // Classification outputs
  // out_multicast_valid / out_unicast_valid are the local control-plane path.
  // Only DSCP=CTRL_DSCP packets are forwarded there.
  output logic        out_multicast_valid,
  output logic        out_unicast_valid,
  output logic        out_host_valid,
  output logic        out_drop_valid,

  // Pass-through fields
  output logic [5:0]  out_ip_dscp,
  output logic [31:0] out_src_ip,
  output logic [15:0] out_src_port,
  output logic [31:0] out_dst_ip,
  output logic [15:0] out_dst_port,
  output logic        out_rm_hdr_valid,
  output logic        out_rm_checksum_ok,
  output logic [15:0] out_rm_flags,
  output logic [31:0] out_rm_seq,
  output logic [7:0]  out_rm_retrans_id,
  output logic [7:0]  out_rm_receiver_id,
  output logic [31:0] out_rm_tsval,
  output logic [31:0] out_rm_tsecr
);

  logic is_ipv4_mcast;
  logic is_udp_pkt;
  logic is_ctrl_pkt;

  always_comb begin
    is_ipv4_mcast = (in_dst_ip[31:28] == 4'hE);
    is_udp_pkt    = in_is_ipv4 && in_is_udp;
    is_ctrl_pkt   = is_udp_pkt && (in_ip_dscp == CTRL_DSCP);

    out_multicast_valid = 1'b0;
    out_unicast_valid   = 1'b0;
    out_host_valid      = 1'b0;
    out_drop_valid      = 1'b0;

    out_ip_dscp         = in_ip_dscp;
    out_src_ip          = in_src_ip;
    out_src_port        = in_src_port;
    out_dst_ip          = in_dst_ip;
    out_dst_port        = in_dst_port;
    out_rm_hdr_valid    = in_rm_hdr_valid;
    out_rm_checksum_ok  = in_rm_checksum_ok;
    out_rm_flags        = in_rm_flags;
    out_rm_seq          = in_rm_seq;
    out_rm_retrans_id   = in_rm_retrans_id;
    out_rm_receiver_id  = in_rm_receiver_id;
    out_rm_tsval        = in_rm_tsval;
    out_rm_tsecr        = in_rm_tsecr;

    if (in_valid) begin
      if (!is_udp_pkt) begin
        if (DROP_NON_UDP) begin
          out_drop_valid = 1'b1;
        end else begin
          out_host_valid = 1'b1;
        end
      end else if (is_ctrl_pkt) begin
        if (is_ipv4_mcast) begin
          out_multicast_valid = 1'b1;
        end else begin
          out_unicast_valid = 1'b1;
        end
      end else begin
        // Not PEEL/RM control traffic: let the normal host/NIC path consume it.
        out_host_valid = 1'b1;
      end
    end
  end

endmodule
