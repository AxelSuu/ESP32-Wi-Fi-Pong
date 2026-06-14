// Host-side unit tests for the game logic. Each game is pure C behind the
// game_module_t vtable with one dependency (gfx_*), which is stubbed — so the
// rules can be exercised on a dev machine with no ESP32. See README "Testing".
//
// Build & run:  make -C test/host run
#include <stdio.h>
#include "game_module.h"
#include "menu.h"
#include "mock.h"

extern const game_module_t SNAKE;
extern const game_module_t TRON;
extern const game_module_t PONG;
extern const game_module_t BREAKOUT;
extern const game_module_t SURVIVOR;

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

// --- Snake (STEP_MS = 140 → one tick(140) == one simulation step) ---

static void test_snake_reversal_rejected(void)
{
    printf("snake: a 180-degree reversal is rejected\n");
    mock_random_reset();
    mock_random_push(0);
    mock_random_push(0); // food at (0,0), out of the path
    SNAKE.reset();
    send(&SNAKE, INPUT_LEFT, 0); // reverse straight into the neck
    SNAKE.tick(140);
    CHECK(!SNAKE.is_over(), "snake keeps heading right instead of dying on its neck");
    CHECK(SNAKE.score() == 0, "rejected reversal causes no growth");
}

static void test_snake_eats_food(void)
{
    printf("snake: eating food grows the snake and scores\n");
    mock_random_reset();
    mock_random_push(11);
    mock_random_push(8); // food one cell ahead of the head
    SNAKE.reset();
    SNAKE.tick(140);
    CHECK(SNAKE.score() == 1, "score increments after eating");
    CHECK(!SNAKE.is_over(), "eating does not end the round");
}

static void test_snake_wall_death(void)
{
    printf("snake: running off the edge ends the round\n");
    mock_random_reset();
    mock_random_push(0);
    mock_random_push(0);
    SNAKE.reset();
    for (int i = 0; i < 11; i++) SNAKE.tick(140); // x: 10 -> 21 (past the right wall)
    CHECK(SNAKE.is_over(), "game over after hitting the wall");
    CHECK(SNAKE.winner() == -1, "single-player snake reports no winner");
}

// --- Tron (STEP_MS = 90, no RNG/clock dependence) ---

static void test_tron_head_on_double_kill(void)
{
    printf("tron: a head-on into the same cell kills both (draw)\n");
    TRON.reset();                              // p0 and p1 start facing each other
    for (int i = 0; i < 8; i++) TRON.tick(90); // close the gap until they collide
    CHECK(TRON.is_over(), "round ends on the head-on");
    CHECK(TRON.winner() == -1, "head-on is a draw (winner -1)");
}

static void test_tron_crash_hands_win_to_survivor(void)
{
    printf("tron: crashing into the wall hands the other player the win\n");
    TRON.reset();
    send(&TRON, INPUT_LEFT, 0);                 // p0 turns up, toward the top edge
    for (int i = 0; i < 13; i++) TRON.tick(90); // p0 runs off the top; p1 survives
    CHECK(TRON.is_over(), "round ends when p0 crashes");
    CHECK(TRON.winner() == 1, "surviving player 1 wins");
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

// --- Breakout (one tick == one ball step; ball starts up-right from the paddle) ---

static void test_breakout_breaks_a_brick(void)
{
    printf("breakout: the ball clears a brick and scores\n");
    BREAKOUT.reset();
    for (int i = 0; i < 30; i++) BREAKOUT.tick(30);   // ball climbs into the brick rows
    CHECK(BREAKOUT.score() >= 1, "score increased after hitting a brick");
    CHECK(!BREAKOUT.is_over(), "breaking a brick does not end the game");
}

static void test_breakout_game_over_loses_lives(void)
{
    printf("breakout: missing the ball eventually ends the game\n");
    BREAKOUT.reset();                                  // paddle never moves -> lives run out
    int ticks = 0;
    while (!BREAKOUT.is_over() && ticks < 20000) {
        BREAKOUT.tick(30);
        ticks++;
    }
    CHECK(BREAKOUT.is_over(), "game ends within the tick budget");
    CHECK(BREAKOUT.winner() == -1, "single-player breakout reports no winner");
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

int main(void)
{
    test_menu_window();
    test_survivor_reset();
    test_survivor_survival_scores();
    test_snake_reversal_rejected();
    test_snake_eats_food();
    test_snake_wall_death();
    test_tron_head_on_double_kill();
    test_tron_crash_hands_win_to_survivor();
    test_pong_reaches_a_valid_winner();
    test_breakout_breaks_a_brick();
    test_breakout_game_over_loses_lives();

    printf("\n%s (%d failure%s)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail,
           g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
