/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MINE_H
#define MINE_H
#define MINE_SIDE 9
#define MINE_CELLS 81
#define MINE_COUNT 10
#define MINE_BOMB 16
#define MINE_OPEN 32
#define MINE_FLAG 64
#define MINE_READY 0
#define MINE_PLAY 1
#define MINE_WON 2
#define MINE_LOST 3
/* A whole game fits on the modal's stack; no heap or filesystem access. */
typedef struct {
    unsigned char cells[MINE_CELLS];
    unsigned seed;
    int cursor, state, flags, opened, turns, hit;
} Mine;
void mine_new(Mine *game, unsigned seed);
void mine_move(Mine *game, int dx, int dy);
void mine_flag(Mine *game);
/* Opening a revealed number chords its neighbors when flags match. */
void mine_reveal(Mine *game);
#endif
