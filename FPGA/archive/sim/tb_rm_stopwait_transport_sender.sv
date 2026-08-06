`timescale 1ns/1ps
import rm_proto_pkg::*;

module tb_rm_stopwait_transport_sender;

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
  logic [31:0] cfg_total_packets;
  logic [15:0] cfg_rto_ms;
  logic [7:0]  cfg_max_retries;

  logic rx_valid;
  logic rx_rm_hdr_valid;
  logic rx_rm_checksum_ok;
  logic [15:0] rx_flags;
  logic [31:0] rx_seq;
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
  logic [31:0] current_seq;
  logic sending_fin;

  int data_send_count;
  int fin_send_count;
  logic [31:0] last_pkt_ts;
  logic [7:0]  last_retrans_id;
  logic [31:0] last_seq;
  logic [15:0] last_flags;

  rm_stopwait_transport_sender #(
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
    .cfg_total_packets,
    .cfg_rto_ms,
    .cfg_max_retries,
    .rx_valid,
    .rx_rm_hdr_valid,
    .rx_rm_checksum_ok,
    .rx_flags,
    .rx_seq,
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
    .current_seq,
    .sending_fin
  );

  always #5 clk = ~clk;

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) tick_1ms <= 1'b0;
    else        tick_1ms <= 1'b1;
  end

  always @(posedge clk) begin
    if (tx_req_valid && tx_req_ready) begin
      last_pkt_ts     <= tx_tsval;
      last_retrans_id <= tx_retrans_id;
      last_seq        <= tx_seq;
      last_flags      <= tx_flags;
      if (tx_flags == FLG_DATA) data_send_count <= data_send_count + 1;
      if (tx_flags == FLG_FIN)  fin_send_count  <= fin_send_count + 1;
    end
  end

  task automatic clear_rx;
    begin
      rx_valid          = 1'b0;
      rx_rm_hdr_valid   = 1'b0;
      rx_rm_checksum_ok = 1'b0;
      rx_flags          = '0;
      rx_seq            = '0;
      rx_retrans_id     = '0;
      rx_receiver_id    = '0;
      rx_tsecr          = '0;
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

  task automatic send_ack(
    input int rid,
    input [15:0] flags,
    input [31:0] seq,
    input [7:0] retrans_id,
    input [31:0] tsecr
  );
    begin
      @(posedge clk);
      rx_valid          <= 1'b1;
      rx_rm_hdr_valid   <= 1'b1;
      rx_rm_checksum_ok <= 1'b1;
      rx_flags          <= flags;
      rx_seq            <= seq;
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
    cfg_sender_port  = 16'd4000;
    cfg_mcast_port   = 16'd5000;
    cfg_mcast_ip     = RM_DEFAULT_MCAST_IP;
    cfg_mcast_mac    = RM_DEFAULT_MCAST_MAC;
    cfg_expected_rcv = 2;
    cfg_total_packets= 3;
    cfg_rto_ms       = 16;
    cfg_max_retries  = 3;
    data_send_count  = 0;
    fin_send_count   = 0;
    last_pkt_ts      = '0;
    last_retrans_id  = '0;
    last_seq         = '0;
    last_flags       = '0;
    clear_rx();

    repeat (3) @(posedge clk);
    rst_n = 1'b1;

    pulse_start();

    // Packet 0 normal ACK from both receivers.
    wait (data_send_count == 1 && last_flags == FLG_DATA && last_seq == 0);
    send_ack(0, (FLG_DATA | FLG_ACK), last_seq, last_retrans_id, last_pkt_ts);
    send_ack(1, (FLG_DATA | FLG_ACK), last_seq, last_retrans_id, last_pkt_ts);

    // Packet 1: force timeout once, then ACK the retransmission.
    wait (data_send_count == 2 && last_flags == FLG_DATA && last_seq == 1 && last_retrans_id == 8'd1);
    wait (data_send_count == 3 && last_flags == FLG_DATA && last_seq == 1 && last_retrans_id == 8'd2);
    send_ack(0, (FLG_DATA | FLG_ACK), last_seq, last_retrans_id, last_pkt_ts);
    send_ack(1, (FLG_DATA | FLG_ACK), last_seq, last_retrans_id, last_pkt_ts);

    // Packet 2 normal ACK path again.
    wait (data_send_count == 4 && last_flags == FLG_DATA && last_seq == 2 && last_retrans_id == 8'd1);
    send_ack(0, (FLG_DATA | FLG_ACK), last_seq, last_retrans_id, last_pkt_ts);
    send_ack(1, (FLG_DATA | FLG_ACK), last_seq, last_retrans_id, last_pkt_ts);

    // FIN ACK from both receivers.
    wait (fin_send_count == 1 && last_flags == FLG_FIN);
    send_ack(0, (FLG_FIN | FLG_ACK), last_seq, last_retrans_id, last_pkt_ts);
    send_ack(1, (FLG_FIN | FLG_ACK), last_seq, last_retrans_id, last_pkt_ts);

    @(posedge clk);
    if (!done) begin
      $error("expected done pulse after FIN ACKs");
      $fatal(1);
    end
    if (fail) begin
      $error("unexpected fail pulse");
      $fatal(1);
    end
    if (data_send_count != 4) begin
      $error("expected 4 DATA sends total (including 1 retransmission), got %0d", data_send_count);
      $fatal(1);
    end
    if (fin_send_count != 1) begin
      $error("expected 1 FIN send, got %0d", fin_send_count);
      $fatal(1);
    end

    $display("tb_rm_stopwait_transport_sender PASS");
    $finish;
  end

endmodule
