/////////////////////////////////////////////////////////////////////
////                                                              ////
////  ORPSoC SystemC Testbench                                    ////
////                                                              ////
////  Description                                                 ////
////  ORPSoC Testbench file                                       ////
////                                                              ////
////  To Do:                                                      ////
////                                                              ////
////                                                              ////
////  Author(s):                                                  ////
////      - Jeremy Bennett jeremy.bennett@embecosm.com            ////
////      - Julius Baxter  julius@opencores.org                   ////
////                                                              ////
////                                                              ////
//////////////////////////////////////////////////////////////////////
////                                                              ////
//// Copyright (C) 2009 Authors and OPENCORES.ORG                 ////
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

#include "OrpsocMain.h"

#include "Vorpsoc_top.h"
#include "OrpsocAccess.h"
#include "MemoryLoad.h"

#include <verilated_vcd_c.h>

#include "ResetSC.h"
#include "Or1200MonitorSC.h"

// Include Verilog ORPSoC defines file, converted to C include format to be
// able to detect if the debug unit is to be built in or not.
#include "orpsoc-defines.h"

#ifdef JTAG_DEBUG
# include "GdbServerSC.h"
# include "JtagSC_includes.h"
#endif

#ifdef UART0
#  include "UartSC.h"
#endif

/* Global quiet variable */
bool gQuiet;

int gSimRunning;

int sc_main(int argc, char *argv[])
{
	sc_set_time_resolution(1, TIMESCALE_UNIT);
	// CPU clock (also used as JTAG TCK) and reset (both active high and low)
	sc_time clkPeriod(BENCH_CLK_HALFPERIOD * 2.0, TIMESCALE_UNIT);
	sc_clock clk("clk", clkPeriod);

	sc_signal < bool > rst;
	sc_signal < bool > rstn;

#ifdef JTAG_DEBUG
	sc_time jtagPeriod(JTAG_CLK_HALFPERIOD * 2.0, TIMESCALE_UNIT);
	sc_clock jtag_tck("jtag-clk", jtagPeriod, 0.5, SC_ZERO_TIME, false);

	sc_signal < bool > jtag_tdi;	// JTAG interface
	sc_signal < bool > jtag_tdo;
	sc_signal < bool > jtag_tms;
	sc_signal < bool > jtag_trst;
#endif

#ifdef UART0
	sc_signal < bool > uart_rx;	// External UART
	sc_signal < bool > uart_tx;
#endif

	gSimRunning = 0;

	// Are we running "quiet"?
	gQuiet = false;
	// Setup the name of the VCD dump file
	bool VCD_enabled = false;
	string dumpNameDefault("vlt-dump.vcd");
	string testNameString;
	string vcdDumpFile;
	// VCD dump controling vars
	bool dump_start_delay_set = false, dump_stop_set = false;
	bool dumping_now = false;
	int dump_depth = 99;	// Default dump depth
	sc_time dump_start, dump_stop, finish_time;
	bool finish_time_set = false;	// By default we will let the simulation 
	// finish naturally
	VerilatedVcdC *verilatorVCDFile;

	/*int */ double time_val;
	bool vcd_file_name_given = false;

#ifdef JTAG_DEBUG
	bool rsp_server_enabled = false;
	int rsp_server_port = DEFAULT_RSP_PORT;
#endif

	// Executable app load variables
	int do_program_file_load = 0;	// Default: we don't require a file, we use the
	// VMEM
	char *program_file;	// Old char* style for program name

	// Verilator accessor
	OrpsocAccess *accessor;

	// Modules
	Vorpsoc_top *orpsoc;	// Verilated ORPSoC

	MemoryLoad *memoryload;	// Memory loader

	ResetSC *reset;		// Generate a RESET signal

	Or1200MonitorSC *monitor;	// Handle l.nop x instructions

#ifdef JTAG_DEBUG
	JtagSC *jtag;		// Generate JTAG signals
	GdbServerSC *gdbServer;	// Map RSP requests to debug unit
#endif

#ifdef UART0
	UartSC *uart;		// Handle UART signals
#endif

	// Instantiate the Verilator model, VCD trace handler and accessor
	orpsoc = new Vorpsoc_top("orpsoc");

	accessor = new OrpsocAccess(orpsoc);

	memoryload = new MemoryLoad(accessor);

	monitor = new Or1200MonitorSC("monitor", accessor, memoryload,
				      argc, argv);

	// Instantiate the SystemC modules
	reset = new ResetSC("reset", BENCH_RESET_TIME);

#ifdef JTAG_DEBUG
	jtag = new JtagSC("jtag");
#endif

#ifdef UART0
	uart = new UartSC("uart");
#endif

	// Parse command line options
	// Default is for VCD generation OFF, only turned on if specified on command 
	// line

	// Search through the command line parameters for options  
	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			if ((strcmp(argv[i], "-e") == 0) ||
			    (strcmp(argv[i], "--endtime") == 0)) {
				time_val = strtod(argv[i + 1], NULL);
				sc_time opt_end_time(time_val, TIMESCALE_UNIT);
				finish_time = opt_end_time;
				finish_time_set = true;
			} else if ((strcmp(argv[i], "-q") == 0) ||
				   (strcmp(argv[i], "--quiet") == 0)) {
				gQuiet = true;
			} else if ((strcmp(argv[i], "-f") == 0) ||
				  (strcmp(argv[i], "--program") == 0)) {
				do_program_file_load = 1;	// Enable program loading - will be 
				// done after sim init.
				program_file = argv[i + 1];	// Old char* style for program name
			} else if ((strcmp(argv[i], "-d") == 0) ||
				   (strcmp(argv[i], "--vcdfile") == 0) ||
				   (strcmp(argv[i], "-v") == 0) ||
				   (strcmp(argv[i], "--vcdon") == 0)
			    ) {
				VCD_enabled = true;
				dumping_now = true;
				vcdDumpFile = dumpNameDefault;
				if (i + 1 < argc)
					if (argv[i + 1][0] != '-') {
						testNameString = argv[i + 1];
						vcdDumpFile = testNameString;
						i++;
					}
			} else if ((strcmp(argv[i], "-s") == 0) ||
				   (strcmp(argv[i], "--vcdstart") == 0)) {
				VCD_enabled = true;
				time_val = strtod(argv[i + 1], NULL);
				sc_time dump_start_time(time_val,
							TIMESCALE_UNIT);
				dump_start = dump_start_time;
				dump_start_delay_set = true;
				dumping_now = false;
			} else if ((strcmp(argv[i], "-t") == 0) ||
				   (strcmp(argv[i], "--vcdstop") == 0)) {
				VCD_enabled = true;
				time_val = strtod(argv[i + 1], NULL);
				sc_time dump_stop_time(time_val,
						       TIMESCALE_UNIT);
				dump_stop = dump_stop_time;
				dump_stop_set = true;
			}
#ifdef JTAG_DEBUG
			else if ((strcmp(argv[i], "-r") == 0) ||
				 (strcmp(argv[i], "--rsp") == 0)) {
				rsp_server_enabled = true;
				if (i + 1 < argc)
					if (argv[i + 1][0] != '-') {
						rsp_server_port =
						    atoi(argv[i + 1]);
						i++;
					}
			}
#endif
			else if ((strcmp(argv[i], "-h") == 0) ||
				   (strcmp(argv[i], "--help") == 0)) {
				printf("Usage: %s [options]\n", argv[0]);
				printf("\n  ORPSoCv2 cycle accurate model\n");
				printf
				    ("  For details visit http://opencores.org/openrisc,orpsocv2\n");
				printf("\n");
				printf("Options:\n");
				printf
				    ("  -h, --help\t\tPrint this help message\n");
				printf
				    ("  -q, --quiet\t\tDisable all except UART print out\n");
				printf("\nSimulation control:\n");
				printf
				    ("  -f, --program <file> \tLoad program from OR32 ELF <file>\n");
				printf
				    ("  -e, --endtime <val> \tStop the sim at <val> ns\n");
				printf("\nVCD generation:\n");
				printf
				    ("  -v, --vcdon\t\tEnable VCD generation\n");
				printf
				    ("  -d, --vcdfile <file>\tEnable and save VCD to <file>\n");

				printf
				    ("  -s, --vcdstart <val>\tEnable and delay VCD generation until <val> ns\n");
				printf
				    ("  -t, --vcdstop <val> \tEnable and terminate VCD generation at <val> ns\n");
#ifdef JTAG_DEBUG
				printf("\nRemote debugging:\n");
				printf
				    ("  -r, --rsp [<port>]\tEnable RSP debugging server, opt. specify <port>\n");
#endif
				monitor->printUsage();
				printf("\n");
				return 0;
			}

		}
	}
	// Determine if we're going to setup a VCD dump:
	// Pretty much setting any related option will enable VCD dumping.
	if (VCD_enabled) {

		cout << "* Enabling VCD trace";

		if (dump_start_delay_set)
			cout << ", on at time " << dump_start.to_string();
		if (dump_stop_set)
			cout << ", off at time " << dump_stop.to_string();
		cout << endl;
	}
#ifdef JTAG_DEBUG
	if (rsp_server_enabled)
		gdbServer =
		    new GdbServerSC("gdb-server", FLASH_START, FLASH_END,
				    rsp_server_port, jtag->tapActionQueue);
	else
		gdbServer = NULL;
#endif

	// Connect up ORPSoC
	orpsoc->clk_pad_i(clk);
	orpsoc->rst_n_pad_i(rstn);

#ifdef JTAG_DEBUG
	orpsoc->tck_pad_i(jtag_tck);	// JTAG interface
	orpsoc->tdi_pad_i(jtag_tdi);
	orpsoc->tms_pad_i(jtag_tms);
	orpsoc->tdo_pad_o(jtag_tdo);
#endif

#ifdef UART0
	orpsoc->uart0_srx_pad_i(uart_rx);	// External UART
	orpsoc->uart0_stx_pad_o(uart_tx);
#endif

	// Connect up the SystemC  modules
	reset->clk(clk);	// Reset
	reset->rst(rst);
	reset->rstn(rstn);

	monitor->clk(clk);	// Monitor

#ifdef JTAG_DEBUG
	jtag->sysReset(rst);	// JTAG
	jtag->tck(jtag_tck);
	jtag->tdi(jtag_tdi);
	jtag->tdo(jtag_tdo);
	jtag->tms(jtag_tms);
	jtag->trst(jtag_trst);
#endif

#ifdef UART0
	uart->clk(clk);		// Uart
	uart->uartrx(uart_rx);	// orpsoc's receive line
	uart->uarttx(uart_tx);	// orpsoc's transmit line
#endif

	// Tie off signals
#ifdef JTAG_DEBUG
	jtag_tdi = 1;		// Tie off the JTAG inputs
	jtag_tms = 1;
#endif

	if (VCD_enabled) {
		Verilated::traceEverOn(true);

		printf("* VCD dumpfile: %s\n", vcdDumpFile.c_str());

		// Establish a new trace with its correct time resolution, and trace to
		// great depth.
		verilatorVCDFile = new VerilatedVcdC();

		orpsoc->trace(verilatorVCDFile, dump_depth);

		if (dumping_now) {
			verilatorVCDFile->open(vcdDumpFile.c_str());
		}
	}
	//printf("* Beginning test\n");

#ifdef UART0
	// Init the UART function
	uart->initUart(115200);
#endif

	if (do_program_file_load)	// Did the user specify a file to load?
	{
		if (!gQuiet)
			cout << "* Loading program from " << program_file << 
				endl;
		if (memoryload->loadcode(program_file, 0, 0) < 0) {
			cout << "* Error: executable file " << program_file <<
			    " not loaded" << endl;
			goto finish_up;
			
		}
	} else			
	{
		/* No ELF file specified, default is to load from  VMEM file */
		if (!gQuiet)
			cout <<
			    "* Loading memory with image from default file, sram.vmem"
			    << endl;
		accessor->do_ram_readmemh();
	}

	gSimRunning = 1;

	// First check how we should run the sim.
	if (VCD_enabled || finish_time_set) {	// We'll run sim with step

		if (!VCD_enabled && finish_time_set) {
			// We just run the sim until the set finish time
			sc_start((double)(finish_time.to_double()),
				 TIMESCALE_UNIT);
			gSimRunning = 0;
			sc_stop();
			// Print performance summary
			monitor->perfSummary();
			// Do memdump if enabled
			monitor->memdump();
		} else {
			if (dump_start_delay_set) {
				// Run the sim until we want to dump
				sc_start((double)(dump_start.to_double()),
					 TIMESCALE_UNIT);
				// Open the trace file
				verilatorVCDFile->open(vcdDumpFile.c_str());
				dumping_now = 1;
			}

			if (dumping_now) {
				// Step the sim and generate the trace
				// Execute until we stop
				while (!Verilated::gotFinish()) {
					// gSimRunning value changed by the
					// monitor when sim should finish.
					if (gSimRunning)
						// Step the sim
						sc_start(1, TIMESCALE_UNIT);
					else {
						verilatorVCDFile->close();
						break;
					}

					verilatorVCDFile->dump(sc_time_stamp().
							       to_double());

					if (dump_stop_set) {
						if (sc_time_stamp() >=
						    dump_stop) {
							// Close dump file
							verilatorVCDFile->close
								();
							// Now continue on again until the end
							if (!finish_time_set)
								sc_start();
							else {
								// Determine how long we should run for
								sc_time
								    sim_time_remaining
								    =
								    finish_time
								    -
								    sc_time_stamp
								    ();
								sc_start((double)(sim_time_remaining.to_double()), TIMESCALE_UNIT);
								// Officially stop the sim
								sc_stop();
								// Print performance summary
								monitor->
								    perfSummary
								    ();
								// Do memdump if enabled
								monitor->memdump
								    ();
							}
							break;
						}
					}
					if (finish_time_set) {
						if (sc_time_stamp() >=
						    finish_time) {
							// Officially stop the sim
							sc_stop();
							// Close dump file
							verilatorVCDFile->close
							    ();
							// Do memdump if enabled
							monitor->memdump();
							// Print performance summary
							monitor->perfSummary();
							break;
						}
					}
				}
			}
		}
	} else {
		// Simple run case
		// Ideally a "l.nop 1" will terminate the simulation gracefully.
		// Need to step at clock period / 4, otherwise model appears to skip the 
		// monitor and logging functions sometimes (?!?)
		while (gSimRunning)
			sc_start(BENCH_CLK_HALFPERIOD / 2, TIMESCALE_UNIT);
		//sc_start();
	}


finish_up:
	// Free memory
#ifdef JTAG_DEBUG
	if (rsp_server_enabled)
		delete gdbServer;

	delete jtag;
#endif

	delete monitor;

	delete reset;

	delete accessor;

	//delete trace;

	delete orpsoc;

	return 0;

}				/* sc_main() */
