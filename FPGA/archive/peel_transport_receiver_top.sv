`timescale 1ns/1ps
import rm_proto_pkg::*;

module peel_transport_receiver_top #(
  parameter int MAX_COHORT    = 16,
  parameter int TS_WIDTH      = 32,
  parameter int RECEIVER_ID_W = (MAX_COHORT > 1) ? $clog2(MAX_COHORT) : 1
)(
  input  logic                   clk,
  input  logic                   rst_n,
  input  logic                   tick_1ms,

  input  logic [RECEIVER_ID_W-1:0] cfg_receiver_id,
  input  logic [15:0]            cfg_receiver_port,
  input  logic [15:0]            cfg_linger_ms,

  // Parsed RX metadata from your parser/NIC.
  input  logic                   in_valid,
  input  logic                   in_is_ipv4,
  input  logic                   in_is_udp,
  input  logic [5:0]             in_ip_dscp,
  input  logic [31:0]            in_dst_ip,
  input  logic [31:0]            in_src_ip,
  input  logic [15:0]            in_dst_port,
  input  logic [15:0]            in_src_port,
  input  logic                   in_rm_hdr_valid,
  input  logic                   in_rm_checksum_ok,
  input  logic [15:0]            in_rm_flags,
  input  logic [31:0]            in_rm_seq,
  input  logic [7:0]             in_rm_retrans_id,
  input  logic [RECEIVER_ID_W-1:0] in_rm_receiver_id,
  input  logic [TS_WIDTH-1:0]    in_rm_tsval,
  input  logic [TS_WIDTH-1:0]    in_rm_tsecr,

  // Non-PEEL traffic should continue to the normal host path.
  output logic                   out_host_valid,
  output logic                   out_drop_valid,

  // Abstract TX port toward the network.
  output logic                   tx_req_valid,
  input  logic                   tx_req_ready,
  output logic                   tx_is_multicast,
  output logic [47:0]            tx_dst_mac,
  output logic [31:0]            tx_dst_ip,
  output logic [15:0]            tx_dst_port,
  output logic [15:0]            tx_src_port,
  output logic [15:0]            tx_flags,
  output logic [31:0]            tx_seq,
  output logic [7:0]             tx_retrans_id,
  output logic [7:0]             tx_receiver_id,
  output logic [TS_WIDTH-1:0]    tx_tsval,
  output logic [TS_WIDTH-1:0]    tx_tsecr,

  output logic                   session_active,
  output logic                   in_linger
);

  logic                   pd_out_multicast_valid;
  logic [31:0]            pd_out_src_ip;
  logic [15:0]            pd_out_src_port;
  logic                   pd_out_rm_hdr_valid;
  logic                   pd_out_rm_checksum_ok;
  logic [15:0]            pd_out_rm_flags;
  logic [31:0]            pd_out_rm_seq;
  logic [TS_WIDTH-1:0]    pd_out_rm_tsval;
  logic [7:0]             pd_out_rm_retrans_id;

  udp_packet_director u_pkt_dir (
    .clk               (clk),
    .rst_n             (rst_n),
    .in_valid          (in_valid),
    .in_is_ipv4        (in_is_ipv4),
    .in_is_udp         (in_is_udp),
    .in_ip_dscp        (in_ip_dscp),
    .in_dst_ip         (in_dst_ip),
    .in_src_ip         (in_src_ip),
    .in_dst_port       (in_dst_port),
    .in_src_port       (in_src_port),
    .in_rm_hdr_valid   (in_rm_hdr_valid),
    .in_rm_checksum_ok (in_rm_checksum_ok),
    .in_rm_flags       (in_rm_flags),
    .in_rm_seq         (in_rm_seq),
    .in_rm_retrans_id  (in_rm_retrans_id),
    .in_rm_receiver_id (in_rm_receiver_id),
    .in_rm_tsval       (in_rm_tsval),
    .in_rm_tsecr       (in_rm_tsecr),
    .out_multicast_valid(pd_out_multicast_valid),
    .out_unicast_valid (),
    .out_host_valid    (out_host_valid),
    .out_drop_valid    (out_drop_valid),
    .out_ip_dscp       (),
    .out_src_ip        (pd_out_src_ip),
    .out_src_port      (pd_out_src_port),
    .out_dst_ip        (),
    .out_dst_port      (),
    .out_rm_hdr_valid  (pd_out_rm_hdr_valid),
    .out_rm_checksum_ok(pd_out_rm_checksum_ok),
    .out_rm_flags      (pd_out_rm_flags),
    .out_rm_seq        (pd_out_rm_seq),
    .out_rm_retrans_id (pd_out_rm_retrans_id),
    .out_rm_receiver_id(),
    .out_rm_tsval      (pd_out_rm_tsval),
    .out_rm_tsecr      ()
  );

  rm_transport_receiver #(
    .MAX_COHORT(MAX_COHORT),
    .TS_WIDTH  (TS_WIDTH)
  ) u_receiver (
    .clk              (clk),
    .rst_n            (rst_n),
    .tick_1ms         (tick_1ms),
    .cfg_receiver_id  (cfg_receiver_id),
    .cfg_receiver_port(cfg_receiver_port),
    .cfg_linger_ms    (cfg_linger_ms),
    .rx_mcast_valid   (pd_out_multicast_valid),
    .rx_rm_hdr_valid  (pd_out_rm_hdr_valid),
    .rx_rm_checksum_ok(pd_out_rm_checksum_ok),
    .rx_src_ip        (pd_out_src_ip),
    .rx_src_port      (pd_out_src_port),
    .rx_flags         (pd_out_rm_flags),
    .rx_seq           (pd_out_rm_seq),
    .rx_retrans_id    (pd_out_rm_retrans_id),
    .rx_tsval         (pd_out_rm_tsval),
    .tx_req_valid     (tx_req_valid),
    .tx_req_ready     (tx_req_ready),
    .tx_is_multicast  (tx_is_multicast),
    .tx_dst_mac       (tx_dst_mac),
    .tx_dst_ip        (tx_dst_ip),
    .tx_dst_port      (tx_dst_port),
    .tx_src_port      (tx_src_port),
    .tx_flags         (tx_flags),
    .tx_seq           (tx_seq),
    .tx_retrans_id    (tx_retrans_id),
    .tx_receiver_id   (tx_receiver_id),
    .tx_tsval         (tx_tsval),
    .tx_tsecr         (tx_tsecr),
    .session_active   (session_active),
    .in_linger        (in_linger)
  );

endmodule
