`timescale 1ns/1ps
import rm_proto_pkg::*;

// Implementation-oriented top with very small top-level I/O count.
// Purpose:
//   - keep the multicast sender/receiver path functionally present
//   - avoid exposing every parser/TX metadata signal as a package pin
//   - allow synthesis/implementation/utilization on a generic device
//
// Suggested use:
//   1) Set this module as the implementation top.
//   2) Add peel_transport_area_top.xdc (clock only).
//   3) Run synthesis / implementation.
//   4) Use report_utilization -hierarchical to inspect area by module.
module peel_transport_area_top #(
  parameter int CLK_FREQ_HZ   = 100_000_000,
  parameter int MAX_COHORT    = 16,
  parameter int NUM_RCV       = 2,
  parameter int TS_WIDTH      = 32,
  parameter int RTO_WIDTH     = 16,
  parameter int RETRY_WIDTH   = 8,
  parameter int EXPECTED_W    = (MAX_COHORT > 1) ? $clog2(MAX_COHORT+1) : 1,
  parameter int RECEIVER_ID_W = (MAX_COHORT > 1) ? $clog2(MAX_COHORT)   : 1,
  parameter int TICK_DIV      = (CLK_FREQ_HZ / 1000)
)(
  input  logic        clk,
  input  logic        rst_n,
  input  logic        start,

  output logic        done,
  output logic        fail,
  output logic [31:0] status
);

  localparam logic [31:0] SENDER_IP   = 32'h0A000001;
  localparam logic [31:0] MCAST_IP    = 32'hE0010101;
  localparam logic [47:0] MCAST_MAC   = 48'h01005E010101;
  localparam logic [15:0] SENDER_PORT = 16'd40000;
  localparam logic [15:0] MCAST_PORT  = 16'd40001;
  localparam logic [15:0] LINGER_MS   = 16'd500;

  // -----------------------------
  // 1ms tick generator
  // -----------------------------
  localparam int TICK_W = (TICK_DIV > 1) ? $clog2(TICK_DIV) : 1;
  logic [TICK_W-1:0] tick_div_ctr;
  logic              tick_1ms;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      tick_div_ctr <= '0;
      tick_1ms     <= 1'b0;
    end else if (tick_div_ctr == TICK_DIV-1) begin
      tick_div_ctr <= '0;
      tick_1ms     <= 1'b1;
    end else begin
      tick_div_ctr <= tick_div_ctr + 1'b1;
      tick_1ms     <= 1'b0;
    end
  end

  // -----------------------------
  // Sender top <-> internal network
  // -----------------------------
  logic                   s_out_host_valid;
  logic                   s_out_drop_valid;
  logic                   s_tx_req_valid;
  logic                   s_tx_req_ready;
  logic                   s_tx_is_multicast;
  logic [47:0]            s_tx_dst_mac;
  logic [31:0]            s_tx_dst_ip;
  logic [15:0]            s_tx_dst_port;
  logic [15:0]            s_tx_src_port;
  logic [15:0]            s_tx_flags;
  logic [31:0]            s_tx_seq;
  logic [7:0]             s_tx_retrans_id;
  logic [7:0]             s_tx_receiver_id;
  logic [TS_WIDTH-1:0]    s_tx_tsval;
  logic [TS_WIDTH-1:0]    s_tx_tsecr;
  logic                   s_handshake_busy;
  logic                   s_transport_busy;
  logic                   s_done;
  logic                   s_fail;

  // ACK path into sender packet director.
  logic                   s_in_valid;
  logic                   s_in_is_ipv4;
  logic                   s_in_is_udp;
  logic [5:0]             s_in_ip_dscp;
  logic [31:0]            s_in_dst_ip;
  logic [31:0]            s_in_src_ip;
  logic [15:0]            s_in_dst_port;
  logic [15:0]            s_in_src_port;
  logic                   s_in_rm_hdr_valid;
  logic                   s_in_rm_checksum_ok;
  logic [15:0]            s_in_rm_flags;
  logic [31:0]            s_in_rm_seq;
  logic [7:0]             s_in_rm_retrans_id;
  logic [RECEIVER_ID_W-1:0] s_in_rm_receiver_id;
  logic [TS_WIDTH-1:0]    s_in_rm_tsval;
  logic [TS_WIDTH-1:0]    s_in_rm_tsecr;

  (* keep_hierarchy = "yes" *) peel_transport_sender_top #(
    .MAX_COHORT   (MAX_COHORT),
    .TS_WIDTH     (TS_WIDTH),
    .RTO_WIDTH    (RTO_WIDTH),
    .RETRY_WIDTH  (RETRY_WIDTH)
  ) u_sender (
    .clk               (clk),
    .rst_n             (rst_n),
    .tick_1ms          (tick_1ms),
    .start             (start),
    .cfg_sender_port   (SENDER_PORT),
    .cfg_mcast_port    (MCAST_PORT),
    .cfg_mcast_ip      (MCAST_IP),
    .cfg_mcast_mac     (MCAST_MAC),
    .cfg_expected_rcv  (EXPECTED_W'(NUM_RCV)),
    .cfg_total_packets (32'd16),
    .cfg_hs_rto_ms     (RTO_WIDTH'(16)),
    .cfg_tx_rto_ms     (RTO_WIDTH'(16)),
    .cfg_hs_max_retries(RETRY_WIDTH'(8)),
    .cfg_tx_max_retries(RETRY_WIDTH'(8)),
    .in_valid          (s_in_valid),
    .in_is_ipv4        (s_in_is_ipv4),
    .in_is_udp         (s_in_is_udp),
    .in_ip_dscp        (s_in_ip_dscp),
    .in_dst_ip         (s_in_dst_ip),
    .in_src_ip         (s_in_src_ip),
    .in_dst_port       (s_in_dst_port),
    .in_src_port       (s_in_src_port),
    .in_rm_hdr_valid   (s_in_rm_hdr_valid),
    .in_rm_checksum_ok (s_in_rm_checksum_ok),
    .in_rm_flags       (s_in_rm_flags),
    .in_rm_seq         (s_in_rm_seq),
    .in_rm_retrans_id  (s_in_rm_retrans_id),
    .in_rm_receiver_id (s_in_rm_receiver_id),
    .in_rm_tsval       (s_in_rm_tsval),
    .in_rm_tsecr       (s_in_rm_tsecr),
    .out_host_valid    (s_out_host_valid),
    .out_drop_valid    (s_out_drop_valid),
    .tx_req_valid      (s_tx_req_valid),
    .tx_req_ready      (s_tx_req_ready),
    .tx_is_multicast   (s_tx_is_multicast),
    .tx_dst_mac        (s_tx_dst_mac),
    .tx_dst_ip         (s_tx_dst_ip),
    .tx_dst_port       (s_tx_dst_port),
    .tx_src_port       (s_tx_src_port),
    .tx_flags          (s_tx_flags),
    .tx_seq            (s_tx_seq),
    .tx_retrans_id     (s_tx_retrans_id),
    .tx_receiver_id    (s_tx_receiver_id),
    .tx_tsval          (s_tx_tsval),
    .tx_tsecr          (s_tx_tsecr),
    .handshake_busy    (s_handshake_busy),
    .transport_busy    (s_transport_busy),
    .done              (s_done),
    .fail              (s_fail)
  );

  assign s_tx_req_ready = 1'b1;

  // -----------------------------
  // Receiver tops <-> internal network
  // -----------------------------
  logic [NUM_RCV-1:0]                     r_out_host_valid;
  logic [NUM_RCV-1:0]                     r_out_drop_valid;
  logic [NUM_RCV-1:0]                     r_tx_req_valid;
  logic [NUM_RCV-1:0]                     r_tx_req_ready;
  logic [NUM_RCV-1:0]                     r_tx_is_multicast;
  logic [NUM_RCV-1:0][47:0]              r_tx_dst_mac;
  logic [NUM_RCV-1:0][31:0]              r_tx_dst_ip;
  logic [NUM_RCV-1:0][15:0]              r_tx_dst_port;
  logic [NUM_RCV-1:0][15:0]              r_tx_src_port;
  logic [NUM_RCV-1:0][15:0]              r_tx_flags;
  logic [NUM_RCV-1:0][31:0]              r_tx_seq;
  logic [NUM_RCV-1:0][7:0]               r_tx_retrans_id;
  logic [NUM_RCV-1:0][7:0]               r_tx_receiver_id;
  logic [NUM_RCV-1:0][TS_WIDTH-1:0]      r_tx_tsval;
  logic [NUM_RCV-1:0][TS_WIDTH-1:0]      r_tx_tsecr;
  logic [NUM_RCV-1:0]                     r_session_active;
  logic [NUM_RCV-1:0]                     r_in_linger;

  genvar gi;
  generate
    for (gi = 0; gi < NUM_RCV; gi++) begin : GEN_RCV
      localparam logic [31:0] RCV_IP = 32'h0A000100 + gi;
      (* keep_hierarchy = "yes" *) peel_transport_receiver_top #(
        .MAX_COHORT(MAX_COHORT),
        .TS_WIDTH  (TS_WIDTH)
      ) u_rcv (
        .clk               (clk),
        .rst_n             (rst_n),
        .tick_1ms          (tick_1ms),
        .cfg_receiver_id   (RECEIVER_ID_W'(gi)),
        .cfg_receiver_port (16'(SENDER_PORT + 16'd100 + gi)),
        .cfg_linger_ms     (LINGER_MS),
        .in_valid          (s_tx_req_valid),
        .in_is_ipv4        (1'b1),
        .in_is_udp         (1'b1),
        .in_ip_dscp        (RM_CTRL_DSCP),
        .in_dst_ip         (s_tx_dst_ip),
        .in_src_ip         (SENDER_IP),
        .in_dst_port       (s_tx_dst_port),
        .in_src_port       (s_tx_src_port),
        .in_rm_hdr_valid   (1'b1),
        .in_rm_checksum_ok (1'b1),
        .in_rm_flags       (s_tx_flags),
        .in_rm_seq         (s_tx_seq),
        .in_rm_retrans_id  (s_tx_retrans_id),
        .in_rm_receiver_id (s_tx_receiver_id[RECEIVER_ID_W-1:0]),
        .in_rm_tsval       (s_tx_tsval),
        .in_rm_tsecr       (s_tx_tsecr),
        .out_host_valid    (r_out_host_valid[gi]),
        .out_drop_valid    (r_out_drop_valid[gi]),
        .tx_req_valid      (r_tx_req_valid[gi]),
        .tx_req_ready      (r_tx_req_ready[gi]),
        .tx_is_multicast   (r_tx_is_multicast[gi]),
        .tx_dst_mac        (r_tx_dst_mac[gi]),
        .tx_dst_ip         (r_tx_dst_ip[gi]),
        .tx_dst_port       (r_tx_dst_port[gi]),
        .tx_src_port       (r_tx_src_port[gi]),
        .tx_flags          (r_tx_flags[gi]),
        .tx_seq            (r_tx_seq[gi]),
        .tx_retrans_id     (r_tx_retrans_id[gi]),
        .tx_receiver_id    (r_tx_receiver_id[gi]),
        .tx_tsval          (r_tx_tsval[gi]),
        .tx_tsecr          (r_tx_tsecr[gi]),
        .session_active    (r_session_active[gi]),
        .in_linger         (r_in_linger[gi])
      );
    end
  endgenerate

  // -----------------------------
  // Simple round-robin arbitration for unicast ACKs back to sender.
  // -----------------------------
  logic [RECEIVER_ID_W-1:0] rr_ptr;
  logic [RECEIVER_ID_W-1:0] grant_idx;
  logic                     ack_valid;
  integer                   k;

  always_comb begin
    ack_valid  = 1'b0;
    grant_idx  = rr_ptr;
    for (k = 0; k < NUM_RCV; k++) begin
      r_tx_req_ready[k] = 1'b0;
    end

    for (k = 0; k < NUM_RCV; k++) begin
      int idx;
      idx = rr_ptr + k;
      if (idx >= NUM_RCV) idx = idx - NUM_RCV;
      if (!ack_valid && r_tx_req_valid[idx]) begin
        ack_valid           = 1'b1;
        grant_idx           = RECEIVER_ID_W'(idx);
        r_tx_req_ready[idx] = 1'b1;
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rr_ptr <= '0;
    end else if (ack_valid) begin
      if (grant_idx == NUM_RCV-1)
        rr_ptr <= '0;
      else
        rr_ptr <= grant_idx + 1'b1;
    end
  end

  // Route granted ACK back into the sender-side packet director.
  always_comb begin
    s_in_valid          = ack_valid;
    s_in_is_ipv4        = 1'b1;
    s_in_is_udp         = 1'b1;
    s_in_ip_dscp        = RM_CTRL_DSCP;
    s_in_dst_ip         = SENDER_IP;
    s_in_src_ip         = 32'h0A000100 + grant_idx;
    s_in_dst_port       = SENDER_PORT;
    s_in_src_port       = 16'(SENDER_PORT + 16'd100 + grant_idx);
    s_in_rm_hdr_valid   = ack_valid;
    s_in_rm_checksum_ok = ack_valid;
    s_in_rm_flags       = ack_valid ? r_tx_flags[grant_idx]        : '0;
    s_in_rm_seq         = ack_valid ? r_tx_seq[grant_idx]          : '0;
    s_in_rm_retrans_id  = ack_valid ? r_tx_retrans_id[grant_idx]   : '0;
    s_in_rm_receiver_id = ack_valid ? r_tx_receiver_id[grant_idx][RECEIVER_ID_W-1:0] : '0;
    s_in_rm_tsval       = ack_valid ? r_tx_tsval[grant_idx]        : '0;
    s_in_rm_tsecr       = ack_valid ? r_tx_tsecr[grant_idx]        : '0;
  end

  assign done = s_done;
  assign fail = s_fail;

  // Small status bus for easy GUI observation without exploding I/Os.
  assign status = {
    8'(rr_ptr),
    6'd0,
    s_transport_busy,
    s_handshake_busy,
    |r_in_linger,
    &r_session_active[NUM_RCV-1:0],
    |r_session_active[NUM_RCV-1:0],
    ack_valid,
    s_tx_req_valid,
    s_fail,
    s_done,
    s_out_drop_valid,
    s_out_host_valid,
    |r_out_drop_valid,
    |r_out_host_valid
  };

endmodule
