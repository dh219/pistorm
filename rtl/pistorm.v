/*
 * Copyright 2020 Claude Schwarz
 * Copyright 2020 Niklas Ekström - rewrite in Verilog
 */
module pistorm(
    output reg      PI_TXN_IN_PROGRESS, // GPIO0
    output reg      PI_IPL_ZERO,        // GPIO1
    input   [1:0]   PI_A,       // GPIO[3..2]
    input           PI_CLK,     // GPIO4
    output reg      PI_RESET,   // GPIO5
    input           PI_RD,      // GPIO6
    input           PI_WR,      // GPIO7
    inout   [15:0]  PI_D,       // GPIO[23..8]

    output reg      LTCH_A_0,
    output reg      LTCH_A_8,
    output reg      LTCH_A_16,
    output reg      LTCH_A_24,
    output reg      LTCH_A_OE_n,
    output reg      LTCH_D_RD_U,
    output reg      LTCH_D_RD_L,
    output reg      LTCH_D_RD_OE_n,
    output reg      LTCH_D_WR_U,
    output reg      LTCH_D_WR_L,
    output reg      LTCH_D_WR_OE_n,

    input           M68K_CLK,
    output   [2:0]  M68K_FC,

    output       M68K_AS_n,
    output       M68K_UDS_n,
    output       M68K_LDS_n,
    output       M68K_RW,

    input           M68K_DTACK_n,
    input           M68K_BERR_n,

    input           M68K_VPA_n,
    output reg      M68K_E,
    output       M68K_VMA_n,

    input   [2:0]   M68K_IPL_n,

    inout           M68K_RESET_n,
    inout           M68K_HALT_n,

    input           M68K_BR_n,
    output       M68K_BG_n,
    input           M68K_BGACK_n,
    input           M68K_C1,
    input           M68K_C3,
    input           CLK_SEL
  );

  wire c200m = PI_CLK;
  reg [2:0] c7m_sync;
//  wire c7m = M68K_CLK;
  wire c7m = c7m_sync[2];
  wire c1c3_clk = !(M68K_C1 ^ M68K_C3);

  localparam REG_DATA = 2'd0;
  localparam REG_ADDR_LO = 2'd1;
  localparam REG_ADDR_HI = 2'd2;
  localparam REG_STATUS = 2'd3;

  initial begin
    PI_TXN_IN_PROGRESS <= 1'b0;
    PI_IPL_ZERO <= 1'b0;

    PI_RESET <= 1'b0;

    FC_INT <= 3'b111;

    RW_INT <= 1'b1;

    M68K_E <= 1'b1;
    VMA_INT <= 1'b1;

    BG_INT <= 1'b1;
	 AS_INT <= 1'b1;	 
  end

  reg [1:0] rd_sync;
  reg [1:0] wr_sync;

  always @(posedge c200m) begin
    rd_sync <= {rd_sync[0], PI_RD};
    wr_sync <= {wr_sync[0], PI_WR};
  end

  wire rd_rising = !rd_sync[1] && rd_sync[0];
  wire wr_rising = !wr_sync[1] && wr_sync[0];

  reg [15:0] data_out;
  assign PI_D = PI_A == REG_STATUS && PI_RD ? data_out : 16'bz;

  reg berr_seen = 1'b0;
  always @(posedge c200m) begin
	 if( !M68K_BERR_n ) begin
		berr_seen <= 1'b1;
	 end
    if (rd_rising && PI_A == REG_STATUS) begin
      data_out <= {ipl, 12'd0, berr_seen};
		berr_seen <= 1'b0;
    end
  end

  reg [15:0] status;
  wire reset_out = !status[1];

  assign M68K_RESET_n = reset_out ? 1'b0 : 1'bz;
  assign M68K_HALT_n = reset_out ? 1'b0 : 1'bz;

  reg op_req = 1'b0;
  reg op_rw = 1'b1;
  reg op_uds_n = 1'b1;
  reg op_lds_n = 1'b1;
  
  reg [2:0] FC_INT;
  reg AS_INT;
  reg UDS_INT;
  reg LDS_INT;
  reg RW_INT;
  reg BG_INT;
  reg VMA_INT;

  always @(*) begin
    LTCH_D_WR_U <= PI_A == REG_DATA && PI_WR;
    LTCH_D_WR_L <= PI_A == REG_DATA && PI_WR;

    LTCH_A_0 <= PI_A == REG_ADDR_LO && PI_WR;
    LTCH_A_8 <= PI_A == REG_ADDR_LO && PI_WR;

    LTCH_A_16 <= PI_A == REG_ADDR_HI && PI_WR;
    LTCH_A_24 <= PI_A == REG_ADDR_HI && PI_WR;

    LTCH_D_RD_OE_n <= !(PI_A == REG_DATA && PI_RD);
  end

  reg a0;

  always @(posedge c200m) begin
    c7m_sync <= {c7m_sync[1:0], (CLK_SEL?M68K_CLK:c1c3_clk)};
  end

  wire c7m_rising = !c7m_sync[2] && c7m_sync[1];
  wire c7m_falling = c7m_sync[2] && !c7m_sync[1];

  reg [2:0] ipl;
  reg [2:0] ipl_1;
  reg [2:0] ipl_2;


  always @(posedge c200m) begin
    if (c7m_falling) begin
      ipl_1 <= ~M68K_IPL_n;
      ipl_2 <= ipl_1;
    end

    if (ipl_2 == ipl_1)
      ipl <= ipl_2;

    PI_IPL_ZERO <= ( ( ipl == 3'd0 ) && ( ~M68K_IPL_n == 3'd0 ) );
  end


  always @(posedge c200m) begin
    //PI_RESET <= 1'b1;//reset_out ? 1'b1 : M68K_RESET_n ;//| M68K_HALT_n;
  end

  reg [3:0] e_counter = 4'd0;

  always @(negedge c7m) begin
    if (e_counter == 4'd9)
      e_counter <= 4'd0;
    else
      e_counter <= e_counter + 4'd1;
  end

  always @(negedge c7m) begin
    if (e_counter == 4'd9)
      M68K_E <= 1'b0;
    else if (e_counter == 4'd5)
      M68K_E <= 1'b1;
  end

  reg [2:0] state = 3'd0;
  reg [2:0] PI_TXN_IN_PROGRESS_delay;
  reg [2:0] op_fc = 3'b111;

	reg [2:0] BR_DELAY = 3'b111;
	reg [2:0] BGK_DELAY = 3'b111;
	  
  always @(posedge c200m) begin
  

    if (wr_rising) begin
      case (PI_A)
        REG_ADDR_LO: begin
          a0 <= PI_D[0];
          PI_TXN_IN_PROGRESS <= 1'b1;
        end
        REG_ADDR_HI: begin
          op_req <= 1'b1;
          op_rw <= PI_D[9];
          op_uds_n <= PI_D[8] ? a0 : 1'b0;
          op_lds_n <= PI_D[8] ? !a0 : 1'b0;
			 op_fc <= PI_D[15:13];
        end
        REG_STATUS: begin
          status <= PI_D;
        end
      endcase
    end
	 
	 BR_DELAY = { BR_DELAY[1:0], M68K_BR_n };
	 BGK_DELAY = { BGK_DELAY[1:0], M68K_BGACK_n };
	 
	 /*
    case (state)
      3'd0: begin // S0
        LTCH_D_WR_OE_n <= 1'b1;
        RW_INT <= 1'b1; // S7 -> S0
		  BG_INT <= 1'b1;
		  FC_INT <= 3'b101;
		  if( BR_DELAY[2:1] == 2'b00 )
				BG_INT <= 1'b0;
		  else if( op_req && M68K_BGACK_n && BGK_DELAY[2] ) begin
				FC_INT <= op_fc;
				RW_INT <= op_rw;
				state <= 3'd2;
			end
		end
		
      3'd2: begin // S2
			  LTCH_D_RD_U <= 1'b0;
			  LTCH_D_RD_L <= 1'b0;
			PI_RESET <= 1'b1;
			LTCH_D_WR_OE_n <= op_rw;
			LTCH_A_OE_n <= 1'b0;
			AS_INT <= 1'b0;
			FC_INT <= op_fc;
			UDS_INT <= op_uds_n;
			LDS_INT <= op_lds_n;
			if (c7m_falling) begin
				state <= 3'd3;
			end
      end

      3'd3: begin // S3
        op_req <= 1'b0;
        if(c7m_rising) begin
          if (!M68K_DTACK_n || !M68K_BERR_n || (!VMA_INT && e_counter == 4'd8)) begin
            state <= 3'd4;
            PI_TXN_IN_PROGRESS_delay[2:0] <= 3'b111;
          end
          else begin
            if (!M68K_VPA_n && e_counter == 4'd2) begin
              VMA_INT <= 1'b0;
            end
          end
        end
      end
		
      3'd4: begin // S4
			if( !op_rw )
				PI_TXN_IN_PROGRESS <= 1'b0;
		  PI_RESET <= M68K_BERR_n;
        if (c7m_falling) begin
          state <= 3'd5;
        end
      end

      3'd5: begin // S5
        if (c7m_rising ) begin 
          state <= 3'd6;
        end
      end
		
      3'd6: begin
		
		  PI_TXN_IN_PROGRESS <= 1'b0;
        LTCH_D_RD_U <= 1'b1;
        LTCH_D_RD_L <= 1'b1;

		
//        LTCH_D_WR_OE_n <= 1'b1;
        LTCH_A_OE_n <= 1'b1;
        AS_INT <= 1'b1;
		  FC_INT <= 3'b111;
        UDS_INT <= 1'b1;
        LDS_INT <= 1'b1;
		  VMA_INT <= 1'b1;
		  
          state <= 3'd0;
      end
    endcase
	 */
	 
	    case (state)

      3'd0: begin // S0
        RW_INT <= 1'b1; // S7 -> S0
        state <= 2'd1;
      end

      3'd1: begin // S1
        if (op_req) begin
          if(c7m_rising) begin
            state <= 3'd2;
          end
        end
      end

      3'd2: begin // S2
        PI_RESET <= 1'b1;
		  FC_INT <= op_fc;

        RW_INT <= op_rw; // S1 -> S2
        LTCH_D_WR_OE_n <= op_rw;
        LTCH_A_OE_n <= 1'b0;
        AS_INT <= 1'b0;
        UDS_INT <= op_rw ? op_uds_n : 1'b1;
        LDS_INT <= op_rw ? op_lds_n : 1'b1;
        if (c7m_falling) begin
          UDS_INT <= op_uds_n;
          LDS_INT <= op_lds_n;
          state <= 3'd3;
        end
      end


      3'd3: begin // S3
        op_req <= 1'b0;
        if(c7m_rising) begin
          if (!M68K_DTACK_n || !M68K_BERR_n || (!M68K_VMA_n && e_counter == 4'd8)) begin
            state <= 3'd4;
//            PI_TXN_IN_PROGRESS_delay[2:0] <= 3'b111;
          end
          else begin
            if (!M68K_VPA_n && e_counter == 4'd2) begin
              VMA_INT <= 1'b0;
            end
          end
        end
      end

      3'd4: begin // S4
//        PI_TXN_IN_PROGRESS_delay <= {PI_TXN_IN_PROGRESS_delay[1:0],1'b0};
//        PI_TXN_IN_PROGRESS <= PI_TXN_IN_PROGRESS_delay[2];
		  PI_RESET <= M68K_BERR_n;
        if (c7m_falling) begin
          state <= 3'd5;
          //PI_TXN_IN_PROGRESS <= 1'b0;
        end
      end


      3'd5: begin // S5
        if (c7m_rising) begin
          state <= 3'd6;
        end
      end      

      3'd6: begin // S6
        LTCH_D_RD_U <= 1'b1;
        LTCH_D_RD_L <= 1'b1;
        if (c7m_falling) begin
          VMA_INT <= 1'b1;
          state <= 3'd7;
        end
      end
      

      3'd7: begin // S7
        PI_TXN_IN_PROGRESS <= 1'b0;
        LTCH_D_RD_U <= 1'b0;
        LTCH_D_RD_L <= 1'b0;

        LTCH_D_WR_OE_n <= 1'b1;
        LTCH_A_OE_n <= 1'b1;
        AS_INT <= 1'b1;
        UDS_INT <= 1'b1;
        LDS_INT <= 1'b1;
//        if(c7m_rising) begin
//          RW_INT <= 1'b1; // S7 -> S0
          state <= 3'd0;
//        end
      end
    endcase

	 

	 if( !M68K_RESET_n & !M68K_HALT_n & !reset_out )  begin
		state <= 3'd0;
		PI_TXN_IN_PROGRESS <= 1'b0;
	 end
  end


	assign M68K_FC = M68K_BGACK_n ? FC_INT : 3'bzzz;
	assign M68K_AS_n = M68K_BGACK_n ? AS_INT : 1'bz;
	assign M68K_UDS_n = M68K_BGACK_n ? UDS_INT : 1'bz;
	assign M68K_LDS_n = M68K_BGACK_n ? LDS_INT : 1'bz;
	assign M68K_RW = M68K_BGACK_n ? RW_INT : 1'bz;
	assign M68K_VMA_n = M68K_BGACK_n ? VMA_INT : 1'bz;

	assign M68K_BG_n = 1'b1;//BG_INT;
endmodule
