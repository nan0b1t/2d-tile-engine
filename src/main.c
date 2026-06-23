#define Camera stupid_garbage_lol
#include "raylib.h"
#undef Camera
#include "nums.h"
#include "stdlib.h"
#include <math.h>
#include <stdio.h>

#define BASE_HEIGHT 200
#define BLOCK_SIZE 16

#define CLAMP(val, min, max)                                                   \
    ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define CHUNK_SIZE 32
#define TABLE_SIZE 2048

#define SEED 67

#define CHUNK_MNGR_ACTIVE_CHUNKS_SIZE 16

#define PLAYER_SPEED 700
#define PLAYER_JUMP_FORCE 400
#define PLAYER_GRAVITY 550
#define PLAYER_GRAVITY_FALLING 1000
#define PLAYER_FRICTION 400
#define PLAYER_MAX_SPEED 300
#define PLAYER_MAX_JUMP_SECS 0.2

#define PLAYER_WIDTH  BLOCK_SIZE - 2
#define PLAYER_HEIGHT BLOCK_SIZE * 1.5
#define PLAYER_HB_OFFSET_Y 0
#define PLAYER_HB_OFFSET_X 0

#define EPSILON 0.001f

#define HALF_SCREEN_W GetScreenWidth() / 2
#define HALF_SCREEN_H GetScreenHeight() / 2

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

int frame = 0;


int getTileCoord(float coord, float blockSize) {
    int val = (int)(coord / blockSize);
    float remainder = coord - (val * blockSize);

    if (remainder < 0) {
        val--;
    }
    return val;
}

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
    bool generated;
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

typedef struct ChunkPair {
    TableMeta *meta;
    Chunk *chunk;
} ChunkPair;

ChunkPair touchChunk(ChunkMap *chunkMap, i32 x, i32 y) {
    u32 hash = getTableIndex(x, y);
    u32 startHash = hash;
    i32 targChunk = -1;

    while (1) {
        if (chunkMap->meta[hash].state == SLOT_FULL &&
            chunkMap->meta[hash].x == x && chunkMap->meta[hash].y == y
        ) {
            ChunkPair result;
            result.chunk = &chunkMap->chunks[hash];
            result.meta = &chunkMap->meta[hash];
            return result;
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
        return (ChunkPair) {.meta = NULL, .chunk = NULL};
    }

    chunkMap->meta[targChunk].x = x;
    chunkMap->meta[targChunk].y = y;
    chunkMap->meta[targChunk].state = SLOT_FULL;

    ChunkPair result;
    result.chunk = &chunkMap->chunks[targChunk];
    result.meta = &chunkMap->meta[targChunk];
    return result;
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
    playerChunk.x = playerX / (CHUNK_SIZE * BLOCK_SIZE);
    playerChunk.y = playerY / (CHUNK_SIZE * BLOCK_SIZE);

    for (i32 i = 0; i < TABLE_SIZE; i++) {
        if (chunkMap->meta[i].state == SLOT_FULL &&
           (chunkMap->meta[i].x < playerChunk.x - renderDistChunks ||
            chunkMap->meta[i].x > playerChunk.x + renderDistChunks ||
            chunkMap->meta[i].y < playerChunk.y - renderDistChunks ||
            chunkMap->meta[i].y > playerChunk.y + renderDistChunks) 
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

void hash_2d(int32_t x, int32_t y, int32_t seed, double* out_gx, double* out_gy) {
    uint32_t state = (uint32_t)x * 0x1F1C1F1CU + (uint32_t)y * 0x3A3A3A3AU + ((uint32_t)seed * 0x9E3779B9U);

    state ^= state >> 16;
    state *= 0x7FEB352DU;
    state ^= state >> 15;
    state *= 0x846CA68BU;
    state ^= state >> 16;

    switch (state & 3) {
        case 0: *out_gx =  1.0; *out_gy =  1.0; break;
        case 1: *out_gx = -1.0; *out_gy =  1.0; break;
        case 2: *out_gx =  1.0; *out_gy = -1.0; break;
        case 3: *out_gx = -1.0; *out_gy = -1.0; break;
    }
}

double lerp(double t, double a, double b) {
    return a + t * (b - a);
}


double noise_2d(double x, double y, int32_t seed) {
    int32_t x0 = (int32_t)floor(x);
    int32_t x1 = x0 + 1;
    int32_t y0 = (int32_t)floor(y);
    int32_t y1 = y0 + 1;

    double xf0 = x - (double)x0; double yf0 = y - (double)y0;
    double xf1 = x - (double)x1; double yf1 = y - (double)y0;
    double xf2 = x - (double)x0; double yf2 = y - (double)y1;
    double xf3 = x - (double)x1; double yf3 = y - (double)y1;

    double g0x, g0y, g1x, g1y, g2x, g2y, g3x, g3y;
    hash_2d(x0, y0, seed, &g0x, &g0y);
    hash_2d(x1, y0, seed, &g1x, &g1y);
    hash_2d(x0, y1, seed, &g2x, &g2y);
    hash_2d(x1, y1, seed, &g3x, &g3y);

    double pull0 = (xf0 * g0x) + (yf0 * g0y);
    double pull1 = (xf1 * g1x) + (yf1 * g1y);
    double pull2 = (xf2 * g2x) + (yf2 * g2y);
    double pull3 = (xf3 * g3x) + (yf3 * g3y);

    double u = magicFade(xf0);
    double v = magicFade(yf0);

    double topBlend    = lerp(u, pull0, pull1);
    double bottomBlend = lerp(u, pull2, pull3);

    return lerp(v, topBlend, bottomBlend);
}


void generateChunk(i32 x, i32 y, Chunk *chunk, ChunkMap *map, TableMeta *meta);
Tile *getTile(i32 x, i32 y, ChunkMap *map) {

    i32 cx = x >> 5;
    i32 cy = y >> 5;

    i32 localX = x & 31;
    i32 localY = y & 31;

    ChunkPair pair = touchChunk(map, cx, cy);
    if (pair.chunk == NULL) {
        return NULL; // table full
    }
    if (pair.meta->state == SLOT_FULL && !pair.meta->generated) {
        generateChunk(cx, cy, pair.chunk, map, pair.meta);
    }
    return &pair.chunk->blocks[(localY * 32) + localX];
}


void setTileFG(i32 x, i32 y, ChunkMap *map, u32 fg) {

    i32 cx = x >> 5;
    i32 cy = y >> 5;

    i32 localX = x & 31;
    i32 localY = y & 31;

    ChunkPair pair = touchChunk(map, cx, cy);
    if (pair.chunk == NULL) {
        return; // table full
    }
    if (pair.meta->state == SLOT_FULL && !pair.meta->generated) {
        generateChunk(cx, cy, pair.chunk, map, pair.meta);
    }
    pair.chunk->blocks[(localY * 32) + localX].bits.foreground = fg;
}

void fillColBottom(u32 *chunk, int col,
                   i32 fillSize, u32 fillVal, i32 emptyVal
) {
    if (col < 0 || col >= CHUNK_SIZE) return;

    if (fillSize > CHUNK_SIZE) fillSize = CHUNK_SIZE;

    for (int row = CHUNK_SIZE - 1; row >= 0; row--) {
        int flatIndex = (row * CHUNK_SIZE) + col;

        int stepsFromBottom = (CHUNK_SIZE - 1) - row;

        if (stepsFromBottom < fillSize) {
            chunk[flatIndex] = fillVal;
        } else if (emptyVal != -1){
            chunk[flatIndex] = emptyVal;
        }
    }
}


void generateChunk(i32 x, i32 y, Chunk *chunk, ChunkMap *map, TableMeta *meta) {
    i32 worldX = x * CHUNK_SIZE;
    i32 worldY = y * CHUNK_SIZE;

    // printf("generateChunk chunk(%d,%d) worldBlock=(%d,%d)\n", x, y, worldX, worldY);

    bool needsUp = false;

    for (int i = 0; i < CHUNK_SIZE; i++) {
        i32 localCol = i;
        i32 globalCol = worldX + localCol;

        i32 colVal = (noise(globalCol * 0.004, SEED) + 0.5) * BASE_HEIGHT;

        i32 fillSize = worldY + CHUNK_SIZE - colVal;

        if (fillSize < 0) fillSize = 0;
        if (fillSize > CHUNK_SIZE) fillSize = CHUNK_SIZE;

        Tile dirt;
        dirt.data = 0;
        dirt.bits.foreground = 1;
        dirt.bits.background = 1;

        Tile grass;
        grass.data = 0;
        grass.bits.foreground = 2;
        grass.bits.background = 2;

        Tile stone;
        stone.data = 0;
        stone.bits.foreground = 3;
        stone.bits.background = 3;

        Tile lava;
        lava.data = 0;
        lava.bits.foreground = 4;
        lava.bits.background = 4;

        // fillColBottom((u32*)chunk->blocks, localCol, fillSize, dirt.data, 0);
        i32 grassStart = colVal;
        i32 dirtStart  = grassStart + 2;
        i32 stoneStart = dirtStart + 10;

        for (int j = 0; j < CHUNK_SIZE; j++) {
            i32 projX = globalCol;
            i32 projY = worldY + j;

            if (projY < grassStart) {
                chunk->blocks[j * CHUNK_SIZE + i].data = 0;
            } else if (projY < dirtStart) {
                chunk->blocks[j * CHUNK_SIZE + i].data = grass.data;
            } else if (projY < stoneStart) {
                chunk->blocks[j * CHUNK_SIZE + i].data = dirt.data;
            } else {
                if (worldY < 200) {
                    chunk->blocks[j * CHUNK_SIZE + i].data = stone.data;
                } else {
                    chunk->blocks[j * CHUNK_SIZE + i].data = lava.data;
                }
            }

            if (chunk->blocks[j * CHUNK_SIZE + i].data != 0 && chunk->blocks[j * CHUNK_SIZE + i].bits.foreground != 4) {
                double noise = fabs(noise_2d((double)projX * 0.02, (double)projY * 0.02, SEED));
                double threshold = MIN(MAX(((double)projY / 400) * 0.15, 0.05), 0.15);
                if (noise < threshold && noise > -threshold) {
                    chunk->blocks[j * CHUNK_SIZE + i].bits.foreground = 0;
                }
            }
        }
    }
    meta->generated = true;
}

typedef struct chunkManager {
    ChunkPair chunks[CHUNK_MNGR_ACTIVE_CHUNKS_SIZE];
} chunkManager;

void updateChunks(ChunkMap *map, i32 x, i32 y, i32 renderDistChunks) {
    collectGarbage(map, x, y, renderDistChunks);

    int chunkPixelSize = CHUNK_SIZE * BLOCK_SIZE;
    int centerChunkX = (x < 0) ? (x - chunkPixelSize + 1) / chunkPixelSize : x / chunkPixelSize;
    int centerChunkY = (y < 0) ? (y - chunkPixelSize + 1) / chunkPixelSize : y / chunkPixelSize;

    for (int i = centerChunkX - renderDistChunks; i < centerChunkX + renderDistChunks; i++) {
        for (int j = centerChunkY - renderDistChunks; j < centerChunkY + renderDistChunks; j++) {
            Chunk *chunk = getChunkMut(map, i, j);
            TableMeta *meta;
            if (chunk == NULL) {
                ChunkPair pair = touchChunk(map, i, j);
                chunk = pair.chunk;
                meta = pair.meta;

                generateChunk(i, j, chunk, map, meta);
            }

            if (chunk == NULL) {
                continue; 
            }

            for (int b = 0; b < CHUNK_SIZE * CHUNK_SIZE; b++) {
                int localBlockX = b % CHUNK_SIZE;
                int localBlockY = b / CHUNK_SIZE;

                int worldPixelX = (i * CHUNK_SIZE + localBlockX) * BLOCK_SIZE;
                int worldPixelY = (j * CHUNK_SIZE + localBlockY) * BLOCK_SIZE;

                int drawX = worldPixelX - x + HALF_SCREEN_W;
                int drawY = worldPixelY - y + HALF_SCREEN_H;


                bool drewFG = false;
                switch (chunk->blocks[b].bits.foreground) {
                    case 0: break; /* empty */
                    case 1: ;
                        DrawRectangle(drawX, drawY,
                                      BLOCK_SIZE, BLOCK_SIZE,
                                      (Color) {.r = 150, .g = 75, .b = 0, .a = 255});
                        drewFG = true;
                        break;

                    case 2:
                        DrawRectangle(drawX, drawY,
                                      BLOCK_SIZE, BLOCK_SIZE,
                                      (Color) {.r = 124, .g = 189, .b = 107, .a = 255});
                        drewFG = true;
                        break;

                    case 3:
                        DrawRectangle(drawX, drawY,
                                      BLOCK_SIZE, BLOCK_SIZE,
                                      (Color) {.r = 149, .g = 150, .b = 149, .a = 255});
                        drewFG = true;
                        break;

                    case 4:
                        DrawRectangle(drawX, drawY,
                                      BLOCK_SIZE, BLOCK_SIZE,
                                      (Color) {.r = 255, .g = 165, .b = 0, .a = 255});
                        drewFG = true;
                        break;

                    default:
                        DrawRectangle(drawX, drawY, BLOCK_SIZE, BLOCK_SIZE, MAGENTA);
                        /* used for invalid blocks */
                        break;
                }

                if (!drewFG) {
                    switch (chunk->blocks[b].bits.background) {
                        case 1: ;
                            DrawRectangle(drawX, drawY,
                                          BLOCK_SIZE, BLOCK_SIZE,
                                          (Color) {.r = 150 * 0.5, .g = 75 * 0.5, .b = 0 * 0.5, .a = 255});
                            break;

                        case 2:
                            DrawRectangle(drawX, drawY,
                                          BLOCK_SIZE, BLOCK_SIZE,
                                          (Color) {.r = 124 * 0.5, .g = 189 * 0.5, .b = 107 * 0.5, .a = 255});
                            break;

                        case 3:
                            DrawRectangle(drawX, drawY,
                                          BLOCK_SIZE, BLOCK_SIZE,
                                          (Color) {.r = 149 * 0.5, .g = 150 * 0.5, .b = 149 * 0.5, .a = 255});
                            break;
                    }
                }
            }

            // debug overlay
            if (IsKeyDown(KEY_GRAVE)) {
                i32 worldX = (i * CHUNK_SIZE * BLOCK_SIZE);
                i32 projX = worldX - x;

                i32 worldY = (j * CHUNK_SIZE * BLOCK_SIZE);
                i32 projY = worldY - y;
                DrawRectangleLines(projX, projY, CHUNK_SIZE * BLOCK_SIZE, CHUNK_SIZE * BLOCK_SIZE, 
                              (Color){.r = 255, .g = 0, .b = 0, .a = 255});
            }
        }
    }
}

ChunkMap *getWorld() {
    ChunkMap *result = calloc(1, sizeof(ChunkMap));
    if (result == NULL) {
        perror("failed to allocate chunkmap\n");
        exit(1);
    }

    return result;
}

typedef struct {
    i32 x; /* offset */
    i32 y; /* offset */
    i32 w;
    i32 h;
} Hitbox;

typedef struct Player {
    float velX;
    float velY;
    float x;
    float y;
    Hitbox hb;
    float jumpCounter;
    bool grounded;
    bool hasJumped;
} Player;

void updatePlayer(Player *p, float dt, Camera *cam, ChunkMap *map) {
    bool drawDebug = IsKeyDown(KEY_GRAVE);

    float mv = PLAYER_SPEED * dt;

    // if (IsKeyDown(KEY_A)) p->velX -= mv;
    // if (IsKeyDown(KEY_D)) p->velX += mv;
    if (IsKeyDown(KEY_W) && p->grounded) {
        p->velY = -PLAYER_JUMP_FORCE;
        if (!p->hasJumped) p->jumpCounter = PLAYER_MAX_JUMP_SECS;
        p->hasJumped = true;
    }

    p->jumpCounter -= dt;

    if (IsKeyReleased(KEY_W)) {
        p->grounded = false;
        p->velY *= 0.5;
    }

    if (p->jumpCounter < 0) p->grounded = false;

    // printf("Player grounded: %d, JumpCounter: %f, hasJumped: %d\n", p->grounded, p->jumpCounter, p->hasJumped);
    // if (IsKeyDown(KEY_S)) p->velY += mv;

    /* apply gravity */
    if (p->velY < 0) {
        p->velY += PLAYER_GRAVITY * dt;
    } else {
        p->velY += PLAYER_GRAVITY_FALLING * dt;
    }

    // /* apply friction */
    // if (p->velX > 0 && !IsKeyDown(KEY_RIGHT)) {
    //     p->velX -= PLAYER_FRICTION * dt;
    //     if (p->velX < 0) p->velX = 0;
    // }
    // if (p->velX < 0 && !IsKeyDown(KEY_LEFT)) {
    //     p->velX += PLAYER_FRICTION * dt;
    //     if (p->velX > 0) p->velX = 0;
    // }
    bool moving = IsKeyDown(KEY_A) || IsKeyDown(KEY_D);

    if (moving) {
        // If moving, apply acceleration instead of setting velocity
        if (IsKeyDown(KEY_A)) p->velX -= PLAYER_SPEED * dt;
        if (IsKeyDown(KEY_D)) p->velX += PLAYER_SPEED * dt;
    } else {
        // 2. Only apply friction when NOT moving
        if (p->velX > 0) {
            p->velX -= PLAYER_FRICTION * dt;
            if (p->velX < 0) p->velX = 0;
        } else if (p->velX < 0) {
            p->velX += PLAYER_FRICTION * dt;
            if (p->velX > 0) p->velX = 0;
        }
    }

    /* constrain speed*/
    if (p->velX >  PLAYER_MAX_SPEED) p->velX =  PLAYER_MAX_SPEED;
    if (p->velX < -PLAYER_MAX_SPEED) p->velX = -PLAYER_MAX_SPEED;

    /* construct hitbox */
    p->hb.x = PLAYER_HB_OFFSET_X;
    p->hb.y = PLAYER_HB_OFFSET_Y;

    p->hb.w = PLAYER_WIDTH;
    p->hb.h = PLAYER_HEIGHT;

    float left, right, top, bottom;
    int minX, maxX, minY, maxY;

    #define CALC_BOUNDS do { \
        left = p->x + p->hb.x; \
        right  = p->x + p->hb.x + p->hb.w - EPSILON; \
        top    = p->y + p->hb.y; \
        bottom = p->y + p->hb.y + p->hb.h - EPSILON; \
        \
        minX = (int)floor(left / BLOCK_SIZE); \
        maxX = (int)floor(right / BLOCK_SIZE); \
        minY = (int)floor(top / BLOCK_SIZE); \
        maxY = (int)floor(bottom / BLOCK_SIZE); \
    } while (0)

    /* X MOVEMENT */
    p->x += p->velX * dt;

    CALC_BOUNDS;
    // printf("player feet world px=(%.1f,%.1f) blockcoord=(%d..%d, %d..%d)\n",
            // p->x, bottom, minX, maxX, minY, maxY);

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            Tile *t = getTile(x, y, map);

            if (t != NULL && t->bits.foreground != 0) {
                if (p->velX > 0) { /* Moving Right */
                        p->x = x * BLOCK_SIZE - p->hb.w - p->hb.x /* - EPSILON */;
                } else if (p->velX < 0) { /* Moving Left */
                        p->x = (x + 1) * BLOCK_SIZE - p->hb.x;
                }
                int screenX = (x * BLOCK_SIZE) - (int)cam->x + (HALF_SCREEN_W);
                int screenY = (y * BLOCK_SIZE) - (int)cam->y + (HALF_SCREEN_H);

                if (drawDebug)
                    DrawRectangle(screenX, screenY, BLOCK_SIZE, BLOCK_SIZE, (Color){ 255, 0, 0, 100 });

                p->velX = 0;
                goto endx;
            }
        }
    }
    endx:

    /* Y MOVEMENT */
    p->y += p->velY * dt;

    CALC_BOUNDS;

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            Tile *t = getTile(x, y, map);

            if (t != NULL && t->bits.foreground != 0) {
                if (p->velY > 0) { // moving down
                    p->y = y * BLOCK_SIZE - p->hb.h - p->hb.y /*- EPSILON*/;
                    p->grounded = true;
                    p->hasJumped = false;
                } else if (p->velY < 0) { // moving up
                    p->y = (y + 1) * BLOCK_SIZE - p->hb.y;
                }
                p->velY = 0;

                int screenX = (x * BLOCK_SIZE) - (int)cam->x + (HALF_SCREEN_W);
                int screenY = (y * BLOCK_SIZE) - (int)cam->y + (HALF_SCREEN_H);
                if (drawDebug)
                    DrawRectangle(screenX, screenY, BLOCK_SIZE, BLOCK_SIZE, (Color){255, 0, 0, 100});
                goto endy;
            }
        }
    }
    endy:

    ;
    i32 mx = GetMouseX();
    i32 my = GetMouseY();

    cam->x = p->x + (int)(mx - HALF_SCREEN_W) * 0.5f;
    cam->y = p->y + (int)(my - HALF_SCREEN_H) * 0.5f;

    i32 mouseWorldX = mx + cam->x - HALF_SCREEN_W;
    i32 mouseWorldY = my + cam->y - HALF_SCREEN_H;

    i32 mtx = (i32)floor((float)mouseWorldX / BLOCK_SIZE);
    i32 mty = (i32)floor((float)mouseWorldY / BLOCK_SIZE);

    i32 tileScreenX = (mtx * BLOCK_SIZE) - cam->x + HALF_SCREEN_W;
    i32 tileScreenY = (mty * BLOCK_SIZE) - cam->y + HALF_SCREEN_H;

    DrawRectangle(tileScreenX, tileScreenY, BLOCK_SIZE, BLOCK_SIZE, (Color){211, 211, 211, 100});

    Tile *mTile = getTile(mtx, mty, map);
    if (mTile != NULL) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            mTile->bits.foreground = 0;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && mTile->bits.foreground == 0) {
            mTile->bits.foreground = 1;
        }
    }
}

void drawPlayer(Player *p, Camera *cam) {
    int x = (int)(p->x + p->hb.x) - cam->x + HALF_SCREEN_W;
    int y = (int)(p->y + p->hb.y) - cam->y + HALF_SCREEN_H;


    DrawRectangle(x, y, p->hb.w, p->hb.h, BLUE);
}

typedef struct Game {
    // World world;
    Camera camera;
    ChunkMap *chunkMap;
    Player player;
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

int initGame(Game *game) {
    game->camera.x = 10;
    game->camera.y = 10;

    game->chunkMap = calloc(1, sizeof(ChunkMap));
    if (game->chunkMap == NULL) {
        perror("couldn't allocate chunkmap");
        exit(1);
    }

    InitWindow(800, 600, "random block game idk bro");
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    // DisableCursor();
    return 0;
}

int updateGame(Game *game) {
    if (WindowShouldClose()) {
        return 0;
    }

    float dt = GetFrameTime();
    // if (IsKeyDown(KEY_GRAVE)) EnableCursor();

    BeginDrawing();
    ClearBackground(SKYBLUE);
    updateChunks(game->chunkMap, game->camera.x, game->camera.y, 2);
    updatePlayer(&game->player, dt, &game->camera, game->chunkMap);
    drawPlayer(&game->player, &game->camera);
    DrawFPS(5, 5);
    EndDrawing();
    frame++;
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
