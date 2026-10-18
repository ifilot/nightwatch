/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "MINE.H"
static int adjacent(int a, int b)
{
    int x = a%9 - b%9, y = a/9 - b/9;
    return a != b && x >= -1 && x <= 1 && y >= -1 && y <= 1;
}
static void check_field(const Mine *game, int first)
{
    int i, j, count, bombs = 0, opened = 0;
    for (i = 0; i < 81; ++i) {
        if (game->cells[i] & MINE_BOMB) {
            ++bombs;
            assert(i != first && !adjacent(i, first));
            assert(!(game->cells[i] & MINE_OPEN));
        } else {
            count = 0;
            for (j = 0; j < 81; ++j)
                if (adjacent(i, j) && (game->cells[j] & MINE_BOMB)) ++count;
            assert((game->cells[i] & 15) == count);
            if (game->cells[i] & MINE_OPEN) ++opened;
        }
    }
    assert(bombs == 10 && opened == game->opened);
}
int main(void)
{
    Mine game, before, other;
    int first, i, j, number = -1, old_opened;
    unsigned seed;
    for (seed = 0; seed < 24; ++seed) for (first = 0; first < 81; ++first) {
        mine_new(&game, seed);
        game.cursor = first;
        mine_reveal(&game);
        assert(game.state == MINE_PLAY || game.state == MINE_WON);
        assert((game.cells[first] & 15) == 0 && game.opened >= 4);
        assert(game.turns == 1);
        check_field(&game, first);
        /* Revealing every safe square must win, independent of visit order. */
        for (i = 0; i < 81; ++i) if (!(game.cells[i] & MINE_BOMB)) {
            game.cursor = i;
            mine_reveal(&game);
        }
        assert(game.state == MINE_WON && game.opened == 71 && game.flags == 10);
        before = game;
        mine_reveal(&game); mine_flag(&game);
        assert(!memcmp(&game, &before, sizeof(game)));
    }
    mine_new(&game, 123);
    mine_new(&other, 123);
    mine_reveal(&game); mine_reveal(&other);
    assert(!memcmp(&game, &other, sizeof(game)));
    /* Edge movement clamps without wrapping; unrecognized/no-op actions can
     * leave the entire view untouched. */
    game.cursor = 0;
    mine_move(&game, -1, 0); mine_move(&game, 0, -1);
    assert(game.cursor == 0);
    game.cursor = 80;
    mine_move(&game, 1, 0); mine_move(&game, 0, 1);
    assert(game.cursor == 80);
    mine_new(&game, 7);
    mine_flag(&game); before = game;
    mine_reveal(&game);
    assert(!memcmp(&game, &before, sizeof(game)) && game.state == MINE_READY);
    mine_flag(&game);
    assert(game.flags == 0);
    for (i = 0; i < 11; ++i) { game.cursor = i; mine_flag(&game); }
    assert(game.flags == 10 && !(game.cells[10] & MINE_FLAG));
    mine_new(&game, 9); mine_reveal(&game);
    for (i = 0; i < 81; ++i) if (game.cells[i] & MINE_BOMB) break;
    game.cursor = i; mine_reveal(&game);
    assert(game.state == MINE_LOST && game.hit == i);
    before = game; mine_flag(&game); mine_reveal(&game);
    assert(!memcmp(&game, &before, sizeof(game)));
    /* Find a revealed numbered boundary with something left to chord. */
    mine_new(&game, 123); mine_reveal(&game);
    for (i = 0; i < 81; ++i) {
        if (!(game.cells[i] & MINE_OPEN) || !(game.cells[i] & 15)) continue;
        for (j = 0; j < 81; ++j)
            if (adjacent(i, j) && !(game.cells[j] & (MINE_BOMB|MINE_OPEN))) number = i;
        if (number >= 0) break;
    }
    assert(number >= 0);
    game.cursor = number; before = game; mine_reveal(&game);
    assert(!memcmp(&game, &before, sizeof(game)));
    for (i = 0; i < 81; ++i) if (adjacent(number, i) && (game.cells[i] & MINE_BOMB)) {
        game.cursor = i; mine_flag(&game);
    }
    old_opened = game.opened; game.cursor = number; mine_reveal(&game);
    assert(game.state != MINE_LOST && game.opened > old_opened);
    /* A matching count of incorrect flags must not hide a mine on a chord. */
    game = before;
    for (i = 0; i < 81; ++i) if (adjacent(number, i) && (game.cells[i] & MINE_BOMB)) {
        game.cursor = i; mine_flag(&game);
    }
    for (i = 0; i < 81; ++i) if (adjacent(number, i) && (game.cells[i] & MINE_FLAG)) break;
    game.cursor = i; mine_flag(&game);
    for (j = 0; j < 81; ++j)
        if (adjacent(number, j) && !(game.cells[j] & (MINE_BOMB|MINE_OPEN))) break;
    assert(j < 81); game.cursor = j; mine_flag(&game);
    game.cursor = number; mine_reveal(&game);
    assert(game.state == MINE_LOST && game.hit == i);
    puts("PASS: Minefield safe starts, counts, floods, flags, bounds, win/loss, chords and deterministic seeds");
    return 0;
}
