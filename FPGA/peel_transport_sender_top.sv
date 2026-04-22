`timescale 1ns/1ps
import rm_proto_pkg::*;

module peel_transport_sender_top #(
  parameter int MAX_COHORT    = 16,
  parameter int TS_WIDTH      = 32,
  parameter int RTO_WIDTH     = 16,
  parameter int RETRY_WIDTH   = 8,
  parameter int EXPECTED_W    = (MAX_COHORT > 1) ? $clog2(MAX_COHORT+1) : 1,
  parameter int RECEIVER_ID_W = (MAX_COHORT > 1) ? $clog2(MAX_COHORT)   : 1
)(
  input  logic                   clk,
  input  logic                   rst_n,
  input  logic                   tick_1ms,
  input  logic                   start,

  input  logic [15:0]            cfg_sender_port,
  input  logic [15:0]            cfg_mcast_port,
  input  logic [31:0]            cfg_mcast_ip,
  input  logic [47:0]            cfg_mcast_mac,
  input  logic [EXPECTED_W-1:0]  cfg_expected_rcv,
  input  logic [31:0]            cfg_total_packets,
  input  logic [RTO_WIDTH-1:0]   cfg_hs_rto_ms,
  input  logic [RTO_WIDTH-1:0]   cfg_tx_rto_ms,
  input  logic [RETRY_WIDTH-1:0] cfg_hs_max_retries,
  input  logic [RETRY_WIDTH-1:0] cfg_tx_max_retries,

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

  // Single abstract TX port toward the network.
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

  output logic                   handshake_busy,
  output logic                   transport_busy,
  output logic                   done,
  output logic                   fail
);

  logic                   pd_out_multicast_valid;
  logic                   pd_out_unicast_valid;
  logic [31:0]            pd_out_src_ip;
  logic [15:0]            pd_out_src_port;
  logic [31:0]            pd_out_dst_ip;
  logic [15:0]            pd_out_dst_port;
  logic                   pd_out_rm_hdr_valid;
  logic                   pd_out_rm_checksum_ok;
  logic [15:0]            pd_out_rm_flags;
  logic [31:0]            pd_out_rm_seq;
  logic [7:0]             pd_out_rm_retrans_id;
  logic [7:0]             pd_out_rm_receiver_id;
  logic [TS_WIDTH-1:0]    pd_out_rm_tsval;
  logic [TS_WIDTH-1:0]    pd_out_rm_tsecr;

  logic hs_tx_req_valid;
  logic hs_tx_req_ready;
  logic hs_tx_is_multicast;
  logic [47:0] hs_tx_dst_mac;
  logic [31:0] hs_tx_dst_ip;
  logic [15:0] hs_tx_dst_port;
  logic [15:0] hs_tx_src_port;
  logic [15:0] hs_tx_flags;
  logic [31:0] hs_tx_seq;
  logic [7:0]  hs_tx_retrans_id;
  logic [7:0]  hs_tx_receiver_id;
  logic [TS_WIDTH-1:0] hs_tx_tsval;
  logic [TS_WIDTH-1:0] hs_tx_tsecr;
  logic hs_done;
  logic hs_fail;
  logic hs_session_start_pulse;

  logic txs_tx_req_valid;
  logic txs_tx_req_ready;
  logic txs_tx_is_multicast;
  logic [47:0] txs_tx_dst_mac;
  logic [31:0] txs_tx_dst_ip;
  logic [15:0] txs_tx_dst_port;
  logic [15:0] txs_tx_src_port;
  logic [15:0] txs_tx_flags;
  logic [31:0] txs_tx_seq;
  logic [7:0]  txs_tx_retrans_id;
  logic [7:0]  txs_tx_receiver_id;
  logic [TS_WIDTH-1:0] txs_tx_tsval;
  logic [TS_WIDTH-1:0] txs_tx_tsecr;
  logic txs_done;
  logic txs_fail;

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
    .out_multicast_valid(),
    .out_unicast_valid (pd_out_unicast_valid),
    .out_host_valid    (out_host_valid),
    .out_drop_valid    (out_drop_valid),
    .out_ip_dscp       (),
    .out_src_ip        (pd_out_src_ip),
    .out_src_port      (pd_out_src_port),
    .out_dst_ip        (pd_out_dst_ip),
    .out_dst_port      (pd_out_dst_port),
    .out_rm_hdr_valid  (pd_out_rm_hdr_valid),
    .out_rm_checksum_ok(pd_out_rm_checksum_ok),
    .out_rm_flags      (pd_out_rm_flags),
    .out_rm_seq        (pd_out_rm_seq),
    .out_rm_retrans_id (pd_out_rm_retrans_id),
    .out_rm_receiver_id(pd_out_rm_receiver_id),
    .out_rm_tsval      (pd_out_rm_tsval),
    .out_rm_tsecr      (pd_out_rm_tsecr)
  );

  rm_mcast_handshake_sender #(
    .MAX_COHORT(MAX_COHORT),
    .TS_WIDTH  (TS_WIDTH)
  ) u_handshake (
    .clk               (clk),
    .rst_n             (rst_n),
    .tick_1ms          (tick_1ms),
    .start             (start),
    .cfg_sender_port   (cfg_sender_port),
    .cfg_mcast_port    (cfg_mcast_port),
    .cfg_mcast_ip      (cfg_mcast_ip),
    .cfg_mcast_mac     (cfg_mcast_mac),
    .cfg_expected_rcv  (cfg_expected_rcv),
    .cfg_rto_ms        (cfg_hs_rto_ms),
    .cfg_max_retries   (cfg_hs_max_retries),
    .rx_valid          (pd_out_unicast_valid),
    .rx_rm_hdr_valid   (pd_out_rm_hdr_valid),
    .rx_rm_checksum_ok (pd_out_rm_checksum_ok),
    .rx_flags          (pd_out_rm_flags),
    .rx_retrans_id     (pd_out_rm_retrans_id),
    .rx_receiver_id    (pd_out_rm_receiver_id[RECEIVER_ID_W-1:0]),
    .rx_tsecr          (pd_out_rm_tsecr),
    .tx_req_valid      (hs_tx_req_valid),
    .tx_req_ready      (hs_tx_req_ready),
    .tx_is_multicast   (hs_tx_is_multicast),
    .tx_dst_mac        (hs_tx_dst_mac),
    .tx_dst_ip         (hs_tx_dst_ip),
    .tx_dst_port       (hs_tx_dst_port),
    .tx_src_port       (hs_tx_src_port),
    .tx_flags          (hs_tx_flags),
    .tx_seq            (hs_tx_seq),
    .tx_retrans_id     (hs_tx_retrans_id),
    .tx_receiver_id    (hs_tx_receiver_id),
    .tx_tsval          (hs_tx_tsval),
    .tx_tsecr          (hs_tx_tsecr),
    .busy              (handshake_busy),
    .done              (hs_done),
    .fail              (hs_fail),
    .session_start_pulse(hs_session_start_pulse),
    .cohort_count      ()
  );

  rm_stopwait_transport_sender #(
    .MAX_COHORT(MAX_COHORT),
    .TS_WIDTH  (TS_WIDTH)
  ) u_transport (
    .clk               (clk),
    .rst_n             (rst_n),
    .tick_1ms          (tick_1ms),
    .start             (hs_session_start_pulse),
    .cfg_sender_port   (cfg_sender_port),
    .cfg_mcast_port    (cfg_mcast_port),
    .cfg_mcast_ip      (cfg_mcast_ip),
    .cfg_mcast_mac     (cfg_mcast_mac),
    .cfg_expected_rcv  (cfg_expected_rcv),
    .cfg_total_packets (cfg_total_packets),
    .cfg_rto_ms        (cfg_tx_rto_ms),
    .cfg_max_retries   (cfg_tx_max_retries),
    .rx_valid          (pd_out_unicast_valid),
    .rx_rm_hdr_valid   (pd_out_rm_hdr_valid),
    .rx_rm_checksum_ok (pd_out_rm_checksum_ok),
    .rx_flags          (pd_out_rm_flags),
    .rx_seq            (pd_out_rm_seq),
    .rx_retrans_id     (pd_out_rm_retrans_id),
    .rx_receiver_id    (pd_out_rm_receiver_id[RECEIVER_ID_W-1:0]),
    .rx_tsecr          (pd_out_rm_tsecr),
    .tx_req_valid      (txs_tx_req_valid),
    .tx_req_ready      (txs_tx_req_ready),
    .tx_is_multicast   (txs_tx_is_multicast),
    .tx_dst_mac        (txs_tx_dst_mac),
    .tx_dst_ip         (txs_tx_dst_ip),
    .tx_dst_port       (txs_tx_dst_port),
    .tx_src_port       (txs_tx_src_port),
    .tx_flags          (txs_tx_flags),
    .tx_seq            (txs_tx_seq),
    .tx_retrans_id     (txs_tx_retrans_id),
    .tx_receiver_id    (txs_tx_receiver_id),
    .tx_tsval          (txs_tx_tsval),
    .tx_tsecr          (txs_tx_tsecr),
    .busy              (transport_busy),
    .done              (txs_done),
    .fail              (txs_fail),
    .current_seq       (),
    .sending_fin       ()
  );

  // Handshake messages have priority over transport messages.
  assign tx_req_valid    = hs_tx_req_valid | txs_tx_req_valid;
  assign tx_is_multicast = hs_tx_req_valid ? hs_tx_is_multicast : txs_tx_is_multicast;
  assign tx_dst_mac      = hs_tx_req_valid ? hs_tx_dst_mac      : txs_tx_dst_mac;
  assign tx_dst_ip       = hs_tx_req_valid ? hs_tx_dst_ip       : txs_tx_dst_ip;
  assign tx_dst_port     = hs_tx_req_valid ? hs_tx_dst_port     : txs_tx_dst_port;
  assign tx_src_port     = hs_tx_req_valid ? hs_tx_src_port     : txs_tx_src_port;
  assign tx_flags        = hs_tx_req_valid ? hs_tx_flags        : txs_tx_flags;
  assign tx_seq          = hs_tx_req_valid ? hs_tx_seq          : txs_tx_seq;
  assign tx_retrans_id   = hs_tx_req_valid ? hs_tx_retrans_id   : txs_tx_retrans_id;
  assign tx_receiver_id  = hs_tx_req_valid ? hs_tx_receiver_id  : txs_tx_receiver_id;
  assign tx_tsval        = hs_tx_req_valid ? hs_tx_tsval        : txs_tx_tsval;
  assign tx_tsecr        = hs_tx_req_valid ? hs_tx_tsecr        : txs_tx_tsecr;

  assign hs_tx_req_ready  = tx_req_ready && hs_tx_req_valid;
  assign txs_tx_req_ready = tx_req_ready && !hs_tx_req_valid && txs_tx_req_valid;

  assign done = txs_done;
  assign fail = hs_fail | txs_fail;

endmodule
