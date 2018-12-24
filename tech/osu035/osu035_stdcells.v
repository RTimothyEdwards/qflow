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
       tpllh$B$Y = 0.11:0.11:0.11,
       tphhl$B$Y = 0.14:0.14:0.14,
       tpllh$A$Y = 0.11:0.11:0.11,
       tphhl$A$Y = 0.12:0.12:0.12;

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
       tpllh$A$Y = 0.13:0.13:0.13,
       tphhl$A$Y = 0.15:0.15:0.15,
       tpllh$B$Y = 0.13:0.13:0.13,
       tphhl$B$Y = 0.17:0.17:0.17;

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
       tplhl$A$Y = 0.088:0.088:0.088,
       tphlh$A$Y = 0.099:0.099:0.099,
       tplhl$B$Y = 0.086:0.086:0.086,
       tphlh$B$Y = 0.084:0.084:0.084,
       tplhl$C$Y = 0.068:0.071:0.074,
       tphlh$C$Y = 0.051:0.068:0.085;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);
     (C *> Y) = (tphlh$C$Y, tplhl$C$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module AOI22X1 (A, B, C, D, Y);
input  A ;
input  B ;
input  C ;
input  D ;
output Y ;

   and (I0_out, A, B);
   and (I1_out, C, D);
   or  (I2_out, I0_out, I1_out);
   not (Y, I2_out);

   specify
     // delay parameters
     specparam
       tplhl$C$Y = 0.066:0.069:0.071,
       tphlh$C$Y = 0.073:0.091:0.11,
       tplhl$D$Y = 0.065:0.068:0.071,
       tphlh$D$Y = 0.061:0.078:0.095,
       tplhl$A$Y = 0.095:0.11:0.12,
       tphlh$A$Y = 0.095:0.11:0.13,
       tplhl$B$Y = 0.094:0.11:0.12,
       tphlh$B$Y = 0.087:0.1:0.11;

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
module BUFX2 (A, Y);
input  A ;
output Y ;

   buf (Y, A);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.13:0.13:0.13,
       tphhl$A$Y = 0.15:0.15:0.15;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module BUFX4 (A, Y);
input  A ;
output Y ;

   buf (Y, A);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.15:0.15:0.15,
       tphhl$A$Y = 0.16:0.16:0.16;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module CLKBUF1 (A, Y);
input  A ;
output Y ;

   buf (Y, A);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.22:0.22:0.22,
       tphhl$A$Y = 0.23:0.23:0.23;

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
       tpllh$A$Y = 0.32:0.32:0.32,
       tphhl$A$Y = 0.33:0.33:0.33;

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
       tpllh$A$Y = 0.42:0.42:0.42,
       tphhl$A$Y = 0.43:0.43:0.43;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module DFFNEGX1 (CLK, D, Q);
input  CLK ;
input  D ;
output Q ;
reg NOTIFIER ;

   not (I0_CLOCK, CLK);
   udp_dff (DS0000, D, I0_CLOCK, 1'B0, 1'B0, NOTIFIER);
   not (P0002, DS0000);
   buf (Q, DS0000);

   specify
     // delay parameters
     specparam
       tphlh$CLK$Q = 0.2:0.2:0.2,
       tphhl$CLK$Q = 0.2:0.2:0.2,
       tminpwh$CLK = 0.061:0.14:0.22,
       tminpwl$CLK = 0.13:0.16:0.2,
       tsetup_negedge$D$CLK = 0.28:0.28:0.28,
       thold_negedge$D$CLK = -0.094:-0.094:-0.094,
       tsetup_posedge$D$CLK = 0.28:0.28:0.28,
       thold_posedge$D$CLK = -0.094:-0.094:-0.094;

     // path delays
     (CLK *> Q) = (tphlh$CLK$Q, tphhl$CLK$Q);
     $setup(negedge D, negedge CLK, tsetup_negedge$D$CLK, NOTIFIER);
     $hold (negedge D, negedge CLK, thold_negedge$D$CLK,  NOTIFIER);
     $setup(posedge D, negedge CLK, tsetup_posedge$D$CLK, NOTIFIER);
     $hold (posedge D, negedge CLK, thold_posedge$D$CLK,  NOTIFIER);
     $width(posedge CLK, tminpwh$CLK, 0, NOTIFIER);
     $width(negedge CLK, tminpwl$CLK, 0, NOTIFIER);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module DFFPOSX1 (CLK, D, Q);
input  CLK ;
input  D ;
output Q ;
reg NOTIFIER ;

   udp_dff (DS0000, D, CLK, 1'B0, 1'B0, NOTIFIER);
   not (P0002, DS0000);
   buf (Q, DS0000);

   specify
     // delay parameters
     specparam
       tpllh$CLK$Q = 0.16:0.16:0.16,
       tplhl$CLK$Q = 0.25:0.25:0.25,
       tminpwh$CLK = 0.081:0.17:0.25,
       tminpwl$CLK = 0.079:0.15:0.22,
       tsetup_negedge$D$CLK = 0.28:0.28:0.28,
       thold_negedge$D$CLK = -0.094:-0.094:-0.094,
       tsetup_posedge$D$CLK = 0.28:0.28:0.28,
       thold_posedge$D$CLK = -0.094:-0.094:-0.094;

     // path delays
     (CLK *> Q) = (tpllh$CLK$Q, tplhl$CLK$Q);
     $setup(negedge D, posedge CLK, tsetup_negedge$D$CLK, NOTIFIER);
     $hold (negedge D, posedge CLK, thold_negedge$D$CLK,  NOTIFIER);
     $setup(posedge D, posedge CLK, tsetup_posedge$D$CLK, NOTIFIER);
     $hold (posedge D, posedge CLK, thold_posedge$D$CLK,  NOTIFIER);
     $width(posedge CLK, tminpwh$CLK, 0, NOTIFIER);
     $width(negedge CLK, tminpwl$CLK, 0, NOTIFIER);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module DFFSR (CLK, D, R, S, Q);
input  CLK ;
input  D ;
input  R ;
input  S ;
output Q ;
reg NOTIFIER ;

   not (I0_CLEAR, R);
   not (I0_SET, S);
   udp_dff (P0003, D_, CLK, I0_SET, I0_CLEAR, NOTIFIER);
   not (D_, D);
   not (P0002, P0003);
   buf (Q, P0002);
   and (\D&S , D, S);
   not (I7_out, D);
   and (\~D&R , I7_out, R);
   and (\S&R , S, R);

   specify
     // delay parameters
     specparam
       tphlh$S$Q = 0.45:0.45:0.45,
       tpllh$R$Q = 0.32:0.32:0.32,
       tphhl$R$Q = 0.34:0.34:0.35,
       tpllh$CLK$Q = 0.52:0.52:0.52,
       tplhl$CLK$Q = 0.52:0.52:0.52,
       tminpwl$S = 0.062:0.26:0.45,
       tminpwl$R = 0.038:0.19:0.35,
       tminpwh$CLK = 0.27:0.39:0.52,
       tminpwl$CLK = 0.26:0.29:0.32,
       tsetup_negedge$D$CLK = 0.094:0.094:0.094,
       thold_negedge$D$CLK = 0.000000059:0.000000059:0.000000059,
       tsetup_posedge$D$CLK = 0.19:0.19:0.19,
       thold_posedge$D$CLK = 0.000000061:0.000000061:0.000000061,
       trec$R$CLK = -0.094:-0.094:-0.094,
       trem$R$CLK = 0.28:0.28:0.28,
       trec$R$S = 0.000000061:0.000000061:0.000000061,
       trec$S$CLK = 0:0:0,
       trem$S$CLK = 0.094:0.094:0.094,
       trec$S$R = 0.094:0.094:0.094;

     // path delays
     (CLK *> Q) = (tpllh$CLK$Q, tplhl$CLK$Q);
     (R *> Q) = (tpllh$R$Q, tphhl$R$Q);
     (S *> Q) = (tphlh$S$Q, 0);
     $setup(negedge D, posedge CLK &&& \S&R , tsetup_negedge$D$CLK, NOTIFIER);
     $hold (negedge D, posedge CLK &&& \S&R , thold_negedge$D$CLK,  NOTIFIER);
     $setup(posedge D, posedge CLK &&& \S&R , tsetup_posedge$D$CLK, NOTIFIER);
     $hold (posedge D, posedge CLK &&& \S&R , thold_posedge$D$CLK,  NOTIFIER);
     $recovery(posedge R, posedge CLK &&& \D&S , trec$R$CLK, NOTIFIER);
     $removal (posedge R, posedge CLK &&& \D&S , trem$R$CLK, NOTIFIER);
     $recovery(posedge R, posedge S, trec$R$S, NOTIFIER);
     $recovery(posedge S, posedge CLK &&& \~D&R , trec$S$CLK, NOTIFIER);
     $removal (posedge S, posedge CLK &&& \~D&R , trem$S$CLK, NOTIFIER);
     $recovery(posedge S, posedge R, trec$S$R, NOTIFIER);
     $width(negedge S, tminpwl$S, 0, NOTIFIER);
     $width(negedge R, tminpwl$R, 0, NOTIFIER);
     $width(posedge CLK, tminpwh$CLK, 0, NOTIFIER);
     $width(negedge CLK, tminpwl$CLK, 0, NOTIFIER);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module FAX1 (A, B, C, YC, YS);
input  A ;
input  B ;
input  C ;
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
       tpllh$A$YS = 0.29:0.31:0.33,
       tplhl$A$YS = 0.3:0.3:0.3,
       tpllh$A$YC = 0.19:0.19:0.19,
       tphhl$A$YC = 0.21:0.21:0.21,
       tpllh$B$YS = 0.3:0.33:0.35,
       tplhl$B$YS = 0.29:0.31:0.33,
       tpllh$B$YC = 0.18:0.2:0.22,
       tphhl$B$YC = 0.22:0.22:0.22,
       tpllh$C$YS = 0.31:0.32:0.33,
       tplhl$C$YS = 0.29:0.3:0.3,
       tpllh$C$YC = 0.17:0.19:0.2,
       tphhl$C$YC = 0.2:0.2:0.2;

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
module HAX1 (A, B, YC, YS);
input  A ;
input  B ;
output YC ;
output YS ;

   and (YC, A, B);
   xor (YS, A, B);

   specify
     // delay parameters
     specparam
       tpllh$A$YS = 0.23:0.23:0.23,
       tplhl$A$YS = 0.25:0.25:0.25,
       tpllh$A$YC = 0.14:0.14:0.14,
       tphhl$A$YC = 0.18:0.18:0.18,
       tpllh$B$YS = 0.22:0.22:0.22,
       tplhl$B$YS = 0.25:0.25:0.25,
       tpllh$B$YC = 0.14:0.14:0.14,
       tphhl$B$YC = 0.16:0.16:0.16;

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
       tplhl$A$Y = 0.053:0.053:0.053,
       tphlh$A$Y = 0.058:0.058:0.058;

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
       tplhl$A$Y = 0.053:0.053:0.053,
       tphlh$A$Y = 0.058:0.058:0.058;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module INVX4 (A, Y);
input  A ;
output Y ;

   not (Y, A);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.053:0.053:0.053,
       tphlh$A$Y = 0.058:0.058:0.058;

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
       tplhl$A$Y = 0.053:0.053:0.053,
       tphlh$A$Y = 0.058:0.058:0.058;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module LATCH (CLK, D, Q);
input  CLK ;
input  D ;
output Q ;
reg NOTIFIER ;

   udp_tlat (DS0000, D, CLK, 1'B0, 1'B0, NOTIFIER);
   not (P0000, DS0000);
   buf (Q, DS0000);

   specify
     // delay parameters
     specparam
       tpllh$D$Q = 0.3:0.3:0.3,
       tphhl$D$Q = 0.34:0.34:0.34,
       tpllh$CLK$Q = 0.25:0.25:0.25,
       tplhl$CLK$Q = 0.34:0.34:0.34,
       tminpwh$CLK = 0.079:0.21:0.34,
       tsetup_negedge$D$CLK = 0.28:0.28:0.28,
       thold_negedge$D$CLK = -0.094:-0.094:-0.094,
       tsetup_posedge$D$CLK = 0.28:0.28:0.28,
       thold_posedge$D$CLK = -0.094:-0.094:-0.094;

     // path delays
     (CLK *> Q) = (tpllh$CLK$Q, tplhl$CLK$Q);
     (D *> Q) = (tpllh$D$Q, tphhl$D$Q);
     $setup(negedge D, negedge CLK, tsetup_negedge$D$CLK, NOTIFIER);
     $hold (negedge D, negedge CLK, thold_negedge$D$CLK,  NOTIFIER);
     $setup(posedge D, negedge CLK, tsetup_posedge$D$CLK, NOTIFIER);
     $hold (posedge D, negedge CLK, thold_posedge$D$CLK,  NOTIFIER);
     $width(posedge CLK, tminpwh$CLK, 0, NOTIFIER);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module MUX2X1 (A, B, S, Y);
input  A ;
input  B ;
input  S ;
output Y ;

   udp_mux2 (I0_out, B, A, S);
   not (Y, I0_out);

   specify
     // delay parameters
     specparam
       tpllh$S$Y = 0.15:0.15:0.15,
       tplhl$S$Y = 0.16:0.16:0.16,
       tplhl$A$Y = 0.082:0.082:0.082,
       tphlh$A$Y = 0.11:0.11:0.11,
       tplhl$B$Y = 0.094:0.094:0.094,
       tphlh$B$Y = 0.096:0.096:0.096;

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
       tplhl$A$Y = 0.051:0.051:0.051,
       tphlh$A$Y = 0.084:0.084:0.084,
       tplhl$B$Y = 0.051:0.051:0.051,
       tphlh$B$Y = 0.069:0.069:0.069;

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
       tplhl$B$Y = 0.06:0.06:0.06,
       tphlh$B$Y = 0.1:0.1:0.1,
       tplhl$A$Y = 0.063:0.063:0.063,
       tphlh$A$Y = 0.12:0.12:0.12,
       tplhl$C$Y = 0.053:0.053:0.053,
       tphlh$C$Y = 0.076:0.076:0.076;

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
       tplhl$B$Y = 0.07:0.07:0.07,
       tphlh$B$Y = 0.063:0.063:0.063,
       tplhl$A$Y = 0.094:0.094:0.094,
       tphlh$A$Y = 0.075:0.075:0.075;

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
       tplhl$B$Y = 0.13:0.13:0.13,
       tphlh$B$Y = 0.099:0.099:0.099,
       tplhl$C$Y = 0.083:0.083:0.083,
       tphlh$C$Y = 0.064:0.064:0.064,
       tplhl$A$Y = 0.15:0.15:0.15,
       tphlh$A$Y = 0.11:0.11:0.11;

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
       tplhl$A$Y = 0.087:0.087:0.087,
       tphlh$A$Y = 0.098:0.098:0.098,
       tplhl$B$Y = 0.064:0.064:0.064,
       tphlh$B$Y = 0.09:0.09:0.09,
       tplhl$C$Y = 0.046:0.062:0.079,
       tphlh$C$Y = 0.07:0.076:0.081;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (B *> Y) = (tphlh$B$Y, tplhl$B$Y);
     (C *> Y) = (tphlh$C$Y, tplhl$C$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module OAI22X1 (A, B, C, D, Y);
input  A ;
input  B ;
input  C ;
input  D ;
output Y ;

   or  (I0_out, A, B);
   or  (I1_out, C, D);
   and (I2_out, I0_out, I1_out);
   not (Y, I2_out);

   specify
     // delay parameters
     specparam
       tplhl$D$Y = 0.057:0.076:0.096,
       tphlh$D$Y = 0.078:0.082:0.086,
       tplhl$C$Y = 0.072:0.094:0.12,
       tphlh$C$Y = 0.085:0.09:0.094,
       tplhl$A$Y = 0.08:0.1:0.12,
       tphlh$A$Y = 0.11:0.11:0.12,
       tplhl$B$Y = 0.063:0.082:0.1,
       tphlh$B$Y = 0.096:0.1:0.11;

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
       tpllh$B$Y = 0.15:0.15:0.15,
       tphhl$B$Y = 0.13:0.13:0.13,
       tpllh$A$Y = 0.12:0.12:0.12,
       tphhl$A$Y = 0.12:0.12:0.12;

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
       tpllh$B$Y = 0.18:0.18:0.18,
       tphhl$B$Y = 0.16:0.16:0.16,
       tpllh$A$Y = 0.15:0.15:0.15,
       tphhl$A$Y = 0.15:0.15:0.15;

     // path delays
     (A *> Y) = (tpllh$A$Y, tphhl$A$Y);
     (B *> Y) = (tpllh$B$Y, tphhl$B$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module PADINC (YPAD, DI);
input  YPAD ;
output DI ;

   buf (DI, YPAD);

   specify
     // delay parameters
     specparam
       tpllh$YPAD$DI = 0.15:0.15:0.15,
       tphhl$YPAD$DI = 0.17:0.17:0.17;

     // path delays
     (YPAD *> DI) = (tpllh$YPAD$DI, tphhl$YPAD$DI);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module PADINOUT (DO, OEN, DI, YPAD);
input  DO ;
input  OEN ;
output DI ;
inout  YPAD ;

   bufif1 (YPAD, DO, OEN);
   buf (DI, YPAD);

   specify
     // delay parameters
     specparam
       tpllh$DO$YPAD = 0.59:0.59:0.59,
       tphhl$DO$YPAD = 0.59:0.59:0.59,
       tpzh$OEN$YPAD = 1.1:1.1:1.1,
       tpzl$OEN$YPAD = 0.65:0.65:0.65,
       tplz$OEN$YPAD = 0.82:0.82:0.82,
       tphz$OEN$YPAD = 1.5:1.5:1.5,
       tpllh$YPAD$DI = 0.15:0.15:0.15,
       tphhl$YPAD$DI = 0.17:0.17:0.17;

     // path delays
     (DO *> YPAD) = (tpllh$DO$YPAD, tphhl$DO$YPAD);
     (OEN *> YPAD) = (0, 0, tplz$OEN$YPAD, tpzh$OEN$YPAD, tphz$OEN$YPAD, tpzl$OEN$YPAD);
     (YPAD *> DI) = (tpllh$YPAD$DI, tphhl$YPAD$DI);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module PADOUT (DO, YPAD);
input  DO ;
output YPAD ;

   buf (YPAD, DO);

   specify
     // delay parameters
     specparam
       tpllh$DO$YPAD = 0.59:0.59:0.59,
       tphhl$DO$YPAD = 0.59:0.59:0.59;

     // path delays
     (DO *> YPAD) = (tpllh$DO$YPAD, tphhl$DO$YPAD);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module TBUFX1 (A, EN, Y);
input  A ;
input  EN ;
output Y ;

   not (I0_out, A);
   bufif1 (Y, I0_out, EN);

   specify
     // delay parameters
     specparam
       tpzh$EN$Y = 0.095:0.095:0.095,
       tpzl$EN$Y = 0.037:0.037:0.037,
       tplz$EN$Y = 0.052:0.052:0.052,
       tphz$EN$Y = 0.094:0.094:0.094,
       tplhl$A$Y = 0.077:0.077:0.077,
       tphlh$A$Y = 0.099:0.099:0.099;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (EN *> Y) = (0, 0, tplz$EN$Y, tpzh$EN$Y, tphz$EN$Y, tpzl$EN$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module TBUFX2 (A, EN, Y);
input  A ;
input  EN ;
output Y ;

   not (I0_out, A);
   bufif1 (Y, I0_out, EN);

   specify
     // delay parameters
     specparam
       tplhl$A$Y = 0.077:0.077:0.077,
       tphlh$A$Y = 0.099:0.099:0.099,
       tpzh$EN$Y = 0.096:0.096:0.096,
       tpzl$EN$Y = 0.037:0.037:0.037,
       tplz$EN$Y = 0.052:0.052:0.052,
       tphz$EN$Y = 0.094:0.094:0.094;

     // path delays
     (A *> Y) = (tphlh$A$Y, tplhl$A$Y);
     (EN *> Y) = (0, 0, tplz$EN$Y, tpzh$EN$Y, tphz$EN$Y, tpzl$EN$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module XNOR2X1 (A, B, Y);
input  A ;
input  B ;
output Y ;

   xor (I0_out, A, B);
   not (Y, I0_out);

   specify
     // delay parameters
     specparam
       tpllh$A$Y = 0.12:0.12:0.12,
       tplhl$A$Y = 0.12:0.12:0.12,
       tpllh$B$Y = 0.15:0.15:0.15,
       tplhl$B$Y = 0.14:0.14:0.14;

     // path delays
     (A *> Y) = (tpllh$A$Y, tplhl$A$Y);
     (B *> Y) = (tpllh$B$Y, tplhl$B$Y);

   endspecify

endmodule
`endcelldefine

`timescale 1ns/10ps
`celldefine
module XOR2X1 (A, B, Y);
input  A ;
input  B ;
output Y ;

   xor (Y, A, B);

   specify
     // delay parameters
     specparam
       tpllh$B$Y = 0.14:0.14:0.14,
       tplhl$B$Y = 0.15:0.15:0.15,
       tpllh$A$Y = 0.12:0.12:0.12,
       tplhl$A$Y = 0.12:0.12:0.12;

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


module PADNC();
endmodule

module PADFC();
endmodule

module PADGND();
endmodule

module PADVDD();
endmodule

