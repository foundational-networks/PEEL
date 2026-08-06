`timescale 1ns/1ps
import rm_proto_pkg::*;

module tb_rm_transport_receiver;

  localparam int MAX_COHORT    = 4;
  localparam int RECEIVER_ID_W = $clog2(MAX_COHORT);

  logic clk;
  logic rst_n;
  logic tick_1ms;

  logic [RECEIVER_ID_W-1:0] cfg_receiver_id;
  logic [15:0] cfg_receiver_port;
  logic [15:0] cfg_linger_ms;

  logic rx_mcast_valid;
  logic rx_rm_hdr_valid;
  logic rx_rm_checksum_ok;
  logic [31:0] rx_src_ip;
  logic [15:0] rx_src_port;
  logic [15:0] rx_flags;
  logic [31:0] rx_seq;
  logic [7:0]  rx_retrans_id;
  logic [31:0] rx_tsval;

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

  logic session_active;
  logic in_linger;

  int ack_count;

  rm_transport_receiver #(
    .MAX_COHORT(MAX_COHORT)
  ) dut (
    .clk,
    .rst_n,
    .tick_1ms,
    .cfg_receiver_id,
    .cfg_receiver_port,
    .cfg_linger_ms,
    .rx_mcast_valid,
    .rx_rm_hdr_valid,
    .rx_rm_checksum_ok,
    .rx_src_ip,
    .rx_src_port,
    .rx_flags,
    .rx_seq,
    .rx_retrans_id,
    .rx_tsval,
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
    .session_active,
    .in_linger
  );

  always #5 clk = ~clk;

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) tick_1ms <= 1'b0;
    else        tick_1ms <= 1'b1;
  end

  always @(posedge clk) begin
    if (tx_req_valid && tx_req_ready)
      ack_count <= ack_count + 1;
  end

  task automatic clear_rx;
    begin
      rx_mcast_valid    = 1'b0;
      rx_rm_hdr_valid   = 1'b0;
      rx_rm_checksum_ok = 1'b0;
      rx_src_ip         = '0;
      rx_src_port       = '0;
      rx_flags          = '0;
      rx_seq            = '0;
      rx_retrans_id     = '0;
      rx_tsval          = '0;
    end
  endtask

  task automatic send_mcast(
    input [15:0] flags,
    input [31:0] seq,
    input [7:0]  retrans_id,
    input [31:0] tsval
  );
    begin
      @(posedge clk);
      rx_mcast_valid    <= 1'b1;
      rx_rm_hdr_valid   <= 1'b1;
      rx_rm_checksum_ok <= 1'b1;
      rx_src_ip         <= 32'h0A000001;
      rx_src_port       <= 16'd4000;
      rx_flags          <= flags;
      rx_seq            <= seq;
      rx_retrans_id     <= retrans_id;
      rx_tsval          <= tsval;
      @(posedge clk);
      clear_rx();
    end
  endtask

  task automatic check_ack(
    input [15:0] exp_flags,
    input [31:0] exp_seq,
    input [7:0]  exp_retrans,
    input [31:0] exp_tsecr,
    input string name
  );
    begin
      wait (tx_req_valid === 1'b1);
      #1;
      if (tx_is_multicast !== 1'b0 ||
          tx_dst_ip      !== 32'h0A000001 ||
          tx_dst_port    !== 16'd4000 ||
          tx_src_port    !== cfg_receiver_port ||
          tx_flags       !== exp_flags ||
          tx_seq         !== exp_seq ||
          tx_retrans_id  !== exp_retrans ||
          tx_receiver_id !== cfg_receiver_id ||
          tx_tsecr       !== exp_tsecr) begin
        $error("%s ACK mismatch", name);
        $fatal(1);
      end
      @(posedge clk);
    end
  endtask

  initial begin
    clk = 1'b0;
    rst_n = 1'b0;
    tick_1ms = 1'b0;
    cfg_receiver_id   = 2;
    cfg_receiver_port = 16'd5000;
    cfg_linger_ms     = 16'd5;
    tx_req_ready      = 1'b1;
    ack_count         = 0;
    clear_rx();

    repeat (3) @(posedge clk);
    rst_n = 1'b1;

    // SYN => SYN|ACK
    send_mcast(FLG_SYN, 32'd10, 8'd1, 32'h1111);
    check_ack((FLG_SYN | FLG_ACK), 32'd10, 8'd1, 32'h1111, "syn");

    // START => session becomes active, no ACK required.
    send_mcast(FLG_START, 32'd0, 8'd0, 32'h2222);
    @(posedge clk);
    if (!session_active || in_linger) begin
      $error("receiver did not enter active session after START");
      $fatal(1);
    end

    // DATA => DATA|ACK, echo retransmission id.
    send_mcast(FLG_DATA, 32'd3, 8'd4, 32'h3333);
    check_ack((FLG_DATA | FLG_ACK), 32'd3, 8'd4, 32'h3333, "data");

    // FIN => FIN|ACK and linger begins.
    send_mcast(FLG_FIN, 32'd3, 8'd4, 32'h4444);
    check_ack((FLG_FIN | FLG_ACK), 32'd3, 8'd4, 32'h4444, "fin");
    @(posedge clk);
    if (session_active || !in_linger) begin
      $error("receiver should be in linger after FIN");
      $fatal(1);
    end

    // During linger, duplicate FIN should still be ACKed.
    send_mcast(FLG_FIN, 32'd3, 8'd5, 32'h5555);
    check_ack((FLG_FIN | FLG_ACK), 32'd3, 8'd5, 32'h5555, "duplicate fin during linger");

    repeat (8) @(posedge clk);
    if (in_linger || session_active) begin
      $error("linger did not expire as expected");
      $fatal(1);
    end
    if (ack_count != 4) begin
      $error("expected 4 ACKs total, got %0d", ack_count);
      $fatal(1);
    end

    $display("tb_rm_transport_receiver PASS");
    $finish;
  end

endmodule
