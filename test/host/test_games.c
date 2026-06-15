// Host-side unit tests for the game logic. Each game is pure C behind the
// game_module_t vtable with one dependency (gfx_*), which is stubbed — so the
// rules can be exercised on a dev machine with no ESP32. See README "Testing".
//
// Build & run:  make -C test/host run
#include <stdio.h>
#include "game_module.h"
#include "menu.h"
#include "mock.h"

extern const game_module_t PONG;
extern const game_module_t SURVIVOR;
extern const game_module_t DESCENDER;
extern const game_module_t PUZZLER;
extern const game_module_t RUNNER;

static int g_fail;
#define CHECK(cond, msg)                                                                       \
    do {                                                                                       \
        if (cond) {                                                                            \
            printf("  ok   - %s\n", (msg));                                                    \
        } else {                                                                               \
            printf("  FAIL - %s\n", (msg));                                                    \
            g_fail++;                                                                          \
        }                                                                                      \
    } while (0)

static void send(const game_module_t *g, input_kind_t kind, int player)
{
    input_event_t ev = {.kind = kind, .player = player, .analog = 0};
    g->on_input(&ev);
}

// --- Pong (drives to completion; AI vs a stationary player) ---

static void test_pong_reaches_a_valid_winner(void)
{
    printf("pong: a game terminates with a valid winner\n");
    mock_random_reset();
    mock_clock_reset();
    PONG.reset();
    const int LIMIT = 1000000;
    int       ticks = 0;
    while (!PONG.is_over() && ticks < LIMIT) {
        PONG.tick(30);
        mock_clock_advance_us(30000); // mirror the 30 ms fixed step
        ticks++;
    }
    CHECK(PONG.is_over(), "pong ends within the tick budget");
    int w = PONG.winner();
    CHECK(w == 0 || w == 1, "winner is a valid player id");
}

// --- Menu scroll window (pure helper from menu.h) ---

static void test_menu_window(void)
{
    printf("menu: scroll window keeps the cursor visible\n");
    CHECK(menu_window_start(2, 3, 5) == 0, "no scroll when everything fits");
    CHECK(menu_window_start(0, 6, 5) == 0, "first row → window at top");
    CHECK(menu_window_start(5, 6, 5) == 1, "last row → window scrolled to show it");
    int ok = 1;
    for (int idx = 0; idx < 6; idx++) {
        int s = menu_window_start(idx, 6, 5);
        if (idx < s || idx >= s + 5) ok = 0;          // cursor must be inside the window
        if (s < 0 || s > 6 - 5) ok = 0;               // window must stay in range
    }
    CHECK(ok, "cursor stays inside an in-range window for every index");
}

// --- Survivor (single-stick roguelite; RNG is mocked) ---

static void test_survivor_reset(void)
{
    printf("survivor: fresh run starts clean\n");
    mock_random_reset();
    SURVIVOR.reset();
    CHECK(!SURVIVOR.is_over(), "not over at start");
    CHECK(SURVIVOR.score() == 0, "score starts at 0");
    CHECK(SURVIVOR.winner() == -1, "single-player: no winner");
}

static void test_survivor_survival_scores(void)
{
    printf("survivor: surviving accrues score over time\n");
    mock_random_reset();
    SURVIVOR.reset();
    for (int i = 0; i < 100; i++) {       // ~3 s; clear any level-up so the sim keeps running
        send(&SURVIVOR, INPUT_SELECT, 0);
        SURVIVOR.tick(30);
    }
    CHECK(SURVIVOR.score() >= 3, "score reflects ~3 s of survival");
    CHECK(SURVIVOR.winner() == -1, "still no winner");
}

// --- Descender (Downwell-like faller; RNG mocked, deterministic LCG fallback) ---

static void test_descender_reset(void)
{
    printf("descender: fresh run starts clean\n");
    mock_random_reset();
    DESCENDER.reset();
    CHECK(!DESCENDER.is_over(), "not over at start");
    CHECK(DESCENDER.score() == 0, "score starts at 0");
    CHECK(DESCENDER.winner() == -1, "single-player: no winner");
}

static void test_descender_descent_scores(void)
{
    printf("descender: falling accrues depth score\n");
    mock_random_reset();
    DESCENDER.reset();
    for (int i = 0; i < 30; i++) DESCENDER.tick(30);   // ~0.9 s of falling, no input
    CHECK(DESCENDER.score() >= 1, "depth contributes score while falling");
    CHECK(!DESCENDER.is_over(), "still alive after a short fall");
}

static void test_descender_attrition_ends(void)
{
    printf("descender: passive play eventually ends the run\n");
    mock_random_reset();
    DESCENDER.reset();
    int ticks = 0;
    while (!DESCENDER.is_over() && ticks < 20000) {
        send(&DESCENDER, INPUT_SELECT, 0);   // clear any upgrade pick so the sim keeps running
        DESCENDER.tick(30);
        ticks++;
    }
    CHECK(DESCENDER.is_over(), "run ends within the tick budget (spiky enemies damage a passive player)");
    CHECK(DESCENDER.winner() == -1, "single-player: no winner");
}

// --- Puzzler (Tetris-like; 7-bag RNG mocked, deterministic LCG fallback) ---

static void test_puzzler_reset(void)
{
    printf("puzzler: fresh game starts clean\n");
    mock_random_reset();
    PUZZLER.reset();
    CHECK(!PUZZLER.is_over(), "not over at start");
    CHECK(PUZZLER.score() == 0, "score starts at 0");
    CHECK(PUZZLER.winner() == -1, "single-player: no winner");
}

static void test_puzzler_soft_drop_scores(void)
{
    printf("puzzler: soft drop adds score per cell\n");
    mock_random_reset();
    PUZZLER.reset();
    for (int i = 0; i < 5; i++) send(&PUZZLER, INPUT_DOWN, 0);
    CHECK(PUZZLER.score() >= 5, "five soft-drop steps scored");
    CHECK(!PUZZLER.is_over(), "still going");
}

static void test_puzzler_tops_out(void)
{
    printf("puzzler: stacking to the top ends the game\n");
    mock_random_reset();
    PUZZLER.reset();
    int n = 0;
    while (!PUZZLER.is_over() && n < 5000) {
        send(&PUZZLER, INPUT_PRIMARY, 0);   // hard drop, no horizontal moves -> columns 2-5 stack up
        PUZZLER.tick(30);
        n++;
    }
    CHECK(PUZZLER.is_over(), "game tops out within budget");
    CHECK(PUZZLER.winner() == -1, "single-player: no winner");
}

// --- Runner (top-down endless dodger; RNG mocked, deterministic LCG fallback) ---

static void test_runner_reset(void)
{
    printf("runner: fresh run starts clean\n");
    mock_random_reset();
    RUNNER.reset();
    CHECK(!RUNNER.is_over(), "not over at start");
    CHECK(RUNNER.score() == 0, "score starts at 0");
    CHECK(RUNNER.winner() == -1, "single-player: no winner");
}

static void test_runner_distance_scores(void)
{
    printf("runner: travelling accrues distance score\n");
    mock_random_reset();
    RUNNER.reset();
    for (int i = 0; i < 20; i++) RUNNER.tick(30);   // before the first obstacle can reach the car
    CHECK(RUNNER.score() >= 1, "distance contributes score");
    CHECK(!RUNNER.is_over(), "still alive after a short run");
}

static void test_runner_crash_ends(void)
{
    printf("runner: a stationary car eventually crashes\n");
    mock_random_reset();
    RUNNER.reset();
    int ticks = 0;
    while (!RUNNER.is_over() && ticks < 20000) {
        send(&RUNNER, INPUT_SELECT, 0);   // clear any perk pick so the sim keeps running
        RUNNER.tick(30);
        ticks++;
    }
    CHECK(RUNNER.is_over(), "run ends within the tick budget (an aligned rock with no shield)");
    CHECK(RUNNER.winner() == -1, "single-player: no winner");
}

int main(void)
{
    test_menu_window();
    test_survivor_reset();
    test_survivor_survival_scores();
    test_descender_reset();
    test_descender_descent_scores();
    test_descender_attrition_ends();
    test_puzzler_reset();
    test_puzzler_soft_drop_scores();
    test_puzzler_tops_out();
    test_runner_reset();
    test_runner_distance_scores();
    test_runner_crash_ends();
    test_pong_reaches_a_valid_winner();

    printf("\n%s (%d failure%s)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail,
           g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
