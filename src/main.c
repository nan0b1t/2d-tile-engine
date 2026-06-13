#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include "nums.h"
#include "stdlib.h"

#define BLOCK_SIZE 16

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
    int (*init)(struct Game *game);
    int (*update)(struct Game *game);
    int (*end)(struct Game *game);
} Game;

int runGame(Game game) {
    int result = game.init(&game);
    if (result) return result;

    while (game.update(&game)) {}

    return game.end(&game);
}

void setColFromBottom(World* world, int height, int x, u32 block) {
    for (int i = world->height - 1; i >= world->height - height - 1; i --) {
        world->blocks[i * world->width + x] = block;
    }
}

int initGame(Game *game) {
    game->world = getWorld(50, 50);

    for (int i = 0; i < game->world.width; i++) {
        int height = noise(i * 0.01, 67);
        setColFromBottom(&game->world,  game->world.height * (height + 0.5), i, 1);
    }

    InitWindow(800, 600, "random block game idk bro");
    return 0;
}

int updateGame(Game *game) {
    if (WindowShouldClose()) {
        return 0;
    }
    BeginDrawing();
    ClearBackground(RAYWHITE);
    for (int i = 0; i < game->world.width * game->world.height; i++) {
        switch (game->world.blocks[i]) {
            case 1:
                DrawRectangle(i % game->world.width * BLOCK_SIZE,
                              i / game->world.width * BLOCK_SIZE,
                              BLOCK_SIZE,
                              BLOCK_SIZE,
                              (Color){150, 75, 0, 255}
                );
                break;
        }
    }
    EndDrawing();
    return 1;
}

int endGame(Game *game) {
    endWorld(&game->world);
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

