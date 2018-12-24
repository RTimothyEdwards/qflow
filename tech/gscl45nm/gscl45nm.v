`timescale 1ns/10ps
`celldefine
module AND2X1 (A, B, Y);
input  A ;
input  B ;
output Y ;

   and (Y, A, B);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.34:0.34:0.34,
       tphhl$A$Y = 0.24:0.24:0.24,
       tpllh$B$Y = 0.34:0.34:0.34,
       tphhl$B$Y = 0.25:0.25:0.25;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);
     (B *> Y) = (tpllh$B$Y, tphhl$B$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module AND2X2 (A, B, Y);
input  A ;
input  B ;
output Y ;

   and (Y, A, B);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.062:0.062:0.062,
       tphhl$A$Y = 0.065:0.065:0.065,
       tpllh$B$Y = 0.062:0.062:0.062,
       tphhl$B$Y = 0.07:0.07:0.07;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);
     (B *> Y) = (tpllh$B$Y, tphhl$B$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module AOI21X1 (A, B, C, Y);
input  A ;
input  B ;
input  C ;
output Y ;

   and (I0_out, A, B);
   or  (I1_out, I0_out, C);
   not (Y, I1_out);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.2:0.2:0.2,
       tphlh$A$Y = 0.37:0.37:0.37,
       tplhl$B$Y = 0.2:0.2:0.2,
       tphlh$B$Y = 0.36:0.36:0.36,
       tplhl$C$Y = 0.23:0.23:0.23,
       tphlh$C$Y = 0.26:0.31:0.36;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);
     (C *> Y) = (tphlh$C$Y, tplhl$C$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module AOI22X1 (C, D, Y, B, A);
input  C ;
input  D ;
input  B ;
input  A ;
output Y ;

   and (I0_out, C, D);
   and (I1_out, A, B);
   or  (I2_out, I0_out, I1_out);
   not (Y, I2_out);

   specify
     // delay parameters
     specparam
       tplhl$C$Y = 0.2:0.2:0.2,
       tphlh$C$Y = 0.27:0.32:0.37,
       tplhl$D$Y = 0.2:0.2:0.2,
       tphlh$D$Y = 0.27:0.32:0.37,
       tplhl$B$Y = 0.21:0.21:0.21,
       tphlh$B$Y = 0.3:0.34:0.38,
       tplhl$A$Y = 0.21:0.21:0.21,
       tphlh$A$Y = 0.3:0.34:0.38;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);
     (C *> Y) = (tphlh$C$Y, tplhl$C$Y);
     (D *> Y) = (tphlh$D$Y, tplhl$D$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module BUFX2 (Y, A);
input  A ;
output Y ;

   buf (Y, A);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.064:0.064:0.064,
       tphhl$A$Y = 0.062:0.062:0.062;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module BUFX4 (Y, A);
input  A ;
output Y ;

   buf (Y, A);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.048:0.048:0.048,
       tphhl$A$Y = 0.07:0.07:0.07;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module CLKBUF1 (Y, A);
input  A ;
output Y ;

   buf (Y, A);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.12:0.12:0.12,
       tphhl$A$Y = 0.1:0.1:0.1;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module CLKBUF2 (A, Y);
input  A ;
output Y ;

   buf (Y, A);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.14:0.14:0.14,
       tphhl$A$Y = 0.13:0.13:0.13;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module CLKBUF3 (A, Y);
input  A ;
output Y ;

   buf (Y, A);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.17:0.17:0.17,
       tphhl$A$Y = 0.15:0.15:0.15;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module DFFNEGX1 (Q, CLK, D);
input  CLK ;
input  D ;
output Q ;
reg NOTIFIER ;

   not (I0_CLOCK, CLK);
   udp_dff (DS0000, D, I0_CLOCK, 1'B0, 1'B0, NOTIFIER);
   not (P0000, DS0000);
   buf (Q, DS0000);

   specify
     // delay parameters
     specparam
       tphlh$CLK$Q = 0.22:0.22:0.22,
       tphhl$CLK$Q = 0.19:0.19:0.19,
       tminpwh$CLK = 0.021:0.04:0.06,
       tminpwl$CLK = 0.04:0.13:0.22,
       tsetup_negedge$D$CLK = 0.094:0.094:0.094,
       thold_negedge$D$CLK = 0:0:0,
       tsetup_posedge$D$CLK = 0.094:0.094:0.094,
       thold_posedge$D$CLK = -0.0000000022:-0.0000000022:-0.0000000022;

     // path delays
     if (CLK == 1'b0)
       (CLK *> Q) = (tphlh$CLK$Q, tphhl$CLK$Q);
     $setup(negedge D, negedge CLK, tsetup_negedge$D$CLK, NOTIFIER);
     $hold (negedge CLK, negedge D, thold_negedge$D$CLK,  NOTIFIER);
     $setup(posedge D, negedge CLK, tsetup_posedge$D$CLK, NOTIFIER);
     $hold (negedge CLK, posedge D, thold_posedge$D$CLK,  NOTIFIER);
     $width(posedge CLK, tminpwh$CLK, 0, NOTIFIER);
     $width(negedge CLK, tminpwl$CLK, 0, NOTIFIER);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module DFFPOSX1 (CLK, Q, D);
input  CLK ;
input  D ;
output Q ;
reg NOTIFIER ;

   udp_dff (DS0000, D, CLK, 1'B0, 1'B0, NOTIFIER);
   not (P0000, DS0000);
   buf (Q, DS0000);

   specify
     // delay parameters
     specparam
       tpllh$CLK$Q = 0.22:0.22:0.22,
       tplhl$CLK$Q = 0.2:0.2:0.2,
       tminpwh$CLK = 0.041:0.13:0.22,
       tminpwl$CLK = 0.042:0.055:0.069,
       tsetup_negedge$D$CLK = 0.094:0.094:0.094,
       thold_negedge$D$CLK = 0.0000000022:0.0000000022:0.0000000022,
       tsetup_posedge$D$CLK = 0.094:0.094:0.094,
       thold_posedge$D$CLK = 0:0:0;

     // path delays
     if (CLK == 1'b1)
       (CLK *> Q) = (tpllh$CLK$Q, tplhl$CLK$Q);
     $setup(negedge D, posedge CLK, tsetup_negedge$D$CLK, NOTIFIER);
     $hold (posedge CLK, negedge D, thold_negedge$D$CLK,  NOTIFIER);
     $setup(posedge D, posedge CLK, tsetup_posedge$D$CLK, NOTIFIER);
     $hold (posedge CLK, posedge D, thold_posedge$D$CLK,  NOTIFIER);
     $width(posedge CLK, tminpwh$CLK, 0, NOTIFIER);
     $width(negedge CLK, tminpwl$CLK, 0, NOTIFIER);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module DFFSR (CLK, D, S, R, Q);
input  CLK ;
input  D ;
input  S ;
input  R ;
output Q ;
reg NOTIFIER ;

   not (I0_CLEAR, R);
   not (I0_SET, S);
   udp_dff (P0003, D_, CLK, I0_SET, I0_CLEAR, NOTIFIER);
   not (D_, D);
   not (P0002, P0003);
   buf (Q, P0002);
   and (D_EQ_1_AN_S_EQ_1, D, S);
   not (I9_out, D);
   and (D_EQ_0_AN_R_EQ_1, I9_out, R);
   and (S_EQ_1_AN_R_EQ_1, S, R);

   specify
     // delay parameters
     specparam
       tpllh$CLK$Q = 0.43:0.43:0.43,
       tplhl$CLK$Q = 0.32:0.32:0.32,
       tphlh$S$Q = 0.4:0.4:0.4,
       tpllh$R$Q = 0.35:0.35:0.35,
       tphhl$R$Q = 0.26:0.26:0.26,
       tminpwh$CLK = 0.099:0.26:0.43,
       tminpwl$CLK = 0.11:0.13:0.14,
       tminpwl$S = 0.031:0.21:0.4,
       tminpwl$R = 0.022:0.14:0.26,
       tsetup_negedge$D$CLK = 0.094:0.094:0.094,
       thold_negedge$D$CLK = 0.0000000022:0.0000000022:0.0000000022,
       tsetup_posedge$D$CLK = 0.094:0.094:0.094,
       thold_posedge$D$CLK = 0:0:0,
       trec$R$CLK = 0:0:0,
       trem$R$CLK = 0.19:0.19:0.19,
       trec$R$S = 0:0:0,
       trec$S$CLK = 0:0:0,
       trem$S$CLK = 0.094:0.094:0.094,
       trec$S$R = 0.094:0.094:0.094;

     // path delays
     if (CLK == 1'b1)
       (CLK *> Q) = (tpllh$CLK$Q, tplhl$CLK$Q);
     (R *> Q) = (tpllh$R$Q, tphhl$R$Q);
     (S *> Q) = (tphlh$S$Q, 0);
     $setup(negedge D, posedge CLK &&& S_EQ_1_AN_R_EQ_1 == 1'b1, tsetup_negedge$D$CLK, NOTIFIER);
     $hold (posedge CLK &&& S_EQ_1_AN_R_EQ_1 == 1'b1, negedge D, thold_negedge$D$CLK,  NOTIFIER);
     $setup(posedge D, posedge CLK &&& S_EQ_1_AN_R_EQ_1 == 1'b1, tsetup_posedge$D$CLK, NOTIFIER);
     $hold (posedge CLK &&& S_EQ_1_AN_R_EQ_1 == 1'b1, posedge D, thold_posedge$D$CLK,  NOTIFIER);
     $recovery(posedge R, posedge CLK &&& D_EQ_1_AN_S_EQ_1 == 1'b1, trec$R$CLK, NOTIFIER);
     $removal (posedge R, posedge CLK &&& D_EQ_1_AN_S_EQ_1 == 1'b1, trem$R$CLK, NOTIFIER);
     $recovery(posedge R, posedge S, trec$R$S, NOTIFIER);
     $recovery(posedge S, posedge CLK &&& D_EQ_0_AN_R_EQ_1 == 1'b1, trec$S$CLK, NOTIFIER);
     $removal (posedge S, posedge CLK &&& D_EQ_0_AN_R_EQ_1 == 1'b1, trem$S$CLK, NOTIFIER);
     $recovery(posedge S, posedge R, trec$S$R, NOTIFIER);
     $width(posedge CLK, tminpwh$CLK, 0, NOTIFIER);
     $width(negedge CLK, tminpwl$CLK, 0, NOTIFIER);
     $width(negedge S, tminpwl$S, 0, NOTIFIER);
     $width(negedge R, tminpwl$R, 0, NOTIFIER);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module FAX1 (YC, B, C, A, YS);
input  B ;
input  C ;
input  A ;
output YC ;
output YS ;

   and (I0_out, A, B);
   and (I1_out, B, C);
   and (I3_out, C, A);
   or  (YC, I0_out, I1_out, I3_out);
   xor (I5_out, A, B);
   xor (YS, I5_out, C);

   specify
     // delay parameters
     specparam
       tpllh$B$YS = 0.35:0.38:0.41,
       tplhl$B$YS = 0.27:0.29:0.3,
       tpllh$B$YC = 0.36:0.36:0.37,
       tphhl$B$YC = 0.28:0.28:0.29,
       tpllh$C$YS = 0.35:0.37:0.4,
       tplhl$C$YS = 0.28:0.28:0.29,
       tpllh$C$YC = 0.35:0.36:0.36,
       tphhl$C$YC = 0.27:0.28:0.28,
       tpllh$A$YS = 0.36:0.39:0.41,
       tplhl$A$YS = 0.28:0.29:0.3,
       tpllh$A$YC = 0.36:0.36:0.36,
       tphhl$A$YC = 0.28:0.28:0.28;

     // path delays
     (A *> YC) = (tpllh$A$YC, tphhl$A$YC);
     (A *> YS) = (tpllh$A$YS, tplhl$A$YS);
     (B *> YC) = (tpllh$B$YC, tphhl$B$YC);
     (B *> YS) = (tpllh$B$YS, tplhl$B$YS);
     (C *> YC) = (tpllh$C$YC, tphhl$C$YC);
     (C *> YS) = (tpllh$C$YS, tplhl$C$YS);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module FILL ();
endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module HAX1 (YS, B, A, YC);
input  B ;
input  A ;
output YS ;
output YC ;

   xor (YS, A, B);
   and (YC, A, B);

   specify
     // delay parameters
     specparam
       tpllh$B$YS = 0.34:0.36:0.37,
       tplhl$B$YS = 0.25:0.26:0.27,
       tpllh$B$YC = 0.35:0.35:0.35,
       tphhl$B$YC = 0.26:0.26:0.26,
       tpllh$A$YS = 0.35:0.36:0.38,
       tplhl$A$YS = 0.26:0.26:0.27,
       tpllh$A$YC = 0.34:0.34:0.34,
       tphhl$A$YC = 0.26:0.26:0.26;

     // path delays
     (A *> YC) = (tpllh$A$YC, tphhl$A$YC);
     (A *> YS) = (tpllh$A$YS, tplhl$A$YS);
     (B *> YC) = (tpllh$B$YC, tphhl$B$YC);
     (B *> YS) = (tpllh$B$YS, tplhl$B$YS);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module INVX1 (A, Y);
input  A ;
output Y ;

   not (Y, A);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.23:0.23:0.23,
       tphlh$A$Y = 0.33:0.33:0.33;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module INVX2 (A, Y);
input  A ;
output Y ;

   not (Y, A);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.039:0.039:0.039,
       tphlh$A$Y = 0.054:0.054:0.054;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module INVX4 (Y, A);
input  A ;
output Y ;

   not (Y, A);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.04:0.04:0.04,
       tphlh$A$Y = 0.054:0.054:0.054;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module INVX8 (A, Y);
input  A ;
output Y ;

   not (Y, A);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.04:0.04:0.04,
       tphlh$A$Y = 0.054:0.054:0.054;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module LATCH (D, CLK, Q);
input  D ;
input  CLK ;
output Q ;
reg NOTIFIER ;

   udp_tlat (DS0000, D, CLK, 1'B0, 1'B0, NOTIFIER);
   not (P0000, DS0000);
   buf (Q, DS0000);

   specify
     // delay parameters
     specparam
       tpllh$D$Q = 0.2:0.2:0.2,
       tphhl$D$Q = 0.18:0.18:0.18,
       tpllh$CLK$Q = 0.2:0.2:0.2,
       tplhl$CLK$Q = 0.17:0.17:0.17,
       tminpwh$CLK = 0.025:0.11:0.2,
       tsetup_negedge$D$CLK = 0.19:0.19:0.19,
       thold_negedge$D$CLK = 0:0:0,
       tsetup_posedge$D$CLK = 0.19:0.19:0.19,
       thold_posedge$D$CLK = -0.0000000022:-0.0000000022:-0.0000000022;

     // path delays
     if (CLK == 1'b1)
       (CLK *> Q) = (tpllh$CLK$Q, tplhl$CLK$Q);
     (D *> Q) = (tpllh$D$Q, tphhl$D$Q);
     $setup(negedge D, negedge CLK, tsetup_negedge$D$CLK, NOTIFIER);
     $hold (negedge CLK, negedge D, thold_negedge$D$CLK,  NOTIFIER);
     $setup(posedge D, negedge CLK, tsetup_posedge$D$CLK, NOTIFIER);
     $hold (negedge CLK, posedge D, thold_posedge$D$CLK,  NOTIFIER);
     $width(posedge CLK, tminpwh$CLK, 0, NOTIFIER);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module MUX2X1 (A, Y, S, B);
input  A ;
input  S ;
input  B ;
output Y ;

   udp_mux2 (I0_out, B, A, S);
   not (Y, I0_out);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.21:0.21:0.21,
       tphlh$A$Y = 0.37:0.37:0.37,
       tpllh$S$Y = 0.36:0.37:0.37,
       tplhl$S$Y = 0.2:0.21:0.22,
       tplhl$B$Y = 0.2:0.2:0.2,
       tphlh$B$Y = 0.37:0.37:0.37;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);
     (S *> Y) = (tpllh$S$Y, tplhl$S$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module NAND2X1 (A, B, Y);
input  A ;
input  B ;
output Y ;

   and (I0_out, A, B);
   not (Y, I0_out);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.2:0.2:0.2,
       tphlh$A$Y = 0.35:0.35:0.35,
       tplhl$B$Y = 0.2:0.2:0.2,
       tphlh$B$Y = 0.34:0.34:0.34;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module NAND3X1 (A, B, C, Y);
input  A ;
input  B ;
input  C ;
output Y ;

   and (I1_out, A, B, C);
   not (Y, I1_out);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.19:0.19:0.19,
       tphlh$A$Y = 0.35:0.35:0.35,
       tplhl$B$Y = 0.19:0.19:0.19,
       tphlh$B$Y = 0.35:0.35:0.35,
       tplhl$C$Y = 0.19:0.19:0.19,
       tphlh$C$Y = 0.35:0.35:0.35;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);
     (C *> Y) = (tphlh$C$Y, tplhl$C$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module NOR2X1 (A, B, Y);
input  A ;
input  B ;
output Y ;

   or  (I0_out, A, B);
   not (Y, I0_out);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.23:0.23:0.23,
       tphlh$A$Y = 0.36:0.36:0.36,
       tplhl$B$Y = 0.23:0.23:0.23,
       tphlh$B$Y = 0.36:0.36:0.36;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module NOR3X1 (A, B, C, Y);
input  A ;
input  B ;
input  C ;
output Y ;

   or  (I1_out, A, B, C);
   not (Y, I1_out);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.25:0.25:0.25,
       tphlh$A$Y = 0.39:0.39:0.39,
       tplhl$B$Y = 0.24:0.24:0.24,
       tphlh$B$Y = 0.38:0.38:0.38,
       tplhl$C$Y = 0.23:0.23:0.23,
       tphlh$C$Y = 0.36:0.36:0.36;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);
     (C *> Y) = (tphlh$C$Y, tplhl$C$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module OAI21X1 (A, B, C, Y);
input  A ;
input  B ;
input  C ;
output Y ;

   or  (I0_out, A, B);
   and (I1_out, I0_out, C);
   not (Y, I1_out);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.2:0.2:0.2,
       tphlh$A$Y = 0.37:0.37:0.37,
       tplhl$B$Y = 0.2:0.2:0.2,
       tphlh$B$Y = 0.37:0.37:0.37,
       tplhl$C$Y = 0.15:0.18:0.2,
       tphlh$C$Y = 0.34:0.34:0.34;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);
     (C *> Y) = (tphlh$C$Y, tplhl$C$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module OAI22X1 (C, D, Y, B, A);
input  C ;
input  D ;
input  B ;
input  A ;
output Y ;

   or  (I0_out, C, D);
   or  (I1_out, A, B);
   and (I2_out, I0_out, I1_out);
   not (Y, I2_out);

   specify
     // delay parameters
     specparam
       tplhl$C$Y = 0.16:0.19:0.21,
       tphlh$C$Y = 0.37:0.37:0.37,
       tplhl$D$Y = 0.16:0.18:0.2,
       tphlh$D$Y = 0.36:0.36:0.36,
       tplhl$B$Y = 0.16:0.18:0.21,
       tphlh$B$Y = 0.37:0.37:0.38,
       tplhl$A$Y = 0.16:0.19:0.21,
       tphlh$A$Y = 0.38:0.38:0.38;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);
     (C *> Y) = (tphlh$C$Y, tplhl$C$Y);
     (D *> Y) = (tphlh$D$Y, tplhl$D$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module OR2X1 (A, B, Y);
input  A ;
input  B ;
output Y ;

   or  (Y, A, B);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.34:0.34:0.34,
       tphhl$A$Y = 0.24:0.24:0.24,
       tpllh$B$Y = 0.35:0.35:0.35,
       tphhl$B$Y = 0.25:0.25:0.25;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);
     (B *> Y) = (tpllh$B$Y, tphhl$B$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module OR2X2 (A, B, Y);
input  A ;
input  B ;
output Y ;

   or  (Y, A, B);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.069:0.069:0.069,
       tphhl$A$Y = 0.066:0.066:0.066,
       tpllh$B$Y = 0.076:0.076:0.076,
       tphhl$B$Y = 0.074:0.074:0.074;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);
     (B *> Y) = (tpllh$B$Y, tphhl$B$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module TBUFX1 (EN, A, Y);
input  EN ;
input  A ;
output Y ;

   not (I0_out, A);
   bufif1 (Y, I0_out, EN);

   specify
     // delay parameters
     specparam
       tpzh$EN$Y = 0.36:0.36:0.36,
       tpzl$EN$Y = 0.2:0.2:0.2,
       tplz$EN$Y = 0.011:0.011:0.011,
       tphz$EN$Y = 0.027:0.027:0.027,
       tplhl$A$Y = 0.2:0.2:0.2,
       tphlh$A$Y = 0.37:0.37:0.37;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (EN *> Y) = (0, 0, tplz$EN$Y, tpzh$EN$Y, tphz$EN$Y, tpzl$EN$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module TBUFX2 (EN, Y, A);
input  EN ;
input  A ;
output Y ;

   not (I0_out, A);
   bufif1 (Y, I0_out, EN);

   specify
     // delay parameters
     specparam
       tpzh$EN$Y = 0.057:0.057:0.057,
       tpzl$EN$Y = 0.036:0.036:0.036,
       tplz$EN$Y = 0.011:0.011:0.011,
       tphz$EN$Y = 0.025:0.025:0.025,
       tplhl$A$Y = 0.042:0.042:0.042,
       tphlh$A$Y = 0.069:0.069:0.069;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (EN *> Y) = (0, 0, tplz$EN$Y, tpzh$EN$Y, tphz$EN$Y, tpzl$EN$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module XNOR2X1 (B, Y, A);
input  B ;
input  A ;
output Y ;

   xor (I0_out, A, B);
   not (Y, I0_out);

   specify
     // delay parameters
     specparam
       tpllh$B$Y = 0.37:0.37:0.37,
       tplhl$B$Y = 0.2:0.21:0.22,
       tpllh$A$Y = 0.36:0.36:0.36,
       tplhl$A$Y = 0.2:0.21:0.21;

     // path delays
     (A *> Y) = (tpllh$A$Y, tplhl$A$Y);
     (B *> Y) = (tpllh$B$Y, tplhl$B$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module XOR2X1 (B, Y, A);
input  B ;
input  A ;
output Y ;

   xor (Y, A, B);

   specify
     // delay parameters
     specparam
       tpllh$B$Y = 0.37:0.37:0.37,
       tplhl$B$Y = 0.2:0.21:0.22,
       tpllh$A$Y = 0.36:0.36:0.36,
       tplhl$A$Y = 0.2:0.21:0.21;

     // path delays
     (A *> Y) = (tpllh$A$Y, tplhl$A$Y);
     (B *> Y) = (tpllh$B$Y, tplhl$B$Y);

   endspecify

endmodule
`endcelldefine

primitive udp_dff (out, in, clk, clr, set, NOTIFIER);
   output out;
   input  in, clk, clr, set, NOTIFIER;
   reg    out;

   table

// in  clk  clr   set  NOT  : Qt : Qt+1
//
   0  r   ?   0   ?   : ?  :  0  ; // clock in 0
   1  r   0   ?   ?   : ?  :  1  ; // clock in 1
   1  *   0   ?   ?   : 1  :  1  ; // reduce pessimism
   0  *   ?   0   ?   : 0  :  0  ; // reduce pessimism
   ?  f   ?   ?   ?   : ?  :  -  ; // no changes on negedge clk
   *  b   ?   ?   ?   : ?  :  -  ; // no changes when in switches
   ?  ?   ?   1   ?   : ?  :  1  ; // set output
   ?  b   0   *   ?   : 1  :  1  ; // cover all transistions on set
   1  x   0   *   ?   : 1  :  1  ; // cover all transistions on set
   ?  ?   1   0   ?   : ?  :  0  ; // reset output
   ?  b   *   0   ?   : 0  :  0  ; // cover all transistions on clr
   0  x   *   0   ?   : 0  :  0  ; // cover all transistions on clr
   ?  ?   ?   ?   *   : ?  :  x  ; // any notifier changed

   endtable
endprimitive // udp_dff

primitive udp_tlat (out, in, enable, clr, set, NOTIFIER);

   output out;
   input  in, enable, clr, set, NOTIFIER;
   reg    out;

   table

// in  enable  clr   set  NOT  : Qt : Qt+1
//
   1  1   0   ?   ?   : ?  :  1  ; //
   0  1   ?   0   ?   : ?  :  0  ; //
   1  *   0   ?   ?   : 1  :  1  ; // reduce pessimism
   0  *   ?   0   ?   : 0  :  0  ; // reduce pessimism
   *  0   ?   ?   ?   : ?  :  -  ; // no changes when in switches
   ?  ?   ?   1   ?   : ?  :  1  ; // set output
   ?  0   0   *   ?   : 1  :  1  ; // cover all transistions on set
   1  ?   0   *   ?   : 1  :  1  ; // cover all transistions on set
   ?  ?   1   0   ?   : ?  :  0  ; // reset output
   ?  0   *   0   ?   : 0  :  0  ; // cover all transistions on clr
   0  ?   *   0   ?   : 0  :  0  ; // cover all transistions on clr
   ?  ?   ?   ?   *   : ?  :  x  ; // any notifier changed

   endtable
endprimitive // udp_tlat

primitive udp_rslat (out, clr, set, NOTIFIER);

   output out;
   input  clr, set, NOTIFIER;
   reg    out;

   table

// clr   set  NOT  : Qt : Qt+1
//
   ?   1   ?   : ?  :  1  ; // set output
   0   *   ?   : 1  :  1  ; // cover all transistions on set
   1   0   ?   : ?  :  0  ; // reset output
   *   0   ?   : 0  :  0  ; // cover all transistions on clr
   ?   ?   *   : ?  :  x  ; // any notifier changed

   endtable
endprimitive // udp_tlat

primitive udp_mux2 (out, in0, in1, sel);
   output out;
   input  in0, in1, sel;

   table

// in0 in1 sel :  out
//
    1  ?  0 :  1 ;
    0  ?  0 :  0 ;
    ?  1  1 :  1 ;
    ?  0  1 :  0 ;
    0  0  x :  0 ;
    1  1  x :  1 ;

   endtable
endprimitive // udp_mux2

