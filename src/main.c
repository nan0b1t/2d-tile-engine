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
#define TABLE_SIZE 16

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

static inline u32 betterModulo(i32 x, i32 y) {
    x = x % y;
    if (x < 0) x += y;
    return x;
}

enum { SLOT_EMPTY = 0, SLOT_FULL = 1, TOMBSTONE = 2 };

typedef struct Point {
    i32 x; i32 y;
} Point;

typedef struct BlockBits {
    u32 foreground : 12;
    u32 background : 10;
    u32 light      :  5;
    u32 data       :  5;
} BlockBits;

typedef union Tile {
    BlockBits bits;
    u32       data;
} Tile;

typedef struct Chunk {
    Tile blocks[CHUNK_SIZE * CHUNK_SIZE];
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

Chunk *touchChunk(ChunkMap *chunkMap, i32 x, i32 y) {
    u32 hash = getTableIndex(x, y);
    u32 startHash = hash;
    i32 targChunk = -1;

    while (1) {
        if (chunkMap->meta[hash].state == SLOT_FULL &&
            chunkMap->meta[hash].x == x && chunkMap->meta[hash].y == y
        ) {
            return &chunkMap->chunks[hash];
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
        return NULL;
    }
    chunkMap->meta[targChunk].x = x;
    chunkMap->meta[targChunk].y = y;
    chunkMap->meta[targChunk].state = SLOT_FULL;
    return &chunkMap->chunks[targChunk];
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

void collectGarbage(ChunkMap *chunkMap, i32 playerX, i32 playerY, i32 renderDistChunks) {
    Point playerChunk;
    playerChunk.x = betterModulo(playerX, CHUNK_SIZE);
    playerChunk.y = betterModulo(playerY, CHUNK_SIZE);

    for (i32 i = 0; i < TABLE_SIZE; i++) {
        if (chunkMap->meta[i].state == SLOT_FULL &&
           (chunkMap->meta[i].x <= playerChunk.x - renderDistChunks ||
            chunkMap->meta[i].x >= playerChunk.x + renderDistChunks ||
            chunkMap->meta[i].y <= playerChunk.y - renderDistChunks ||
            chunkMap->meta[i].y >= playerChunk.y + renderDistChunks) 
        ) {
            chunkMap->meta[i].state = TOMBSTONE;
        }
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

void fillColBottom(u32 *chunk, int col, int fillSize, u32 fillVal, u32 emptyVal) {
    if (col < 0 || col >= CHUNK_SIZE) return;

    if (fillSize > CHUNK_SIZE) fillSize = CHUNK_SIZE;

    for (int row = CHUNK_SIZE - 1; row >= 0; row--) {
        int flatIndex = (row * CHUNK_SIZE) + col;

        int stepsFromBottom = (CHUNK_SIZE - 1) - row;

        if (stepsFromBottom < fillSize) {
            chunk[flatIndex] = fillVal;
        } else {
            chunk[flatIndex] = emptyVal;
        }
    }
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

void generateChunk(i32 x, i32 y, Chunk *chunk) {
    i32 worldX = x * CHUNK_SIZE;
    i32 worldY = y * CHUNK_SIZE;

    for (int i = 0; i < CHUNK_SIZE * CHUNK_SIZE; i++) {
        i32 col = worldX + (i % CHUNK_SIZE);
        i32 colVal = (noise(0.015, 67) + 0.5) * BASE_HEIGHT;

        Tile colFill;
        colFill.data = 0;
        colFill.bits.foreground = 1;

        fillColBottom(&chunk->blocks->data, col, colVal - (y * CHUNK_SIZE), colFill.data, 0);
    }
}

void updateChunks(ChunkMap *map, i32 x, i32 y) {
    Point chunks[1];
    chunks[0] = blockToChunk(x, y);

    Chunk blank;

    int length = sizeof(chunks) / sizeof(chunks[0]); 
    for (int i = 0; i < length; i++) {
        if (getChunk(map, chunks[i].x, chunks[i].y) == NULL) {
            generateChunk(chunks[i].x, chunks[i].y, touchChunk(map, chunks[i].x, chunks[i].y));
        }
    }

    collectGarbage(map, x, y, 1);
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
    game->camera.x = 0;
    game->camera.y = 0;

    game->chunkMap = calloc(1, sizeof(ChunkMap));
    if (game->chunkMap == NULL) {
        perror("couldn't allocate chunkmap");
        exit(1);
    }

    InitWindow(800, 600, "random block game idk bro");
    return 0;
}

int updateGame(Game *game) {
    if (WindowShouldClose()) {
        return 0;
    }

    updateChunks(game->chunkMap, game->camera.x, game->camera.y);

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
    EndDrawing();
    return 1;
}

int endGame(Game *game) {
    free(game->chunkMap);
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
