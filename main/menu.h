#pragma once

// First visible row for a scrolling list that shows `visible` of `count` rows
// with the cursor at `idx`: keeps the cursor on-screen, centred where possible,
// clamped so the window never runs past either end. Pure → host-tested
// (test/host/test_games.c).
static inline int menu_window_start(int idx, int count, int visible)
{
    if (count <= visible) return 0;
    int start = idx - visible / 2;
    if (start < 0) start = 0;
    if (start > count - visible) start = count - visible;
    return start;
}
