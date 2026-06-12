#include "raylib.h"
#include "nums.h"
#include "stdlib.h"

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

