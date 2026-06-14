#define Camera stupid_garbage_lol
#include "raylib.h"
#undef Camera
#include <stdio.h>
#include <math.h>
#include "nums.h"
#include "stdlib.h"

#define BASE_HEIGHT 200
#define BLOCK_SIZE 4
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

typedef struct {
    int x;
    int y;
} Camera;

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
    result.blocks = calloc(width * height, sizeof(u32));

    printf("allocated %p (%zu bytes) for world data.\n",
           (void *)result.blocks,
           sizeof(u32) * (size_t)width * height);

    return result;
}

void endWorld(World *world) {
    free(world->blocks);
}

typedef struct Game {
    World world;
    Camera camera;
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

void setColFromBottom(World *world, int height, int x, u32 block) {
    height = CLAMP(height, 0, (int)world->height);

    int start = (int)world->height - 1;
    int end   = (int)world->height - height;

    for (int y = start; y >= end; --y) {
        world->blocks[y * world->width + x] = block;
    }
}

int initGame(Game *game) {
    game->world = getWorld(1600, BASE_HEIGHT + 50);
    game->camera.x = 0;
    game->camera.y = 0;

    double amp = 0.015;

    for (int i = 0; i < game->world.width; i++) {
        double height = noise(i * amp, 47);
        setColFromBottom(&game->world,  BASE_HEIGHT + (game->world.height - BASE_HEIGHT ) * (height + 0.5), i, 1);
    }

    InitWindow(800, 600, "random block game idk bro");
    return 0;
}

int updateGame(Game *game) {
    if (WindowShouldClose()) {
        return 0;
    }

    if (IsKeyDown(KEY_A)) game->camera.x -= 5;
    if (IsKeyDown(KEY_D)) game->camera.x += 5;
    if (IsKeyDown(KEY_W)) game->camera.y -= 5;
    if (IsKeyDown(KEY_S)) game->camera.y += 5;

    BeginDrawing();
    ClearBackground(RAYWHITE);
    for (int i = 0; i < game->world.width * game->world.height; i++) {
        int projX = ((i % game->world.width) * BLOCK_SIZE) + game->camera.x;
        int projY = ((i / game->world.width) * BLOCK_SIZE) + game->camera.y;

        if ((projX < 0 - BLOCK_SIZE || projX > GetScreenWidth()) || (projY < 0 - BLOCK_SIZE || projY > GetScreenHeight())) {
            continue;
        }

        switch (game->world.blocks[i]) {
            case 1:
                DrawRectangle(i % game->world.width * BLOCK_SIZE + game->camera.x,
                              i / game->world.width * BLOCK_SIZE + game->camera.y,
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

