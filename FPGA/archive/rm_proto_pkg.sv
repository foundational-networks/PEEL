`timescale 1ns/1ps
package rm_proto_pkg;

  localparam logic [15:0] FLG_SYN   = 16'h0001;
  localparam logic [15:0] FLG_ACK   = 16'h0002;
  localparam logic [15:0] FLG_START = 16'h0004;
  localparam logic [15:0] FLG_DATA  = 16'h0008;
  localparam logic [15:0] FLG_FIN   = 16'h0010;
  localparam logic [15:0] FLG_RST   = 16'h0020;

  localparam logic [5:0]  RM_CTRL_DSCP         = 6'd7;
  localparam logic [47:0] RM_DEFAULT_MCAST_MAC = 48'h01_00_5E_01_02_03;
  localparam logic [31:0] RM_DEFAULT_MCAST_IP  = 32'hE0010203; // 224.1.2.3
  localparam logic [15:0] RM_DEFAULT_MCAST_PORT= 16'd5000;
  localparam logic [15:0] RM_DEFAULT_LINGER_MS = 16'd500;

endpackage
