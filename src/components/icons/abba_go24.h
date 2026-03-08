#pragma once
#include <cstdint>

// ABBA Go Icon 24x24 - Refined v1.3.0
// Grid at x=8, 16 and y=8, 16
static const uint8_t AbbaGo24Icon[] = {
    // y=0-5: Vertical bars
    0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F,
    
    // y=6: Black stone top (x: 15-17)
    // byte 1: bit 0(15). byte 2: bit 7(16), 6(17). -> mask ~bits -> byte 1: 0xFE, byte 2: 0x3F
    0xFF, 0xFE, 0x3F,
    
    // y=7: Black stone mid (x: 14-18)
    // byte 1: bit 1,0. byte 2: bit 7,6,5. -> byte 1: 0xFC, byte 2: 0x1F
    0xFF, 0xFC, 0x1F,
    
    // y=8-9: Horizontal bar intersected by black stone (x: 14-18)
    // Horizontal bars are 0x00. Stone is 0x00.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    
    // y=10: Black stone bottom (x: 15-17)
    0xFF, 0xFE, 0x3F,
    
    // y=11-13: Vertical bars
    0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F,
    
    // y=14: White stone top (x: 7-9)
    // x=7 (byte 0 bit 0), 8(byte 1 bit 7), 9(bit 6)
    // byte 0 bit 0 = 0 (outline), byte 1 bit 7 = 1 (white), bit 6 = 0 (outline)
    // byte 0: 0xFE, byte 1: 0xBF
    0xFE, 0xBF, 0x3F,
    
    // y=15: White stone mid (x: 6-10)
    // x=6,10 are outline (0). x=7,8,9 are white (1).
    // byte 0: bit 1=0, bit 0=1. byte 1: bits 7,6=1, bit 5=0.
    // byte 0: 1111 1101 (0xFD), byte 1: 1101 1111 (0xDF)
    0xFD, 0xDF, 0x3F,
    
    // y=16-17: Horizontal bar intersected by white stone (x: 6-10)
    // Bar is 0x00. White stone is 1.
    // byte 0 bit 1,0 = 01 (outline, white). byte 1 bits 7,6,5 = 110 (white, white, outline)
    // byte 0: (bar 0x00) -> mask bits -> byte 0: 0xFD?, byte 1: 0xDF? 
    // Wait, let's just use the previous logic.
    0xFD, 0xDF, 0x00, 0xFD, 0xDF, 0x00,
    
    // y=18: White stone bottom (x: 7-9)
    0xFE, 0xBF, 0x3F,
    
    // y=19-23: Vertical bars
    0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F, 0xFF, 0x3F, 0x3F
};
