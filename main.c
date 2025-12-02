#define _USE_MATH_DEFINES
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define WINDOW_W    1200
#define WINDOW_H    675
#define GRAVITY     0.88f
#define JUMP_POWER  16.0f
#define SUPER_JUMP_MULT 2.1f
#define ACCEL_GROUND 1.35f
#define ACCEL_AIR    0.58f
#define FRICTION_GROUND 0.79f
#define FRICTION_AIR    0.94f
#define SUPER_DASH_BOOST 18.0f
#define MAX_PLATFORMS 100
#define PLAYER_W    40.0f
#define PLAYER_H    50.0f
#define POWER_FILL_RATE 0.018f
#define AHEAD_DISTANCE 5000.0f
#define BEHIND_DISTANCE 3000.0f

#define NUM_STARS       680
#define NUM_DEBRIS      380
#define MAX_SPEEDLINES  120
#define MAX_PARTICLES   180

typedef struct { float x, y, vx, vy, power; int onGround, graceFrames, score; } Player;
typedef struct { float x, y, w, h; int active; } Platform;
typedef struct { float base_x, y; int brightness, phase, size; } Star;
typedef struct { float base_x, y; float vx; int size; } Debris;
typedef struct { float base_x, y, radius; Uint32 color; float parallax; } Planet;
typedef struct { float x, y, vx, vy, life; Uint32 color; } Particle;
typedef struct { float x, y, len; } SpeedLine;

Player player = {0};
Platform platforms[MAX_PLATFORMS] = {0};
Star stars[NUM_STARS] = {0};
Debris debris[NUM_DEBRIS] = {0};
Planet planets[5] = {0};
Particle particles[MAX_PARTICLES] = {0};
SpeedLine speedlines[MAX_SPEEDLINES] = {0};
int particleCount = 0;
int speedlineCount = 0;

float cameraX = 0;
int game_frame = 0;
SDL_Window* win = NULL;
SDL_Renderer* ren = NULL;

void spawnParticles(float x, float y, int count, Uint32 color) {
    for(int i = 0; i < count && particleCount < MAX_PARTICLES; i++) {
        float angle = 3.7f + (rand() % 140) * 0.008f;
        float speed = 6.0f + (rand() & 15) * 1.1f;
        float life = 32 + (rand() & 31);
        Uint32 c = color;
        if(rand() % 3 == 0) c = 0xFFFFFFFF;
        particles[particleCount++] = (Particle){
            x + 15 + (rand() % 10), y + PLAYER_H - 5,
            cosf(angle) * speed * 0.4f + player.vx * 0.2f,
            sinf(angle) * speed + 2.0f,
            life,
            c
        };
    }
}

void addSpeedLine(float x, float y) {
    if(speedlineCount >= MAX_SPEEDLINES) return;
    speedlines[speedlineCount++] = (SpeedLine){x, y, 35 + rand() % 45};
}

void initGame() {
    srand(time(NULL));
    player = (Player){100, 300, 0, 0, 0.0f, 0, 8, 0};
    cameraX = 0;
    particleCount = speedlineCount = 0;
    game_frame = 0;

    platforms[0] = (Platform){80, 400, 140, 30, 1};
    float lastX = 220;
    for(int i = 1; i < 40 && i < MAX_PLATFORMS; i++) {
        float gap = 80 + (rand() % 70);
        float nx = lastX + gap;
        float sz = 100 + (rand() % 100);
        float hy = 400 + sinf(nx * 0.003f) * 120;
        platforms[i] = (Platform){nx, hy, sz, 30, 1};
        lastX = nx + sz;
    }

    for(int i = 0; i < NUM_STARS; i++) {
        stars[i].base_x = (rand() % 14000) - 5000;
        stars[i].y = rand() % WINDOW_H;
        stars[i].brightness = 100 + (rand() % 155);
        stars[i].phase = rand() & 255;
        stars[i].size = 1 + (rand() % 3);
    }

    for(int i = 0; i < NUM_DEBRIS; i++) {
        debris[i].base_x = (rand() % 16000) - 6000;
        debris[i].y = rand() % WINDOW_H;
        debris[i].vx = 0.15f + (rand() % 50) * 0.01f;
        debris[i].size = 1 + (rand() % 3);
    }

    planets[0] = (Planet){-1500, 100, 90, 0xAA44FFFF, 0.007f};
    planets[1] = (Planet){800, 420, 75, 0x44AAFFFF, 0.009f};
    planets[2] = (Planet){3000, 160, 95, 0xFFAA44FF, 0.005f};
    planets[3] = (Planet){-800, 520, 70, 0xFF6666FF, 0.008f};
    planets[4] = (Planet){2000, 300, 60, 0xFFFF88FF, 0.011f};
}

void updatePlayer() {
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    static int prev_space = 0;
    int space_now = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_UP];
    int jump_pressed = space_now && !prev_space;
    prev_space = space_now;

    game_frame++;

    float farthest_end = player.x;
    for(int i = 0; i < MAX_PLATFORMS; i++) {
        if(platforms[i].active) {
            float end = platforms[i].x + platforms[i].w;
            if(end > farthest_end) farthest_end = end;
        }
    }
    for(int i = 0; i < MAX_PLATFORMS; i++) {
        if(platforms[i].active && platforms[i].x + platforms[i].w < player.x - BEHIND_DISTANCE)
            platforms[i].active = 0;
    }
    while(farthest_end < player.x + AHEAD_DISTANCE) {
        int slot = -1;
        for(int j = 0; j < MAX_PLATFORMS; j++) if(!platforms[j].active) { slot = j; break; }
        if(slot == -1) break;
        float gap = 80 + (rand() % 70);
        float nx = farthest_end + gap;
        float sz = 100 + (rand() % 100);
        float hy = 400 + sinf(nx * 0.003f) * 120;
        platforms[slot] = (Platform){nx, hy, sz, 30, 1};
        farthest_end = nx + sz;
    }

    player.vx *= player.onGround ? FRICTION_GROUND : FRICTION_AIR;
    float accel = player.onGround ? ACCEL_GROUND : ACCEL_AIR;
    if(keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  player.vx -= accel;
    if(keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) player.vx += accel;

    if(fabsf(player.vx) > 10.0f && game_frame % 2 == 0)
        addSpeedLine(player.x + 20 + (rand() % 40), player.y + 20 + (rand() % 30));

    if(jump_pressed) {
        if(player.onGround || player.graceFrames > 0) {
            player.vy = -JUMP_POWER;
            player.onGround = 0;
            player.graceFrames = 0;
            spawnParticles(player.x, player.y, 32, 0xEEFFFFFF);
        } else if(player.power >= 1.0f) {
            player.vy = -JUMP_POWER * SUPER_JUMP_MULT;
            player.vx += SUPER_DASH_BOOST * 1.3f;
            player.power = 0.0f;
            spawnParticles(player.x, player.y, 70, 0xFF4400FF);
        }
    }

    player.vy += GRAVITY;
    player.x += player.vx;
    player.y += player.vy;

    player.onGround = 0;
    for(int i = 0; i < MAX_PLATFORMS; i++) {
        if(!platforms[i].active) continue;
        Platform* p = &platforms[i];
        if(player.x + PLAYER_W > p->x && player.x < p->x + p->w &&
           player.y + PLAYER_H > p->y && player.y < p->y + p->h && player.vy >= 0) {
            player.y = p->y - PLAYER_H;
            player.vy = 0;
            player.onGround = 1;
            player.graceFrames = 8;          // ← God's mercy restored
            if(fabsf(player.vx) > 2.0f)
                spawnParticles(player.x, player.y, 15, 0xFFFFCCFF);
        }
    }

    if(player.onGround && fabsf(player.vx) > 1.0f) {
        player.power += POWER_FILL_RATE;
        if(player.power > 1.0f) player.power = 1.0f;
    }
    if(!player.onGround && player.graceFrames > 0) player.graceFrames--;  // ← mercy fades gently

    cameraX = player.x - 300;
    player.score = (int)(cameraX / 50);

    if(player.y > WINDOW_H + 100) {
        printf("Fallen into the void! Score: %d\n", player.score);
        initGame();
    }
}

void render() {
    for(int y = 0; y < WINDOW_H; y++) {
        int b = 8 + y / 10;
        SDL_SetRenderDrawColor(ren, 0, 0, b, 255);
        SDL_RenderDrawLine(ren, 0, y, WINDOW_W, y);
    }

    for(int i = 0; i < NUM_STARS; i++) {
        float px = stars[i].base_x - cameraX * 0.03f;
        while(px < -10000) px += 20000;
        while(px > 10000) px -= 20000;
        if(px < -50 || px > WINDOW_W + 50) continue;
        float twinkle = 0.6f + 0.4f * sinf((game_frame * 0.07f) + stars[i].phase);
        int b = (int)(stars[i].brightness * twinkle);
        if(b < 70) b = 70;
        int x = (int)px, y = (int)stars[i].y;
        SDL_SetRenderDrawColor(ren, b/4, b/4, b, 60);
        for(int r = 1; r <= stars[i].size + 2; r++) {
            SDL_RenderDrawPoint(ren, x - r, y);
            SDL_RenderDrawPoint(ren, x + r, y);
            SDL_RenderDrawPoint(ren, x, y - r);
            SDL_RenderDrawPoint(ren, x, y + r);
        }
        if(stars[i].size >= 2) {
            SDL_SetRenderDrawColor(ren, b, b, 255, 100);
            for(int a = 0; a < 8; a++) {
                float ang = a * 0.785f;
                int dx = (int)(cosf(ang) * (4 + stars[i].size));
                int dy = (int)(sinf(ang) * (4 + stars[i].size));
                SDL_RenderDrawLine(ren, x, y, x + dx, y + dy);
            }
        }
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        for(int s = 0; s < stars[i].size; s++) {
            SDL_RenderDrawPoint(ren, x, y);
            if(s > 0) {
                SDL_RenderDrawPoint(ren, x + s, y);
                SDL_RenderDrawPoint(ren, x - s, y);
                SDL_RenderDrawPoint(ren, x, y + s);
                SDL_RenderDrawPoint(ren, x, y - s);
            }
        }
    }

    for(int i = 0; i < NUM_DEBRIS; i++) {
        float px = debris[i].base_x - cameraX * 0.3f;
        while(px < -15000) px += 30000;
        while(px > 15000) px -= 30000;
        if(px < -20 || px > WINDOW_W + 20) continue;
        int gray = 60 + (int)(debris[i].vx * 100);
        SDL_SetRenderDrawColor(ren, gray, gray, gray + 40, 255);
        int x = (int)px, y = (int)debris[i].y;
        for(int s = 0; s < debris[i].size; s++) {
            SDL_RenderDrawPoint(ren, x + s, y);
            SDL_RenderDrawPoint(ren, x, y + s);
        }
    }

    for(int i = 0; i < speedlineCount; i++) {
        SpeedLine* s = &speedlines[i];
        float sx = s->x - cameraX;
        if(sx < -100 || sx > WINDOW_W + 100) { *s = speedlines[--speedlineCount]; i--; continue; }
        int alpha = (int)(255 * (1.0f - (game_frame - (int)s->len) / 30.0f));
        if(alpha < 0) alpha = 0;
        SDL_SetRenderDrawColor(ren, 200, 220, 255, alpha);
        SDL_RenderDrawLine(ren, (int)sx, (int)s->y, (int)(sx - 40), (int)s->y);
    }

    for(int i = 0; i < 5; i++) {
        Planet* p = &planets[i];
        float px = p->base_x - cameraX * p->parallax;
        if(px < -200 || px > WINDOW_W + 200) continue;
        int r = (int)p->radius;
        Uint32 c = p->color;
        for(int y = -r; y <= r; y += 5) {
            int hw = (int)sqrtf(r*r - y*y);
            SDL_SetRenderDrawColor(ren, (c>>16)&255, (c>>8)&255, c&255, 180);
            SDL_RenderDrawLine(ren, (int)px - hw, (int)(p->y + y), (int)px + hw, (int)(p->y + y));
        }
    }

    float sunX = 1200 - cameraX * 0.004f;
    SDL_SetRenderDrawColor(ren, 255, 230, 100, 100);
    for(int r = 110; r > 40; r -= 30)
        for(int y = -r; y <= r; y += 12) {
            int hw = (int)sqrtf(r*r - y*y);
            SDL_RenderDrawLine(ren, (int)sunX - hw, 100 + y, (int)sunX + hw, 100 + y);
        }

    for(int i = 0; i < MAX_PLATFORMS; i++) {
        if(!platforms[i].active) continue;
        Platform p = platforms[i];
        SDL_Rect r = {(int)(p.x - cameraX), (int)p.y, (int)p.w, (int)p.h};
        if(r.x + r.w < 0 || r.x > WINDOW_W) continue;
        SDL_SetRenderDrawColor(ren, 110, 110, 130, 255);
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawColor(ren, 70, 70, 90, 255);
        int seed = (int)p.x;
        for(int j = 0; j < 4; j++) {
            seed = seed * 1664525 + 1013904223;
            int x1 = r.x + (seed & 63);
            int y1 = r.y + ((seed >> 8) & 31);
            int x2 = x1 + ((seed >> 16) & 31) - 15;
            int y2 = y1 + ((seed >> 24) & 31) - 15;
            SDL_RenderDrawLine(ren, x1, y1, x2, y2);
        }
        SDL_SetRenderDrawColor(ren, 180, 180, 200, 255);
        SDL_RenderDrawRect(ren, &r);
    }

    for(int i = 0; i < particleCount; i++) {
        Particle* p = &particles[i];
        p->x += p->vx; p->y += p->vy; p->vy += 0.25f; p->life--;
        if(p->life <= 0) { *p = particles[--particleCount]; i--; continue; }
        int alpha = p->life * 11;
        if(alpha > 255) alpha = 255;
        SDL_SetRenderDrawColor(ren,
            (p->color >> 16)&255,
            (p->color >>  8)&255,
            p->color & 255,
            alpha);
        int px = (int)(p->x - cameraX);
        int py = (int)p->y;
        for(int dx = -1; dx <= 1; dx++)
            for(int dy = -1; dy <= 1; dy++)
                SDL_RenderDrawPoint(ren, px + dx, py + dy);
    }

    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_Rect body = {(int)(player.x - cameraX + 10), (int)player.y + 10, (int)PLAYER_W, (int)(PLAYER_H - 10)};
    SDL_RenderFillRect(ren, &body);

    int barX = body.x - 5, barY = body.y - 22, barW = 50, barH = 12;
    SDL_SetRenderDrawColor(ren, 180, 120, 40, 255);
    SDL_RenderDrawRect(ren, &(SDL_Rect){barX, barY, barW, barH});
    SDL_SetRenderDrawColor(ren, 255, 220, 100, 255);
    SDL_RenderDrawRect(ren, &(SDL_Rect){barX+1, barY+1, barW-2, barH-2});
    int fill = (int)(barW * player.power - 2);
    if(fill > 0) {
        SDL_SetRenderDrawColor(ren, 255, 40, 40, 255);
        SDL_RenderFillRect(ren, &(SDL_Rect){barX+2, barY+2, fill, barH-4});
        if(player.power >= 1.0f) {
            int pulse = 80 + (int)(sinf(game_frame * 0.25f) * 40);
            SDL_SetRenderDrawColor(ren, 255, pulse, pulse, 140);
            SDL_RenderDrawRect(ren, &(SDL_Rect){barX-3, barY-3, barW+6, barH+6});
        }
    }

    SDL_RenderPresent(ren);
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    win = SDL_CreateWindow("Dasher", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    initGame();

    while(1) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) if(e.type == SDL_QUIT) return 0;
        updatePlayer();
        render();
        SDL_Delay(16);
    }
}