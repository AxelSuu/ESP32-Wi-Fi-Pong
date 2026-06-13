#pragma once

// Tiny transient visual-effects layer for arcade "juice": screen-shake, a brief
// full-screen flash, and particle sparks. Allocation-free fixed pools. Triggered
// by the engine and games; rendered each frame between the world draw and the
// present. Drawing goes through gfx_*, so shake rides the global gfx origin.

void fx_reset(void);                       // clear all effects
void fx_flash(void);                       // brief full-screen white flash
void fx_shake(int amplitude, int frames);  // jitter the whole frame for N frames
void fx_spark(int x, int y);               // small particle burst at (x, y)

void fx_update(void);   // advance effects + apply shake origin; call once/frame BEFORE drawing
void fx_draw(void);     // draw sparks + flash overlay; call AFTER drawing, before present
