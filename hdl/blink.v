`timescale 1ns / 1ps
module lights(
    input clk,

    input rx,
    output tx,

    output [23:0] leds
    );

    reg [23:0] blink;
    always @ (posedge clk) blink <= blink + 1;
    assign leds[1] = blink[23];
    assign leds[23:2] = 0;
    assign leds[0] = 0;
    
endmodule
