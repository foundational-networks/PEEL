`timescale 1ns/1ps
import rm_proto_pkg::*;

module tb_peel_transport_end_to_end;

  localparam int MAX_COHORT    = 4;
  localparam int EXPECTED_W    = $clog2(MAX_COHORT+1);
  localparam int RECEIVER_ID_W = $clog2(MAX_COHORT);

  logic clk;
  logic rst_n;
  logic tick_1ms;

  // Handshake sender control
  logic hs_start;
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
  logic [31:0] hs_tx_tsval;
  logic [31:0] hs_tx_tsecr;
  logic hs_busy;
  logic hs_done;
  logic hs_fail;
  logic session_start_pulse;
  logic [EXPECTED_W-1:0] cohort_count;
  logic hs_rx_valid;
  logic hs_rx_rm_hdr_valid;
  logic hs_rx_rm_checksum_ok;
  logic [15:0] hs_rx_flags;
  logic [7:0]  hs_rx_retrans_id;
  logic [RECEIVER_ID_W-1:0] hs_rx_receiver_id;
  logic [31:0] hs_rx_tsecr;

  // Transport sender control
  logic tx_start;
  logic tx_tx_req_valid;
  logic tx_tx_req_ready;
  logic tx_tx_is_multicast;
  logic [47:0] tx_tx_dst_mac;
  logic [31:0] tx_tx_dst_ip;
  logic [15:0] tx_tx_dst_port;
  logic [15:0] tx_tx_src_port;
  logic [15:0] tx_tx_flags;
  logic [31:0] tx_tx_seq;
  logic [7:0]  tx_tx_retrans_id;
  logic [7:0]  tx_tx_receiver_id;
  logic [31:0] tx_tx_tsval;
  logic [31:0] tx_tx_tsecr;
  logic tx_busy;
  logic tx_done;
  logic tx_fail;
  logic [31:0] current_seq;
  logic sending_fin;
  logic tx_rx_valid;
  logic tx_rx_rm_hdr_valid;
  logic tx_rx_rm_checksum_ok;
  logic [15:0] tx_rx_flags;
  logic [31:0] tx_rx_seq;
  logic [7:0]  tx_rx_retrans_id;
  logic [RECEIVER_ID_W-1:0] tx_rx_receiver_id;
  logic [31:0] tx_rx_tsecr;

  // Multicast network pipeline
  logic        net_mcast_valid_d;
  logic [31:0] net_mcast_src_ip_d;
  logic [15:0] net_mcast_src_port_d;
  logic [15:0] net_mcast_flags_d;
  logic [31:0] net_mcast_seq_d;
  logic [7:0]  net_mcast_retrans_id_d;
  logic [31:0] net_mcast_tsval_d;

  // Receiver inputs (same multicast fanout)
  logic rcv_rx_mcast_valid;
  logic rcv_rx_rm_hdr_valid;
  logic rcv_rx_rm_checksum_ok;
  logic [31:0] rcv_rx_src_ip;
  logic [15:0] rcv_rx_src_port;
  logic [15:0] rcv_rx_flags;
  logic [31:0] rcv_rx_seq;
  logic [7:0]  rcv_rx_retrans_id;
  logic [31:0] rcv_rx_tsval;

  // Receiver 0 outputs
  logic r0_tx_req_valid;
  logic r0_tx_req_ready;
  logic r0_tx_is_multicast;
  logic [47:0] r0_tx_dst_mac;
  logic [31:0] r0_tx_dst_ip;
  logic [15:0] r0_tx_dst_port;
  logic [15:0] r0_tx_src_port;
  logic [15:0] r0_tx_flags;
  logic [31:0] r0_tx_seq;
  logic [7:0]  r0_tx_retrans_id;
  logic [7:0]  r0_tx_receiver_id;
  logic [31:0] r0_tx_tsval;
  logic [31:0] r0_tx_tsecr;
  logic r0_session_active;
  logic r0_in_linger;

  // Receiver 1 outputs
  logic r1_tx_req_valid;
  logic r1_tx_req_ready;
  logic r1_tx_is_multicast;
  logic [47:0] r1_tx_dst_mac;
  logic [31:0] r1_tx_dst_ip;
  logic [15:0] r1_tx_dst_port;
  logic [15:0] r1_tx_src_port;
  logic [15:0] r1_tx_flags;
  logic [31:0] r1_tx_seq;
  logic [7:0]  r1_tx_retrans_id;
  logic [7:0]  r1_tx_receiver_id;
  logic [31:0] r1_tx_tsval;
  logic [31:0] r1_tx_tsecr;
  logic r1_session_active;
  logic r1_in_linger;

  int hs_syn_count;
  int hs_start_count;
  int tx_data_count;
  int tx_fin_count;
  logic rr_sel;
  int watchdog;

  rm_mcast_handshake_sender #(
    .MAX_COHORT(MAX_COHORT)
  ) u_hs (
    .clk,
    .rst_n,
    .tick_1ms,
    .start(hs_start),
    .cfg_sender_port(16'd4000),
    .cfg_mcast_port(RM_DEFAULT_MCAST_PORT),
    .cfg_mcast_ip(RM_DEFAULT_MCAST_IP),
    .cfg_mcast_mac(RM_DEFAULT_MCAST_MAC),
    .cfg_expected_rcv(2),
    .cfg_rto_ms(16'd16),
    .cfg_max_retries(8'd3),
    .rx_valid(hs_rx_valid),
    .rx_rm_hdr_valid(hs_rx_rm_hdr_valid),
    .rx_rm_checksum_ok(hs_rx_rm_checksum_ok),
    .rx_flags(hs_rx_flags),
    .rx_retrans_id(hs_rx_retrans_id),
    .rx_receiver_id(hs_rx_receiver_id),
    .rx_tsecr(hs_rx_tsecr),
    .tx_req_valid(hs_tx_req_valid),
    .tx_req_ready(hs_tx_req_ready),
    .tx_is_multicast(hs_tx_is_multicast),
    .tx_dst_mac(hs_tx_dst_mac),
    .tx_dst_ip(hs_tx_dst_ip),
    .tx_dst_port(hs_tx_dst_port),
    .tx_src_port(hs_tx_src_port),
    .tx_flags(hs_tx_flags),
    .tx_seq(hs_tx_seq),
    .tx_retrans_id(hs_tx_retrans_id),
    .tx_receiver_id(hs_tx_receiver_id),
    .tx_tsval(hs_tx_tsval),
    .tx_tsecr(hs_tx_tsecr),
    .busy(hs_busy),
    .done(hs_done),
    .fail(hs_fail),
    .session_start_pulse,
    .cohort_count
  );

  rm_stopwait_transport_sender #(
    .MAX_COHORT(MAX_COHORT)
  ) u_tx (
    .clk,
    .rst_n,
    .tick_1ms,
    .start(tx_start),
    .cfg_sender_port(16'd4000),
    .cfg_mcast_port(RM_DEFAULT_MCAST_PORT),
    .cfg_mcast_ip(RM_DEFAULT_MCAST_IP),
    .cfg_mcast_mac(RM_DEFAULT_MCAST_MAC),
    .cfg_expected_rcv(2),
    .cfg_total_packets(32'd2),
    .cfg_rto_ms(16'd16),
    .cfg_max_retries(8'd3),
    .rx_valid(tx_rx_valid),
    .rx_rm_hdr_valid(tx_rx_rm_hdr_valid),
    .rx_rm_checksum_ok(tx_rx_rm_checksum_ok),
    .rx_flags(tx_rx_flags),
    .rx_seq(tx_rx_seq),
    .rx_retrans_id(tx_rx_retrans_id),
    .rx_receiver_id(tx_rx_receiver_id),
    .rx_tsecr(tx_rx_tsecr),
    .tx_req_valid(tx_tx_req_valid),
    .tx_req_ready(tx_tx_req_ready),
    .tx_is_multicast(tx_tx_is_multicast),
    .tx_dst_mac(tx_tx_dst_mac),
    .tx_dst_ip(tx_tx_dst_ip),
    .tx_dst_port(tx_tx_dst_port),
    .tx_src_port(tx_tx_src_port),
    .tx_flags(tx_tx_flags),
    .tx_seq(tx_tx_seq),
    .tx_retrans_id(tx_tx_retrans_id),
    .tx_receiver_id(tx_tx_receiver_id),
    .tx_tsval(tx_tx_tsval),
    .tx_tsecr(tx_tx_tsecr),
    .busy(tx_busy),
    .done(tx_done),
    .fail(tx_fail),
    .current_seq,
    .sending_fin
  );

  rm_transport_receiver #(
    .MAX_COHORT(MAX_COHORT)
  ) u_r0 (
    .clk,
    .rst_n,
    .tick_1ms,
    .cfg_receiver_id(0),
    .cfg_receiver_port(RM_DEFAULT_MCAST_PORT),
    .cfg_linger_ms(16'd5),
    .rx_mcast_valid(rcv_rx_mcast_valid),
    .rx_rm_hdr_valid(rcv_rx_rm_hdr_valid),
    .rx_rm_checksum_ok(rcv_rx_rm_checksum_ok),
    .rx_src_ip(rcv_rx_src_ip),
    .rx_src_port(rcv_rx_src_port),
    .rx_flags(rcv_rx_flags),
    .rx_seq(rcv_rx_seq),
    .rx_retrans_id(rcv_rx_retrans_id),
    .rx_tsval(rcv_rx_tsval),
    .tx_req_valid(r0_tx_req_valid),
    .tx_req_ready(r0_tx_req_ready),
    .tx_is_multicast(r0_tx_is_multicast),
    .tx_dst_mac(r0_tx_dst_mac),
    .tx_dst_ip(r0_tx_dst_ip),
    .tx_dst_port(r0_tx_dst_port),
    .tx_src_port(r0_tx_src_port),
    .tx_flags(r0_tx_flags),
    .tx_seq(r0_tx_seq),
    .tx_retrans_id(r0_tx_retrans_id),
    .tx_receiver_id(r0_tx_receiver_id),
    .tx_tsval(r0_tx_tsval),
    .tx_tsecr(r0_tx_tsecr),
    .session_active(r0_session_active),
    .in_linger(r0_in_linger)
  );

  rm_transport_receiver #(
    .MAX_COHORT(MAX_COHORT)
  ) u_r1 (
    .clk,
    .rst_n,
    .tick_1ms,
    .cfg_receiver_id(1),
    .cfg_receiver_port(RM_DEFAULT_MCAST_PORT),
    .cfg_linger_ms(16'd5),
    .rx_mcast_valid(rcv_rx_mcast_valid),
    .rx_rm_hdr_valid(rcv_rx_rm_hdr_valid),
    .rx_rm_checksum_ok(rcv_rx_rm_checksum_ok),
    .rx_src_ip(rcv_rx_src_ip),
    .rx_src_port(rcv_rx_src_port),
    .rx_flags(rcv_rx_flags),
    .rx_seq(rcv_rx_seq),
    .rx_retrans_id(rcv_rx_retrans_id),
    .rx_tsval(rcv_rx_tsval),
    .tx_req_valid(r1_tx_req_valid),
    .tx_req_ready(r1_tx_req_ready),
    .tx_is_multicast(r1_tx_is_multicast),
    .tx_dst_mac(r1_tx_dst_mac),
    .tx_dst_ip(r1_tx_dst_ip),
    .tx_dst_port(r1_tx_dst_port),
    .tx_src_port(r1_tx_src_port),
    .tx_flags(r1_tx_flags),
    .tx_seq(r1_tx_seq),
    .tx_retrans_id(r1_tx_retrans_id),
    .tx_receiver_id(r1_tx_receiver_id),
    .tx_tsval(r1_tx_tsval),
    .tx_tsecr(r1_tx_tsecr),
    .session_active(r1_session_active),
    .in_linger(r1_in_linger)
  );

  always #5 clk = ~clk;

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) tick_1ms <= 1'b0;
    else        tick_1ms <= 1'b1;
  end

  // One-cycle pulse to start transport after handshake succeeds.
  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) tx_start <= 1'b0;
    else        tx_start <= session_start_pulse;
  end

  // Count sender transmissions.
  always @(posedge clk) begin
    if (hs_tx_req_valid && hs_tx_req_ready) begin
      if (hs_tx_flags == FLG_SYN) begin
        hs_syn_count <= hs_syn_count + 1;
        $display("%0t HS SEND SYN retrans=%0d ts=%0d", $time, hs_tx_retrans_id, hs_tx_tsval);
      end
      if (hs_tx_flags == FLG_START) begin
        hs_start_count <= hs_start_count + 1;
        $display("%0t HS SEND START", $time);
      end
    end
    if (tx_tx_req_valid && tx_tx_req_ready) begin
      if (tx_tx_flags == FLG_DATA) begin
        tx_data_count <= tx_data_count + 1;
        $display("%0t TX SEND DATA seq=%0d retrans=%0d ts=%0d", $time, tx_tx_seq, tx_tx_retrans_id, tx_tx_tsval);
      end
      if (tx_tx_flags == FLG_FIN) begin
        tx_fin_count <= tx_fin_count + 1;
        $display("%0t TX SEND FIN seq=%0d retrans=%0d ts=%0d", $time, tx_tx_seq, tx_tx_retrans_id, tx_tx_tsval);
      end
    end
    if (hs_rx_valid) begin
      $display("%0t HS RX ACK flags=0x%0h rid=%0d retrans=%0d tsecr=%0d", $time, hs_rx_flags, hs_rx_receiver_id, hs_rx_retrans_id, hs_rx_tsecr);
    end
    if (hs_done) begin
      $display("%0t HS DONE", $time);
    end
    if (hs_fail) begin
      $display("%0t HS FAIL", $time);
    end
    if (tx_done) begin
      $display("%0t TX DONE", $time);
    end
    if (tx_fail) begin
      $display("%0t TX FAIL", $time);
    end
  end

  // Sender -> multicast network pipe -> both receivers.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      net_mcast_valid_d      <= 1'b0;
      net_mcast_src_ip_d     <= '0;
      net_mcast_src_port_d   <= '0;
      net_mcast_flags_d      <= '0;
      net_mcast_seq_d        <= '0;
      net_mcast_retrans_id_d <= '0;
      net_mcast_tsval_d      <= '0;
    end else begin
      net_mcast_valid_d <= 1'b0;
      if (hs_tx_req_valid && hs_tx_req_ready) begin
        net_mcast_valid_d      <= 1'b1;
        net_mcast_src_ip_d     <= 32'h0A000001;
        net_mcast_src_port_d   <= hs_tx_src_port;
        net_mcast_flags_d      <= hs_tx_flags;
        net_mcast_seq_d        <= hs_tx_seq;
        net_mcast_retrans_id_d <= hs_tx_retrans_id;
        net_mcast_tsval_d      <= hs_tx_tsval;
      end else if (tx_tx_req_valid && tx_tx_req_ready) begin
        net_mcast_valid_d      <= 1'b1;
        net_mcast_src_ip_d     <= 32'h0A000001;
        net_mcast_src_port_d   <= tx_tx_src_port;
        net_mcast_flags_d      <= tx_tx_flags;
        net_mcast_seq_d        <= tx_tx_seq;
        net_mcast_retrans_id_d <= tx_tx_retrans_id;
        net_mcast_tsval_d      <= tx_tx_tsval;
      end
    end
  end

  always_comb begin
    rcv_rx_mcast_valid    = net_mcast_valid_d;
    rcv_rx_rm_hdr_valid   = net_mcast_valid_d;
    rcv_rx_rm_checksum_ok = net_mcast_valid_d;
    rcv_rx_src_ip         = net_mcast_src_ip_d;
    rcv_rx_src_port       = net_mcast_src_port_d;
    rcv_rx_flags          = net_mcast_flags_d;
    rcv_rx_seq            = net_mcast_seq_d;
    rcv_rx_retrans_id     = net_mcast_retrans_id_d;
    rcv_rx_tsval          = net_mcast_tsval_d;
  end

  // Fair ACK arbitration from the two receivers back into the single sender RX
  // channel. Round-robin when both are pending.
  always_comb begin
    r0_tx_req_ready = 1'b0;
    r1_tx_req_ready = 1'b0;

    hs_rx_valid          = 1'b0;
    hs_rx_rm_hdr_valid   = 1'b0;
    hs_rx_rm_checksum_ok = 1'b0;
    hs_rx_flags          = '0;
    hs_rx_retrans_id     = '0;
    hs_rx_receiver_id    = '0;
    hs_rx_tsecr          = '0;

    tx_rx_valid          = 1'b0;
    tx_rx_rm_hdr_valid   = 1'b0;
    tx_rx_rm_checksum_ok = 1'b0;
    tx_rx_flags          = '0;
    tx_rx_seq            = '0;
    tx_rx_retrans_id     = '0;
    tx_rx_receiver_id    = '0;
    tx_rx_tsecr          = '0;

    hs_tx_req_ready = 1'b1;
    tx_tx_req_ready = 1'b1;

    if (r0_tx_req_valid && r1_tx_req_valid) begin
      if (!rr_sel) begin
        r0_tx_req_ready = 1'b1;
      end else begin
        r1_tx_req_ready = 1'b1;
      end
    end else if (r0_tx_req_valid) begin
      r0_tx_req_ready = 1'b1;
    end else if (r1_tx_req_valid) begin
      r1_tx_req_ready = 1'b1;
    end

    if (r0_tx_req_ready) begin
      if (hs_busy) begin
        hs_rx_valid          = 1'b1;
        hs_rx_rm_hdr_valid   = 1'b1;
        hs_rx_rm_checksum_ok = 1'b1;
        hs_rx_flags          = r0_tx_flags;
        hs_rx_retrans_id     = r0_tx_retrans_id;
        hs_rx_receiver_id    = r0_tx_receiver_id[RECEIVER_ID_W-1:0];
        hs_rx_tsecr          = r0_tx_tsecr;
      end else if (tx_busy) begin
        tx_rx_valid          = 1'b1;
        tx_rx_rm_hdr_valid   = 1'b1;
        tx_rx_rm_checksum_ok = 1'b1;
        tx_rx_flags          = r0_tx_flags;
        tx_rx_seq            = r0_tx_seq;
        tx_rx_retrans_id     = r0_tx_retrans_id;
        tx_rx_receiver_id    = r0_tx_receiver_id[RECEIVER_ID_W-1:0];
        tx_rx_tsecr          = r0_tx_tsecr;
      end
    end else if (r1_tx_req_ready) begin
      if (hs_busy) begin
        hs_rx_valid          = 1'b1;
        hs_rx_rm_hdr_valid   = 1'b1;
        hs_rx_rm_checksum_ok = 1'b1;
        hs_rx_flags          = r1_tx_flags;
        hs_rx_retrans_id     = r1_tx_retrans_id;
        hs_rx_receiver_id    = r1_tx_receiver_id[RECEIVER_ID_W-1:0];
        hs_rx_tsecr          = r1_tx_tsecr;
      end else if (tx_busy) begin
        tx_rx_valid          = 1'b1;
        tx_rx_rm_hdr_valid   = 1'b1;
        tx_rx_rm_checksum_ok = 1'b1;
        tx_rx_flags          = r1_tx_flags;
        tx_rx_seq            = r1_tx_seq;
        tx_rx_retrans_id     = r1_tx_retrans_id;
        tx_rx_receiver_id    = r1_tx_receiver_id[RECEIVER_ID_W-1:0];
        tx_rx_tsecr          = r1_tx_tsecr;
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rr_sel <= 1'b0;
    end else if (r0_tx_req_ready || r1_tx_req_ready) begin
      rr_sel <= ~rr_sel;
    end
  end

  task automatic pulse_hs_start;
    begin
      @(posedge clk);
      hs_start <= 1'b1;
      @(posedge clk);
      hs_start <= 1'b0;
    end
  endtask

  initial begin
    clk = 1'b0;
    rst_n = 1'b0;
    tick_1ms = 1'b0;
    hs_start = 1'b0;
    tx_start = 1'b0;
    hs_syn_count = 0;
    hs_start_count = 0;
    tx_data_count = 0;
    tx_fin_count = 0;
    net_mcast_valid_d = 1'b0;
    rr_sel = 1'b0;
    watchdog = 0;

    repeat (3) @(posedge clk);
    rst_n = 1'b1;

    pulse_hs_start();

    while (!tx_done && !hs_fail && !tx_fail && watchdog < 1000) begin
      watchdog = watchdog + 1;
      @(posedge clk);
    end

    if (watchdog >= 1000) begin
      $error("end-to-end test timed out waiting for completion");
      $fatal(1);
    end

    if (hs_fail || tx_fail) begin
      $error("unexpected fail in end-to-end flow: hs_fail=%0b tx_fail=%0b", hs_fail, tx_fail);
      $fatal(1);
    end
    if (!hs_done && hs_start_count != 1) begin
      $error("handshake never reached START phase");
      $fatal(1);
    end
    if (hs_syn_count != 1 || hs_start_count != 1) begin
      $error("unexpected handshake multicast counts: SYN=%0d START=%0d", hs_syn_count, hs_start_count);
      $fatal(1);
    end
    if (tx_data_count != 2 || tx_fin_count != 1) begin
      $error("unexpected transport multicast counts: DATA=%0d FIN=%0d", tx_data_count, tx_fin_count);
      $fatal(1);
    end

    repeat (8) @(posedge clk);
    if (r0_session_active || r1_session_active || r0_in_linger || r1_in_linger) begin
      $error("receivers should be idle after linger expires");
      $fatal(1);
    end

    $display("tb_peel_transport_end_to_end PASS");
    $finish;
  end

endmodule
