
#ifndef ASTAR_H
#define ASTAR_H

enum CELL_TYPE{CELL_EMPTY=0,CELL_FULL};

struct grid_coord
{
    int x;
    int y;
};

struct grid
{
    struct grid_coord **cells;
    enum CELL_TYPE **colision;
    int **cost;
    int width;
    int height;
};

struct grid_coord  *astar(struct grid *grid,struct grid_coord start,struct grid_coord goal);
int manhattan_dist(struct grid_coord cell_a ,struct grid_coord cell_b);

#endif
