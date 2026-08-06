`timescale 1ns/1ps
import rm_proto_pkg::*;

module rm_transport_receiver #(
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

  // Inbound multicast control/data packets from packet director control path.
  input  logic                   rx_mcast_valid,
  input  logic                   rx_rm_hdr_valid,
  input  logic                   rx_rm_checksum_ok,
  input  logic [31:0]            rx_src_ip,
  input  logic [15:0]            rx_src_port,
  input  logic [15:0]            rx_flags,
  input  logic [31:0]            rx_seq,
  input  logic [7:0]             rx_retrans_id,
  input  logic [TS_WIDTH-1:0]    rx_tsval,

  // Outbound unicast ACK request.
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

  logic [TS_WIDTH-1:0] now_ms;
  logic [TS_WIDTH-1:0] linger_start_ms;

  logic pending;
  logic [31:0]         pend_dst_ip;
  logic [15:0]         pend_dst_port;
  logic [15:0]         pend_flags;
  logic [31:0]         pend_seq;
  logic [7:0]          pend_retrans_id;
  logic [TS_WIDTH-1:0] pend_tsecr;

  logic is_syn;
  logic is_start;
  logic is_data;
  logic is_fin;
  logic rx_ok;

  // Next-pending staging so an ACK being consumed in the same cycle as a new
  // inbound packet does not get dropped.
  logic                   pend_set;
  logic [31:0]            pend_set_dst_ip;
  logic [15:0]            pend_set_dst_port;
  logic [15:0]            pend_set_flags;
  logic [31:0]            pend_set_seq;
  logic [7:0]             pend_set_retrans_id;
  logic [TS_WIDTH-1:0]    pend_set_tsecr;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      now_ms <= '0;
    end else if (tick_1ms) begin
      now_ms <= now_ms + 1'b1;
    end
  end

  assign rx_ok    = rx_mcast_valid && rx_rm_hdr_valid && rx_rm_checksum_ok;
  assign is_syn   = ((rx_flags & FLG_SYN)   != 0);
  assign is_start = ((rx_flags & FLG_START) != 0);
  assign is_data  = ((rx_flags & FLG_DATA)  != 0);
  assign is_fin   = ((rx_flags & FLG_FIN)   != 0);

  always_comb begin
    pend_set            = 1'b0;
    pend_set_dst_ip     = rx_src_ip;
    pend_set_dst_port   = rx_src_port;
    pend_set_flags      = '0;
    pend_set_seq        = rx_seq;
    pend_set_retrans_id = rx_retrans_id;
    pend_set_tsecr      = rx_tsval;

    if (rx_ok) begin
      if (is_syn) begin
        pend_set       = 1'b1;
        pend_set_flags = (FLG_SYN | FLG_ACK);
      end else if (is_data && session_active) begin
        pend_set       = 1'b1;
        pend_set_flags = (FLG_DATA | FLG_ACK);
      end else if (is_fin && (session_active || in_linger)) begin
        pend_set       = 1'b1;
        pend_set_flags = (FLG_FIN | FLG_ACK);
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pending         <= 1'b0;
      pend_dst_ip     <= '0;
      pend_dst_port   <= '0;
      pend_flags      <= '0;
      pend_seq        <= '0;
      pend_retrans_id <= '0;
      pend_tsecr      <= '0;
      session_active  <= 1'b0;
      in_linger       <= 1'b0;
      linger_start_ms <= '0;
    end else begin
      if (in_linger && ((now_ms - linger_start_ms) >= cfg_linger_ms)) begin
        in_linger      <= 1'b0;
        session_active <= 1'b0;
      end

      // Default behavior: clear pending only if the current ACK was consumed.
      if (tx_req_valid && tx_req_ready) begin
        pending <= 1'b0;
      end

      if (rx_ok) begin
        if (is_start) begin
          session_active <= 1'b1;
          in_linger      <= 1'b0;
        end

        if (is_fin && (session_active || in_linger)) begin
          session_active  <= 1'b0;
          in_linger       <= 1'b1;
          linger_start_ms <= now_ms;
        end
      end

      // Highest priority: capture a newly generated ACK slot, even if the
      // previous slot was consumed in this same cycle.
      if (pend_set) begin
        pending         <= 1'b1;
        pend_dst_ip     <= pend_set_dst_ip;
        pend_dst_port   <= pend_set_dst_port;
        pend_flags      <= pend_set_flags;
        pend_seq        <= pend_set_seq;
        pend_retrans_id <= pend_set_retrans_id;
        pend_tsecr      <= pend_set_tsecr;
      end
    end
  end

  always_comb begin
    tx_req_valid    = pending;
    tx_is_multicast = 1'b0;
    tx_dst_mac      = 48'd0;
    tx_dst_ip       = pend_dst_ip;
    tx_dst_port     = pend_dst_port;
    tx_src_port     = cfg_receiver_port;
    tx_flags        = pend_flags;
    tx_seq          = pend_seq;
    tx_retrans_id   = pend_retrans_id;
    tx_receiver_id  = cfg_receiver_id;
    tx_tsval        = now_ms;
    tx_tsecr        = pend_tsecr;
  end

endmodule
