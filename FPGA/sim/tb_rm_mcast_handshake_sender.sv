`timescale 1ns/1ps
import rm_proto_pkg::*;

module tb_rm_mcast_handshake_sender;

  localparam int MAX_COHORT    = 4;
  localparam int EXPECTED_W    = $clog2(MAX_COHORT+1);
  localparam int RECEIVER_ID_W = $clog2(MAX_COHORT);

  logic clk;
  logic rst_n;
  logic tick_1ms;
  logic start;
  logic [15:0] cfg_sender_port;
  logic [15:0] cfg_mcast_port;
  logic [31:0] cfg_mcast_ip;
  logic [47:0] cfg_mcast_mac;
  logic [EXPECTED_W-1:0] cfg_expected_rcv;
  logic [15:0] cfg_rto_ms;
  logic [7:0]  cfg_max_retries;

  logic rx_valid;
  logic rx_rm_hdr_valid;
  logic rx_rm_checksum_ok;
  logic [15:0] rx_flags;
  logic [7:0]  rx_retrans_id;
  logic [RECEIVER_ID_W-1:0] rx_receiver_id;
  logic [31:0] rx_tsecr;

  logic tx_req_valid;
  logic tx_req_ready;
  logic tx_is_multicast;
  logic [47:0] tx_dst_mac;
  logic [31:0] tx_dst_ip;
  logic [15:0] tx_dst_port;
  logic [15:0] tx_src_port;
  logic [15:0] tx_flags;
  logic [31:0] tx_seq;
  logic [7:0]  tx_retrans_id;
  logic [7:0]  tx_receiver_id;
  logic [31:0] tx_tsval;
  logic [31:0] tx_tsecr;

  logic busy;
  logic done;
  logic fail;
  logic session_start_pulse;
  logic [EXPECTED_W-1:0] cohort_count;

  int syn_send_count;
  int start_send_count;
  logic [31:0] last_syn_ts;
  logic [7:0]  last_syn_retrans;

  rm_mcast_handshake_sender #(
    .MAX_COHORT(MAX_COHORT)
  ) dut (
    .clk,
    .rst_n,
    .tick_1ms,
    .start,
    .cfg_sender_port,
    .cfg_mcast_port,
    .cfg_mcast_ip,
    .cfg_mcast_mac,
    .cfg_expected_rcv,
    .cfg_rto_ms,
    .cfg_max_retries,
    .rx_valid,
    .rx_rm_hdr_valid,
    .rx_rm_checksum_ok,
    .rx_flags,
    .rx_retrans_id,
    .rx_receiver_id,
    .rx_tsecr,
    .tx_req_valid,
    .tx_req_ready,
    .tx_is_multicast,
    .tx_dst_mac,
    .tx_dst_ip,
    .tx_dst_port,
    .tx_src_port,
    .tx_flags,
    .tx_seq,
    .tx_retrans_id,
    .tx_receiver_id,
    .tx_tsval,
    .tx_tsecr,
    .busy,
    .done,
    .fail,
    .session_start_pulse,
    .cohort_count
  );

  always #5 clk = ~clk;

  // For fast simulation, treat every cycle as a 1ms tick.
  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) tick_1ms <= 1'b0;
    else        tick_1ms <= 1'b1;
  end

  always @(posedge clk) begin
    if (tx_req_valid && tx_req_ready) begin
      if (tx_flags == FLG_SYN) begin
        syn_send_count   <= syn_send_count + 1;
        last_syn_ts      <= tx_tsval;
        last_syn_retrans <= tx_retrans_id;
      end
      if (tx_flags == FLG_START) begin
        start_send_count <= start_send_count + 1;
      end
    end
  end

  task automatic clear_rx;
    begin
      rx_valid           = 1'b0;
      rx_rm_hdr_valid    = 1'b0;
      rx_rm_checksum_ok  = 1'b0;
      rx_flags           = '0;
      rx_retrans_id      = '0;
      rx_receiver_id     = '0;
      rx_tsecr           = '0;
    end
  endtask

  task automatic pulse_start;
    begin
      @(posedge clk);
      start <= 1'b1;
      @(posedge clk);
      start <= 1'b0;
    end
  endtask

  task automatic send_syn_ack(
    input int rid,
    input [7:0] retrans_id,
    input [31:0] tsecr
  );
    begin
      @(posedge clk);
      rx_valid          <= 1'b1;
      rx_rm_hdr_valid   <= 1'b1;
      rx_rm_checksum_ok <= 1'b1;
      rx_flags          <= (FLG_SYN | FLG_ACK);
      rx_retrans_id     <= retrans_id;
      rx_receiver_id    <= rid[RECEIVER_ID_W-1:0];
      rx_tsecr          <= tsecr;
      @(posedge clk);
      clear_rx();
    end
  endtask

  initial begin
    clk = 1'b0;
    rst_n = 1'b0;
    tick_1ms = 1'b0;
    start = 1'b0;
    tx_req_ready = 1'b1;
    cfg_sender_port = 16'd4000;
    cfg_mcast_port  = 16'd5000;
    cfg_mcast_ip    = RM_DEFAULT_MCAST_IP;
    cfg_mcast_mac   = RM_DEFAULT_MCAST_MAC;
    cfg_expected_rcv= 2;
    cfg_rto_ms      = 16;
    cfg_max_retries = 3;
    syn_send_count  = 0;
    start_send_count= 0;
    last_syn_ts     = '0;
    last_syn_retrans= '0;
    clear_rx();

    repeat (3) @(posedge clk);
    rst_n = 1'b1;

    // ---------- Run 1: successful handshake, duplicate ACK ignored ----------
    pulse_start();

    wait (syn_send_count == 1);
    if (!busy) begin
      $error("busy should assert during handshake");
      $fatal(1);
    end

    send_syn_ack(0, last_syn_retrans, last_syn_ts);
    send_syn_ack(0, last_syn_retrans, last_syn_ts); // duplicate, must be ignored
    send_syn_ack(1, last_syn_retrans, last_syn_ts);

    wait (start_send_count == 1);
    @(posedge clk);

    if (!done) begin
      $error("expected done pulse after START multicast");
      $fatal(1);
    end
    if (!session_start_pulse) begin
      $error("expected session_start_pulse after successful handshake");
      $fatal(1);
    end
    if (cohort_count != 2) begin
      $error("expected exactly 2 unique ACKs, got %0d", cohort_count);
      $fatal(1);
    end

    repeat (3) @(posedge clk);

    // ---------- Run 2: first SYN times out, retransmit, then succeed ----------
    syn_send_count   = 0;
    start_send_count = 0;
    pulse_start();

    wait (syn_send_count == 1);
    // Do not ACK yet. Let timeout fire and force a retransmission.
    wait (syn_send_count == 2);
    if (last_syn_retrans != 8'd2) begin
      $error("expected retrans_id to increment on SYN retry, got %0d", last_syn_retrans);
      $fatal(1);
    end

    send_syn_ack(0, last_syn_retrans, last_syn_ts);
    send_syn_ack(1, last_syn_retrans, last_syn_ts);

    wait (start_send_count == 1);
    @(posedge clk);
    if (!done || !session_start_pulse) begin
      $error("second handshake did not complete successfully");
      $fatal(1);
    end
    if (fail) begin
      $error("unexpected fail pulse in successful retry case");
      $fatal(1);
    end

    $display("tb_rm_mcast_handshake_sender PASS");
    $finish;
  end

endmodule
