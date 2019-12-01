`timescale 1ns / 1ps
module lights(
    input clk,

    input rx,
    output tx,

    output led0,
    output led1,
    output led2,
    output led3,
    output led4,
    output led5,
    output led6,
    output led7,
    output led8,
    output led9,
    output led10,
    output led11,
    output led12,
    output led13,
    output led14,
    output led15,
    output led16,
    output led17,
    output led18,
    output led19,
    output led20,
    output led21,
    output led22,
    output led23
    );

    reg [2*12-1:0] leds = 0;
    assign led0 = leds[0];
    assign led1 = leds[1];
    assign led2 = leds[2];
    assign led3 = leds[3];
    assign led4 = leds[4];
    assign led5 = leds[5];
    assign led6 = leds[6];
    assign led7 = leds[7];
    assign led8 = leds[8];
    assign led9 = leds[9];
    assign led10 = leds[10];
    assign led11 = leds[11];
    assign led12 = leds[12];
    assign led13 = leds[13];
    assign led14 = leds[14];
    assign led15 = leds[15];
    assign led16 = leds[16];
    assign led17 = leds[17];
    assign led18 = leds[18];
    assign led19 = leds[19];
    assign led20 = leds[20];
    assign led21 = leds[21];
    assign led22 = leds[22];
    assign led23 = leds[23];

endmodule