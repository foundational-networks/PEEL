`timescale 1ns/1ps
import rm_proto_pkg::*;

module rm_mcast_handshake_sender #(
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
  input  logic [RTO_WIDTH-1:0]   cfg_rto_ms,
  input  logic [RETRY_WIDTH-1:0] cfg_max_retries,

  // inbound unicast replies from receivers
  input  logic                   rx_valid,
  input  logic                   rx_rm_hdr_valid,
  input  logic                   rx_rm_checksum_ok,
  input  logic [15:0]            rx_flags,
  input  logic [7:0]             rx_retrans_id,
  input  logic [RECEIVER_ID_W-1:0] rx_receiver_id,
  input  logic [TS_WIDTH-1:0]    rx_tsecr,

  // abstract TX request interface (multicast SYN / START)
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

  output logic                   busy,
  output logic                   done,
  output logic                   fail,
  output logic                   session_start_pulse,
  output logic [EXPECTED_W-1:0]  cohort_count
);

  typedef enum logic [2:0] {
    ST_IDLE       = 3'd0,
    ST_PREP_SYN   = 3'd1,
    ST_SEND_SYN   = 3'd2,
    ST_WAIT_ACKS  = 3'd3,
    ST_SEND_START = 3'd4,
    ST_DONE_PLS   = 3'd5,
    ST_FAIL_PLS   = 3'd6
  } state_t;

  state_t state, state_n;

  logic [TS_WIDTH-1:0] now_ms;
  logic [TS_WIDTH-1:0] syn_ts;
  logic [TS_WIDTH-1:0] epoch_start_ts;
  logic [RETRY_WIDTH-1:0] attempt;
  logic [7:0] retrans_id;
  logic [EXPECTED_W-1:0] expected_rcv_latched;
  logic [MAX_COHORT-1:0] ack_seen;

  logic timeout_hit;
  logic got_enough;
  logic got_enough_next;
  logic ack_match;
  logic new_ack;

  integer i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      now_ms <= '0;
    end else if (tick_1ms) begin
      now_ms <= now_ms + 1'b1;
    end
  end

  assign timeout_hit = ((now_ms - epoch_start_ts) >= cfg_rto_ms);
  assign got_enough      = (cohort_count >= expected_rcv_latched) && (expected_rcv_latched != '0);
  assign got_enough_next = ((cohort_count + (new_ack ? 1'b1 : 1'b0)) >= expected_rcv_latched) && (expected_rcv_latched != '0);

  assign ack_match = rx_valid && rx_rm_hdr_valid && rx_rm_checksum_ok &&
                     (((rx_flags & (FLG_SYN | FLG_ACK)) == (FLG_SYN | FLG_ACK))) &&
                     (rx_retrans_id == retrans_id) &&
                     (rx_tsecr == syn_ts) &&
                     (rx_receiver_id < MAX_COHORT);

  assign new_ack   = ack_match && !ack_seen[rx_receiver_id];

  always_comb begin
    state_n = state;
    unique case (state)
      ST_IDLE: begin
        if (start) begin
          state_n = ST_PREP_SYN;
        end
      end

      ST_PREP_SYN: begin
        state_n = ST_SEND_SYN;
      end

      ST_SEND_SYN: begin
        if (tx_req_ready) begin
          state_n = ST_WAIT_ACKS;
        end
      end

      ST_WAIT_ACKS: begin
        if (got_enough_next) begin
          state_n = ST_SEND_START;
        end else if (timeout_hit) begin
          if (attempt >= cfg_max_retries) begin
            state_n = ST_FAIL_PLS;
          end else begin
            state_n = ST_PREP_SYN;
          end
        end
      end

      ST_SEND_START: begin
        if (tx_req_ready) begin
          state_n = ST_DONE_PLS;
        end
      end

      ST_DONE_PLS: begin
        state_n = ST_IDLE;
      end

      ST_FAIL_PLS: begin
        state_n = ST_IDLE;
      end

      default: begin
        state_n = ST_IDLE;
      end
    endcase
  end

  always_comb begin
    tx_req_valid      = 1'b0;
    tx_is_multicast   = 1'b1;
    tx_dst_mac        = cfg_mcast_mac;
    tx_dst_ip         = cfg_mcast_ip;
    tx_dst_port       = cfg_mcast_port;
    tx_src_port       = cfg_sender_port;
    tx_flags          = 16'd0;
    tx_seq            = 32'd0;
    tx_retrans_id     = retrans_id;
    tx_receiver_id    = 8'd0;
    tx_tsval          = syn_ts;
    tx_tsecr          = '0;

    case (state)
      ST_SEND_SYN: begin
        tx_req_valid  = 1'b1;
        tx_flags      = FLG_SYN;
        tx_tsval      = syn_ts;
      end

      ST_SEND_START: begin
        tx_req_valid  = 1'b1;
        tx_flags      = FLG_START;
        tx_retrans_id = 8'd0;
        tx_tsval      = now_ms;
      end

      default: begin
      end
    endcase
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state               <= ST_IDLE;
      syn_ts              <= '0;
      epoch_start_ts      <= '0;
      attempt             <= '0;
      retrans_id          <= 8'd1;
      expected_rcv_latched<= '0;
      ack_seen            <= '0;
      cohort_count        <= '0;
      busy                <= 1'b0;
      done                <= 1'b0;
      fail                <= 1'b0;
      session_start_pulse <= 1'b0;
    end else begin
      state               <= state_n;
      busy                <= (state_n != ST_IDLE);
      done                <= 1'b0;
      fail                <= 1'b0;
      session_start_pulse <= 1'b0;

      case (state)
        ST_IDLE: begin
          if (start) begin
            if (cfg_expected_rcv > MAX_COHORT[EXPECTED_W-1:0]) begin
              expected_rcv_latched <= MAX_COHORT[EXPECTED_W-1:0];
            end else begin
              expected_rcv_latched <= cfg_expected_rcv;
            end
            attempt      <= '0;
            retrans_id   <= 8'd1;
            ack_seen     <= '0;
            cohort_count <= '0;
          end
        end

        ST_PREP_SYN: begin
          syn_ts         <= now_ms;
          epoch_start_ts <= now_ms;
          ack_seen       <= '0;
          cohort_count   <= '0;
        end

        ST_WAIT_ACKS: begin
          if (new_ack) begin
            ack_seen[rx_receiver_id] <= 1'b1;
            cohort_count             <= cohort_count + 1'b1;
          end

          if (timeout_hit && !got_enough_next) begin
            if (attempt < cfg_max_retries) begin
              attempt    <= attempt + 1'b1;
              retrans_id <= retrans_id + 1'b1;
            end
          end
        end

        ST_SEND_START: begin
          // no per-state update needed
        end

        ST_DONE_PLS: begin
          done                <= 1'b1;
          session_start_pulse <= 1'b1;
          busy                <= 1'b0;
        end

        ST_FAIL_PLS: begin
          fail <= 1'b1;
          busy <= 1'b0;
        end

        default: begin
        end
      endcase
    end
  end

endmodule
