/* SPDX-License-Identifier: GPL-3.0-only */
#include <string.h>
#include "MINE.H"
static unsigned random_next(Mine *game)
{
    /* Explicitly bounded to 16 bits on both the host and 8086. */
    unsigned long value = game->seed;
    value ^= (value << 7) & 65535UL;
    value ^= value >> 9;
    value ^= (value << 8) & 65535UL;
    game->seed = (unsigned)(value & 65535UL);
    return game->seed;
}
static int neighbor(int cell, int other)
{
    int dx = cell % 9 - other % 9, dy = cell / 9 - other / 9;
    return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1 && cell != other;
}
void mine_new(Mine *game, unsigned seed)
{
    memset(game, 0, sizeof(*game));
    game->seed = seed ? seed : 0x4e57;
    game->cursor = 40;
    game->hit = -1;
}
void mine_move(Mine *game, int dx, int dy)
{
    int x = game->cursor % 9 + dx, y = game->cursor / 9 + dy;
    if (x >= 0 && x < 9 && y >= 0 && y < 9) game->cursor = y * 9 + x;
}
void mine_flag(Mine *game)
{
    unsigned char *cell = game->cells + game->cursor;
    if (game->state >= MINE_WON || (*cell & MINE_OPEN)) return;
    if (*cell & MINE_FLAG) { *cell &= ~MINE_FLAG; --game->flags; }
    else if (game->flags < MINE_COUNT) { *cell |= MINE_FLAG; ++game->flags; }
}
static void plant(Mine *game)
{
    int placed = 0, at, i, j;
    while (placed < MINE_COUNT) {
        at = random_next(game) % MINE_CELLS;
        if (at == game->cursor || neighbor(at, game->cursor) ||
            (game->cells[at] & MINE_BOMB)) continue;
        game->cells[at] |= MINE_BOMB;
        ++placed;
    }
    for (i = 0; i < MINE_CELLS; ++i) {
        if (game->cells[i] & MINE_BOMB) continue;
        for (j = 0; j < MINE_CELLS; ++j)
            if ((game->cells[j] & MINE_BOMB) && neighbor(i, j)) ++game->cells[i];
    }
    game->state = MINE_PLAY;
}
static void open_cell(Mine *game, int at)
{
    unsigned char *cell = game->cells + at;
    if (*cell & (MINE_OPEN | MINE_FLAG)) return;
    if (*cell & MINE_BOMB) { game->state = MINE_LOST; game->hit = at; return; }
    *cell |= MINE_OPEN;
    ++game->opened;
}
void mine_reveal(Mine *game)
{
    int i, j, flags = 0, before, expanded;
    unsigned char cell = game->cells[game->cursor];
    if (game->state >= MINE_WON || (cell & MINE_FLAG)) return;
    if (game->state == MINE_READY) plant(game);
    before = game->opened;
    if (cell & MINE_OPEN) {
        for (i = 0; i < MINE_CELLS; ++i)
            if (neighbor(game->cursor, i) && (game->cells[i] & MINE_FLAG)) ++flags;
        if (flags != (cell & 15)) return;
        for (i = 0; i < MINE_CELLS && game->state != MINE_LOST; ++i)
            if (neighbor(game->cursor, i)) open_cell(game, i);
    } else open_cell(game, game->cursor);
    /* Iterative flood expansion avoids recursion and a separate queue. */
    do {
        expanded = game->opened;
        for (i = 0; i < MINE_CELLS && game->state != MINE_LOST; ++i) {
            if (game->cells[i] != MINE_OPEN) continue;
            for (j = 0; j < MINE_CELLS; ++j)
                if (neighbor(i, j)) open_cell(game, j);
        }
    } while (expanded != game->opened && game->state != MINE_LOST);
    if (game->opened != before || game->state == MINE_LOST) ++game->turns;
    if (game->state != MINE_LOST && game->opened == MINE_CELLS - MINE_COUNT) {
        game->state = MINE_WON;
        game->flags = MINE_COUNT;
        for (i = 0; i < MINE_CELLS; ++i)
            if (game->cells[i] & MINE_BOMB) game->cells[i] |= MINE_FLAG;
    }
}
