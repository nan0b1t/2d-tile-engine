#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include "nums.h"
#include "stdlib.h"


double magicHash(i32 x, i32 seed) {
    uint32_t state = (i32)x + ((i32)seed * 0x9E3779B9U);

    state ^= state >> 16;
    state *= 0x7FEB352DU;
    state ^= state >> 15;
    state *= 0x846CA68BU;
    state ^= state >> 16;

    return (state & 1) == 0 ? 1.0 : -1.0;
}

double magicFade(double u) {
    return u * u * u * (u * (u * 6.0 - 15.0) + 10.0);
}

double noise(double i, i32 seed) {
    double lower = floor(i);
    double higher = lower + 1.0; 

    double lowerVal = magicHash(lower, seed);
    double higherVal = magicHash(higher, seed);

    double lowerDist = i - lower;
    double higherDist = i - higher; 

    double lowerPull = lowerDist * lowerVal;
    double higherPull = higherDist * higherVal;

    return lowerPull + magicFade(lowerDist) * (higherPull - lowerPull);
}

typedef struct {
    u32* blocks;
    u32 width;
    u32 height;
} World;

World getWorld(u32 width, u32 height) {
    World result;
    result.width = width;
    result.height = height;
    result.blocks = malloc(sizeof(u32) * width * height);
    return result;
}

void endWorld(World *world) {
    free(world->blocks);
}

typedef struct Game {
    World world;
    int (*init)(struct Game game);
    int (*update)(struct Game game);
    int (*end)(struct Game game);
} Game;

int runGame(Game game) {
    int result = game.init(game);
    if (result) return result;

    while (game.update(game)) {}

    return game.end(game);
}

int initGame() {
    int32_t world_seed = 67;
    double step_size = 0.1; // Smaller number = smoother, wider hills
    int total_steps = 100;

    printf("Testing Seeded 1D Perlin Noise (Seed: %d):\n", world_seed);
    printf("--------------------------------------------------\n");

    for (int step = 0; step < total_steps; step++) {
        double x = step * step_size;
        double val = noise(x, world_seed);

        // Convert the noise value (-1.0 to 1.0) into spaces for a text graph
        // This maps the value cleanly to a width of 0 to 40 characters
        int graph_width = (int)((val + 1.0) * 20.0);
        
        // Print the coordinate, raw value, and the visual wave
        printf("X: %4.2f (%+6.3f) | ", x, val);
        for (int s = 0; s < graph_width; s++) {
            printf(" ");
        }
        printf("*\n"); // The point on our curve
    }

    InitWindow(800, 600, "random block game idk bro");
    return 0;
}

int updateGame() {
    if (WindowShouldClose()) {
        return 0;
    }
    BeginDrawing();
    ClearBackground(RAYWHITE);
    EndDrawing();
    return 1;
}

int endGame() {
    CloseWindow();
    return 0;
}
int main() {
    Game game;
    game.init = initGame;
    game.update = updateGame;
    game.end = endGame;
    runGame(game);
    return 0;
}

