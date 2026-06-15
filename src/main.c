#define Camera stupid_garbage_lol
#include "raylib.h"
#undef Camera
#include "nums.h"
#include "stdlib.h"
#include <math.h>
#include <stdio.h>

#define BASE_HEIGHT 200
#define BLOCK_SIZE 4
#define CLAMP(val, min, max)                                                   \
    ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

#define CHUNK_SIZE 32
#define TABLE_SIZE 2048

#if defined(_MSC_VER)
/* MSVC Style */
#define PACK_START __pragma(pack(push, 1))
#define PACK_END __pragma(pack(pop))
#define PACK_ATTR
#elif defined(__GNUC__) || defined(__clang__)
/* GCC and Clang Style */
#define PACK_START
#define PACK_END
#define PACK_ATTR __attribute__((packed))
#endif

enum { SLOT_EMPTY = 0, SLOT_FULL = 1, TOMBSTONE = 2 };

typedef struct Point {
    i32 x; i32 y;
} Point;

typedef struct Chunk {
    int blocks[CHUNK_SIZE * CHUNK_SIZE];
} Chunk;

PACK_START typedef struct TableMeta {
    i32 x, y;
    u8 state;
} PACK_ATTR TableMeta;

PACK_END typedef struct ChunkMap {
    TableMeta meta[TABLE_SIZE];
    Chunk chunks[TABLE_SIZE];
} ChunkMap;

u32 getTableIndex(i32 x, i32 y) {
    u64 h1 = (u64)(u32)x;
    u64 h2 = (u64)(u32)y;
    h1 *= 0x9e3779b97f4a7c15ULL;
    h2 *= 0xbf58476d1ce4e5b9ULL;
    u64 combined = h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    combined ^= combined >> 33;
    combined *= 0xff51afd7ed558ccdULL;
    combined ^= combined >> 33;
    combined *= 0xc4ceb9fe1a85ec53ULL;
    combined ^= combined >> 33;
#if (TABLE_SIZE & (TABLE_SIZE - 1)) == 0
    return (u32)(combined & (TABLE_SIZE - 1));
#else
    return (u32)(combined % TABLE_SIZE);
#endif
}

const Chunk *getChunk(ChunkMap *chunkMap, i32 x, i32 y) {
    u32 hash = getTableIndex(x, y);
    u32 startHash = hash;
    while (chunkMap->meta[hash].state != SLOT_EMPTY) {
        if (chunkMap->meta[hash].state == SLOT_FULL &&
            chunkMap->meta[hash].x == x && chunkMap->meta[hash].y == y) {
            return &chunkMap->chunks[hash];
        }
        hash = (hash + 1) & (TABLE_SIZE - 1);
        if (hash == startHash) {
            break;
        }
    }
    return NULL;
}

Chunk *getChunkMut(ChunkMap *chunkMap, i32 x, i32 y) {
    u32 hash = getTableIndex(x, y);
    u32 startHash = hash;
    while (chunkMap->meta[hash].state != SLOT_EMPTY) {
        if (chunkMap->meta[hash].state == SLOT_FULL &&
            chunkMap->meta[hash].x == x && chunkMap->meta[hash].y == y) {
            return &chunkMap->chunks[hash];
        }
        hash = (hash + 1) & (TABLE_SIZE - 1);
        if (hash == startHash) {
            break;
        }
    }
    return NULL;
}

void setChunk(ChunkMap *chunkMap, i32 x, i32 y, Chunk *chunk) {
    u32 hash = getTableIndex(x, y);
    u32 startHash = hash;
    i32 targChunk = -1;

    while (1) {
        if (chunkMap->meta[hash].state == SLOT_FULL &&
            chunkMap->meta[hash].x == x && chunkMap->meta[hash].y == y
        ) {
            chunkMap->chunks[hash] = *chunk;
            return;
        }
        if ((chunkMap->meta[hash].state == TOMBSTONE ||
             chunkMap->meta[hash].state == SLOT_EMPTY) &&
            targChunk == -1) {
            targChunk = hash;
        }
        if (chunkMap->meta[hash].state == SLOT_EMPTY) {
            break;
        }
        hash = (hash + 1) & (TABLE_SIZE - 1);
        if (hash == startHash) {
            break;
        }
    }
    if (targChunk == -1) {
        return;
    }
    chunkMap->chunks[targChunk] = *chunk;
    chunkMap->meta[targChunk].x = x;
    chunkMap->meta[targChunk].y = y;
    chunkMap->meta[targChunk].state = SLOT_FULL;
}

void removeChunk(ChunkMap *chunkMap, i32 x, i32 y) {
    unsigned int index = getTableIndex(x, y);
    unsigned int start_index = index;
    while (chunkMap->meta[index].state != SLOT_EMPTY) {
        if (chunkMap->meta[index].state == SLOT_FULL &&
            chunkMap->meta[index].x == x && chunkMap->meta[index].y == y) {
            chunkMap->meta[index].state = TOMBSTONE;
            return;
        }
        index = (index + 1) % TABLE_SIZE;
        if (index == start_index)
            break;
    }
}

typedef struct {
    int x;
    int y;
} Camera;

Point localToWorld(i32 x, i32 y, Camera camera) {
    return (Point) {
        .x = x + camera.x,
        .y = y + camera.y
    };
}

Point localToBlock(i32 x, i32 y, Camera camera) {
    x = x + camera.x % BLOCK_SIZE;
    if (x < 0) x += BLOCK_SIZE;

    y = y + camera.y % BLOCK_SIZE;
    if (y < 0) y += BLOCK_SIZE;

    return (Point) {
        .x = x,
        .y = y
    };
}

inline u32 betterModulo(i32 x, i32 y) {
    x = x % y;
    if (x < 0) x += y;
    return x;
}

Point globalToBlock(i32 x, i32 y) {
    return (Point) {
        .x = betterModulo(x, BLOCK_SIZE),
        .y = betterModulo(x, BLOCK_SIZE) 
    };
}

Point blockToChunk(i32 x, i32 y) {
    return (Point) {
        .x = betterModulo(x, CHUNK_SIZE),
        .y = betterModulo(y, CHUNK_SIZE)
    };
}

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
    u32 *blocks;
    u32 width;
    u32 height;
} World;

ChunkMap *getWorld() {
    ChunkMap *result = calloc(1, sizeof(ChunkMap));
    if (result == NULL) {
        perror("failed to allocate chunkmap\n");
        exit(1);
    }

    return result;
}

void endWorld(World *world) {
    free(world->blocks);
}

typedef struct Game {
    // World world;
    Camera camera;
    ChunkMap *chunkMap;
    int (*init)(struct Game *game);
    int (*update)(struct Game *game);
    int (*end)(struct Game *game);
} Game;

int runGame(Game game) {
    int result = game.init(&game);
    if (result)
        return result;

    while (game.update(&game)) {
    }

    return game.end(&game);
}

void setColFromBottom(World *world, int height, int x, u32 block) {
    height = CLAMP(height, 0, (int)world->height);

    int start = (int)world->height - 1;
    int end = (int)world->height - height;

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
        setColFromBottom(&game->world,
                         BASE_HEIGHT + (game->world.height - BASE_HEIGHT) *
                                           (height + 0.5),
                         i, 1);
    }

    InitWindow(800, 600, "random block game idk bro");
    return 0;
}

int updateGame(Game *game) {
    if (WindowShouldClose()) {
        return 0;
    }

    if (IsKeyDown(KEY_A))
        game->camera.x -= 5;
    if (IsKeyDown(KEY_D))
        game->camera.x += 5;
    if (IsKeyDown(KEY_W))
        game->camera.y -= 5;
    if (IsKeyDown(KEY_S))
        game->camera.y += 5;

    BeginDrawing();
    ClearBackground(RAYWHITE);
    for (int i = 0; i < game->world.width * game->world.height; i++) {
        int projX = ((i % game->world.width) * BLOCK_SIZE) + game->camera.x;
        int projY = ((i / game->world.width) * BLOCK_SIZE) + game->camera.y;

        if ((projX < 0 - BLOCK_SIZE || projX > GetScreenWidth()) ||
            (projY < 0 - BLOCK_SIZE || projY > GetScreenHeight())) {
            continue;
        }

        switch (game->world.blocks[i]) {
        case 1:
            DrawRectangle(i % game->world.width * BLOCK_SIZE + game->camera.x,
                          i / game->world.width * BLOCK_SIZE + game->camera.y,
                          BLOCK_SIZE, BLOCK_SIZE, (Color){150, 75, 0, 255});
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
