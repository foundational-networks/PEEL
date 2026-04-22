`timescale 1ns/1ps
import rm_proto_pkg::*;

module rm_mcast_handshake_receiver #(
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

  input  logic                   rx_mcast_valid,
  input  logic                   rx_rm_hdr_valid,
  input  logic                   rx_rm_checksum_ok,
  input  logic [31:0]            rx_src_ip,
  input  logic [15:0]            rx_src_port,
  input  logic [15:0]            rx_flags,
  input  logic [31:0]            rx_seq,
  input  logic [7:0]             rx_retrans_id,
  input  logic [TS_WIDTH-1:0]    rx_tsval,

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

  rm_transport_receiver #(
    .MAX_COHORT(MAX_COHORT),
    .TS_WIDTH  (TS_WIDTH)
  ) u_impl (
    .clk              (clk),
    .rst_n            (rst_n),
    .tick_1ms         (tick_1ms),
    .cfg_receiver_id  (cfg_receiver_id),
    .cfg_receiver_port(cfg_receiver_port),
    .cfg_linger_ms    (cfg_linger_ms),
    .rx_mcast_valid   (rx_mcast_valid),
    .rx_rm_hdr_valid  (rx_rm_hdr_valid),
    .rx_rm_checksum_ok(rx_rm_checksum_ok),
    .rx_src_ip        (rx_src_ip),
    .rx_src_port      (rx_src_port),
    .rx_flags         (rx_flags),
    .rx_seq           (rx_seq),
    .rx_retrans_id    (rx_retrans_id),
    .rx_tsval         (rx_tsval),
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
