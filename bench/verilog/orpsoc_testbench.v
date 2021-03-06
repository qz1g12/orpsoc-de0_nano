//////////////////////////////////////////////////////////////////////
///                                                               //// 
/// ORPSoC testbench                                              ////
///                                                               ////
/// Instantiate ORPSoC, monitors, provide stimulus                ////
///                                                               ////
/// Julius Baxter, julius@opencores.org                           ////
///                                                               ////
//////////////////////////////////////////////////////////////////////
////                                                              ////
//// Copyright (C) 2009, 2010 Authors and OPENCORES.ORG           ////
////                                                              ////
//// This source file may be used and distributed without         ////
//// restriction provided that this copyright statement is not    ////
//// removed from the file and that any derivative work contains  ////
//// the original copyright notice and the associated disclaimer. ////
////                                                              ////
//// This source file is free software; you can redistribute it   ////
//// and/or modify it under the terms of the GNU Lesser General   ////
//// Public License as published by the Free Software Foundation; ////
//// either version 2.1 of the License, or (at your option) any   ////
//// later version.                                               ////
////                                                              ////
//// This source is distributed in the hope that it will be       ////
//// useful, but WITHOUT ANY WARRANTY; without even the implied   ////
//// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      ////
//// PURPOSE.  See the GNU Lesser General Public License for more ////
//// details.                                                     ////
////                                                              ////
//// You should have received a copy of the GNU Lesser General    ////
//// Public License along with this source; if not, download it   ////
//// from http://www.opencores.org/lgpl.shtml                     ////
////                                                              ////
//////////////////////////////////////////////////////////////////////

`include "orpsoc-defines.v"
`include "orpsoc-testbench-defines.v"
`include "test-defines.v"

`include "timescale.v"

module orpsoc_testbench;

   reg clk = 0;
   reg rst_n = 1; // Active LOW
   
   always
     #((`BOARD_CLOCK_PERIOD)/2) clk <= ~clk;

   // Reset, ACTIVE LOW
   initial 
     begin
	#1;
	repeat (32) @(negedge clk)
	  rst_n <= 1;
	repeat (32) @(negedge clk)
	  rst_n <= 0;
	repeat (32) @(negedge clk)
	  rst_n <= 1;
     end

`include "orpsoc-params.v"

`ifdef JTAG_DEBUG
   wire 		     tdo_pad_o;
   wire 		     tck_pad_i;
   wire 		     tms_pad_i;
   wire 		     tdi_pad_i;
`endif   
`ifdef VERSATILE_SDRAM   
   wire [12:0] 		     sdram_a_pad_o;
   wire [1:0] 		     sdram_ba_pad_o;
   wire 		     sdram_cas_pad_o;
   wire 		     sdram_cke_pad_o;
   wire 		     sdram_clk_pad_o;
   wire 		     sdram_cs_n_pad_o;
   wire [15:0] 		     sdram_dq_pad_io;
   wire [1:0] 		     sdram_dqm_pad_o;
   wire 		     sdram_ras_pad_o;
   wire 		     sdram_we_pad_o;
`endif
`ifdef UART0
   wire 		     uart0_stx_pad_o;
   wire 		     uart0_srx_pad_i;
`endif
   
   orpsoc_top dut
     (
      .clk_pad_i                        (clk),
`ifdef JTAG_DEBUG          
      .tms_pad_i			(tms_pad_i),
      .tck_pad_i			(tck_pad_i),
      .tdi_pad_i			(tdi_pad_i),
      .tdo_pad_o			(tdo_pad_o),
`endif     
`ifdef VERSATILE_SDRAM
      .sdram_dq_pad_io			(sdram_dq_pad_io),            
      .sdram_ba_pad_o			(sdram_ba_pad_o),
      .sdram_a_pad_o			(sdram_a_pad_o),
      .sdram_cs_n_pad_o			(sdram_cs_n_pad_o),
      .sdram_ras_pad_o			(sdram_ras_pad_o),
      .sdram_cas_pad_o			(sdram_cas_pad_o),
      .sdram_we_pad_o			(sdram_we_pad_o),
      .sdram_dqm_pad_o			(sdram_dqm_pad_o),
      .sdram_cke_pad_o			(sdram_cke_pad_o),
      .sdram_clk_pad_o                  (sdram_clk_pad_o),
`endif 
`ifdef UART0      
      .uart0_stx_pad_o			(uart0_stx_pad_o),
      .uart0_srx_pad_i			(uart0_srx_pad_i),
`endif
      .rst_n_pad_i			(rst_n)
      );

   //
   // Instantiate OR1200 monitor
   //
   or1200_monitor monitor();

`ifndef SIM_QUIET
 `define CPU_ic_top or1200_ic_top
 `define CPU_dc_top or1200_dc_top
   wire ic_en = orpsoc_testbench.dut.or1200_top0.or1200_ic_top.ic_en;
   always @(posedge ic_en)
     $display("Or1200 IC enabled at %t", $time);

   wire dc_en = orpsoc_testbench.dut.or1200_top0.or1200_dc_top.dc_en;
   always @(posedge dc_en)
     $display("Or1200 DC enabled at %t", $time);
`endif


`ifdef JTAG_DEBUG   
 `ifdef VPI_DEBUG
   // Debugging interface
   jtag_vpi jtag_vpi0
     (
      .tms(tms_pad_i), 
      .tck(tck_pad_i), 
      .tdi(tdi_pad_i), 
      .tdo(tdo_pad_o)
      );
 `else   
   // If no VPI debugging, tie off JTAG inputs
   assign tdi_pad_i = 1;
   assign tck_pad_i = 0;
   assign tms_pad_i = 1;
 `endif // !`ifdef VPI_DEBUG_ENABLE
`endif //  `ifdef JTAG_DEBUG
   
`ifdef VERSATILE_SDRAM
   parameter TPROP_PCB = 2.0;
   reg [12:0] 		     sdram_a_pad_o_to_sdram;
   reg [1:0] 		     sdram_ba_pad_o_to_sdram;
   reg 		             sdram_cas_pad_o_to_sdram;
   reg 		             sdram_cke_pad_o_to_sdram;
   reg 		             sdram_cs_n_pad_o_to_sdram;
   wire [15:0] 		     sdram_dq_pad_io_to_sdram;
   reg [1:0] 		     sdram_dqm_pad_o_to_sdram;
   reg 		             sdram_ras_pad_o_to_sdram;
   reg 		             sdram_we_pad_o_to_sdram;

   always @( * ) begin
      sdram_a_pad_o_to_sdram    <= #(TPROP_PCB) sdram_a_pad_o;
      sdram_ba_pad_o_to_sdram   <= #(TPROP_PCB) sdram_ba_pad_o;
      sdram_cas_pad_o_to_sdram  <= #(TPROP_PCB) sdram_cas_pad_o;
      sdram_cke_pad_o_to_sdram  <= #(TPROP_PCB) sdram_cke_pad_o;
      sdram_cs_n_pad_o_to_sdram <= #(TPROP_PCB) sdram_cs_n_pad_o;
      sdram_dqm_pad_o_to_sdram  <= #(TPROP_PCB) sdram_dqm_pad_o;
      sdram_ras_pad_o_to_sdram  <= #(TPROP_PCB) sdram_ras_pad_o;
      sdram_we_pad_o_to_sdram   <= #(TPROP_PCB) sdram_we_pad_o;
   end

   genvar dqwd;
   generate
      for (dqwd = 0;dqwd < 16 ;dqwd = dqwd+1) begin : dq_delay
	 wiredelay #
	   (
            .Delay_g     (TPROP_PCB),
            .Delay_rd    (TPROP_PCB)
	    )
	 u_delay_dq
	   (
            .A           (sdram_dq_pad_io[dqwd]),
            .B           (sdram_dq_pad_io_to_sdram[dqwd]),
            .reset       (rst_n)
	    );
      end
   endgenerate
   
   // SDRAM
   mt48lc16m16a2 sdram0
     (
      // Inouts
      .Dq    (sdram_dq_pad_io_to_sdram),
      // Inputs
      .Addr  (sdram_a_pad_o_to_sdram),
      .Ba    (sdram_ba_pad_o_to_sdram),
      .Clk   (sdram_clk_pad_o),
      .Cke   (sdram_cke_pad_o_to_sdram),
      .Cs_n  (sdram_cs_n_pad_o_to_sdram),
      .Ras_n (sdram_ras_pad_o_to_sdram),
      .Cas_n (sdram_cas_pad_o_to_sdram),
      .We_n  (sdram_we_pad_o_to_sdram),
      .Dqm   (sdram_dqm_pad_o_to_sdram));
`endif //  `ifdef VERSATILE_SDRAM

   initial 
     begin
`ifndef SIM_QUIET
	$display("\n* Starting simulation of ORPSoC RTL.\n* Test: %s\n",
		 `TEST_NAME_STRING );
`endif	
      
`ifdef VCD
 `ifdef VCD_DELAY
	#(`VCD_DELAY);   
 `endif

	// Delay by x insns
 `ifdef VCD_DELAY_INSNS
	#10; // Delay until after the value becomes valid
	while (monitor.insns < `VCD_DELAY_INSNS)
	  @(posedge clk);
 `endif	

 `ifdef SIMULATOR_MODELSIM
	// Modelsim can GZip VCDs on the fly if given in the suffix
  `define VCD_SUFFIX   ".vcd.gz"
 `else
  `define VCD_SUFFIX   ".vcd"
 `endif
	
`ifndef SIM_QUIET
	$display("* VCD in %s\n", {"../out/",`TEST_NAME_STRING,`VCD_SUFFIX});
`endif	
	$dumpfile({"../out/",`TEST_NAME_STRING,`VCD_SUFFIX});
 `ifndef VCD_DEPTH
  `define VCD_DEPTH 0
 `endif     
	$dumpvars(`VCD_DEPTH);
`endif
      
   end // initial begin
   
`ifdef END_TIME
   initial begin
      #(`END_TIME);
`ifndef SIM_QUIET      
      $display("* Finish simulation due to END_TIME being set at %t", $time);
`endif      
      $finish;
   end
`endif

`ifdef END_INSNS
   initial begin
      #10
	while (monitor.insns < `END_INSNS)
	  @(posedge clk);
 `ifndef SIM_QUIET      
      $display("* Finish simulation due to END_INSNS count (%d) reached at %t",
	       `END_INSNS, $time);
 `endif
      $finish;
   end
`endif     
   
`ifdef UART0   
   //	
   // UART0 decoder
   //   
   uart_decoder
     #( 
	.uart_baudrate_period_ns(8680) // 115200 baud = period 8.68uS
	)
   uart0_decoder
     (
      .clk(clk),
      .uart_tx(uart0_stx_pad_o)
      );
   
   // UART0 stimulus
   /*
   uart_stim
     #( 
	.uart_baudrate_period_ns(8680) // 115200 baud = period 8.68uS
	)
   uart0_stim
     (
      .clk(clk),
      .uart_rx(uart0_srx_pad_i)
      );
    */
   // UART0 is looped back for now
   assign uart0_srx_pad_i = uart0_stx_pad_o;
   
`endif //  `ifdef UART0
   
endmodule // orpsoc_testbench

// Local Variables:
// verilog-library-directories:("." "../../rtl/verilog/orpsoc_top")
// verilog-library-files:()
// verilog-library-extensions:(".v" ".h")
// End:

