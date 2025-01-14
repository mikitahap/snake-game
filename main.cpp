#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h> 

extern "C" {
#include "./SDL2-2.0.10/include/SDL.h"
#include "./SDL2-2.0.10/include/SDL_main.h"
}

#define SCREEN_WIDTH    640
#define SCREEN_HEIGHT   480
#define GAME_WIDTH      480
#define INFO_WIDTH      (SCREEN_WIDTH - GAME_WIDTH)
#define SNAKE_WIDTH 20
#define SNAKE_HEIGHT 20
#define SPEEDUP_INTERVAL 30.0
#define SPEEDUP_FACTOR 0.2 
#define REDPOINT_TIMER 5.0
#define SNAKE_SPEED 0.1
#define SLOWDOWN_FACTOR 1.5

void DrawString(SDL_Surface* screen, int x, int y, const char* text, SDL_Surface* charset) {
    int px, py, c;
    SDL_Rect s, d;
    s.w = 8;
    s.h = 8;
    d.w = 8;
    d.h = 8;
    while (*text) {
        c = *text & 255;
        px = (c % 16) * 8;
        py = (c / 16) * 8;
        s.x = px;
        s.y = py;
        d.x = x;
        d.y = y;
        SDL_BlitSurface(charset, &s, screen, &d);
        x += 8;
        text++;
    };
}

void DrawSurface(SDL_Surface* screen, SDL_Surface* sprite, int x, int y) {
    SDL_Rect dest;
    dest.x = x - sprite->w / 2;
    dest.y = y - sprite->h / 2;
    dest.w = sprite->w;
    dest.h = sprite->h;
    SDL_BlitSurface(sprite, NULL, screen, &dest);
}

void DrawPixel(SDL_Surface* surface, int x, int y, Uint32 color) {
    int bpp = surface->format->BytesPerPixel;
    Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;
    *(Uint32*)p = color;
}

void DrawLine(SDL_Surface* screen, int x, int y, int l, int dx, int dy, Uint32 color) {
    for (int i = 0; i < l; i++) {
        DrawPixel(screen, x, y, color);
        x += dx;
        y += dy;
    };
}

void DrawRectangle(SDL_Surface* screen, int x, int y, int l, int k, Uint32 outlineColor, Uint32 fillColor) {
    DrawLine(screen, x, y, k, 0, 1, outlineColor);
    DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor);
    DrawLine(screen, x, y, l, 1, 0, outlineColor);
    DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor);
    for (int i = y + 1; i < y + k - 1; i++)
        DrawLine(screen, x + 1, i, l - 2, 1, 0, fillColor);
}

int InitializeSDL(SDL_Window** window, SDL_Renderer** renderer, SDL_Surface** screen, SDL_Texture** scrtex) {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    int rc = SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0, window, renderer);
    if (rc != 0) {
        SDL_Quit();
        printf("SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(*renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);
    SDL_SetWindowTitle(*window, "Snake Game");

    *screen = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    *scrtex = SDL_CreateTexture(*renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

    SDL_ShowCursor(SDL_DISABLE);
    return 0;
}

SDL_Surface* LoadCharset() {
    SDL_Surface* charset = SDL_LoadBMP("./cs8x8.bmp");
    if (charset == NULL) {
        printf("SDL_LoadBMP(cs8x8.bmp) error: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_SetColorKey(charset, true, 0x000000);
    return charset;
}

void DrawProgressBar(SDL_Surface* screen, int x, int y, int width, int height, double progress, Uint32 barColor, Uint32 bgColor) {
    DrawRectangle(screen, x, y, width, height, bgColor, bgColor);
    int filledWidth = (int)(width * progress);
    DrawRectangle(screen, x, y, filledWidth, height, barColor, barColor);
}

void RenderGame(SDL_Surface* screen, SDL_Surface* charset, double worldTime, double fps, double redPointProgress, int score) {
    char text[128];
    int black = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
    int red = SDL_MapRGB(screen->format, 0xFF, 0x00, 0x00);
    int blue = SDL_MapRGB(screen->format, 0x11, 0x11, 0xCC);
    int green = SDL_MapRGB(screen->format, 0x00, 0xFF, 0x00);

    SDL_FillRect(screen, NULL, black);
    DrawRectangle(screen, 0, 0, GAME_WIDTH, SCREEN_HEIGHT, blue, black);
    DrawRectangle(screen, GAME_WIDTH, 0, INFO_WIDTH, SCREEN_HEIGHT, blue, black);

    snprintf(text, sizeof(text), "Time: %.1lf s", worldTime);
    DrawString(screen, GAME_WIDTH + 10, 10, text, charset);
    snprintf(text, sizeof(text), "Controls:");
    DrawString(screen, GAME_WIDTH + 10, 50, text, charset);
    snprintf(text, sizeof(text), "Esc - Exit");
    DrawString(screen, GAME_WIDTH + 10, 70, text, charset);
    snprintf(text, sizeof(text), "N - New Game");
    DrawString(screen, GAME_WIDTH + 10, 90, text, charset);
    snprintf(text, sizeof(text), "Score: %d", score);
    DrawString(screen, GAME_WIDTH + 10, 130, text, charset);

    DrawString(screen, GAME_WIDTH + 10, 200, "Red Dot Timer:", charset);
    DrawProgressBar(screen, GAME_WIDTH + 10, 220, INFO_WIDTH - 20, 20, redPointProgress, red, black);
}


enum Direction { UP, DOWN, LEFT, RIGHT };

struct Snake {
    int x[100]; 
    int y[100];
    int length;
    Direction dir; 
    double moveTimer; 
    double speed;
};


bool CanMove(Snake* snake, Direction dir) {
    switch (dir) {
    case UP:
        return snake->y[0] > 0;
    case DOWN:
        return snake->y[0] + SNAKE_HEIGHT < SCREEN_HEIGHT;
    case LEFT:
        return snake->x[0] > 0;
    case RIGHT:
        return snake->x[0] + SNAKE_WIDTH < GAME_WIDTH;
    }
    return false;
}

void TurnSnake(Snake* snake) {
    switch (snake->dir) {
    case UP:
        if (CanMove(snake, RIGHT)) {
            snake->dir = RIGHT;
        }
        else if (CanMove(snake, LEFT)) {
            snake->dir = LEFT;
        }
        break;
    case DOWN:
        if (CanMove(snake, LEFT)) {
            snake->dir = LEFT;
        }
        else if (CanMove(snake, RIGHT)) {
            snake->dir = RIGHT;
        }
        break;
    case LEFT:
        if (CanMove(snake, DOWN)) {
            snake->dir = DOWN;
        }
        else if (CanMove(snake, UP)) {
            snake->dir = UP;
        }
        break;
    case RIGHT:
        if (CanMove(snake, UP)) {
            snake->dir = UP;
        }
        else if (CanMove(snake, DOWN)) {
            snake->dir = DOWN;
        }
        break;
    }
}

void MoveSnake(Snake* snake, double delta, double worldTime) {
    double startspeed = SNAKE_SPEED;

    if (worldTime > SPEEDUP_INTERVAL) {
        int speedups = (int)(worldTime / SPEEDUP_INTERVAL);
        snake->speed = startspeed - (SPEEDUP_FACTOR * startspeed * speedups);
        if (snake->speed < 0.02) {
            snake->speed = 0.02;
        }
    }

    snake->moveTimer += delta;
    if (snake->moveTimer < snake->speed) {
        return;
    }

    snake->moveTimer = 0.0;

    if (!CanMove(snake, snake->dir)) {
        TurnSnake(snake);
    }

    for (int i = snake->length - 1; i > 0; i--) {
        snake->x[i] = snake->x[i - 1];
        snake->y[i] = snake->y[i - 1];
    }

    switch (snake->dir) {
    case UP:
        snake->y[0] -= SNAKE_HEIGHT;
        break;
    case DOWN:
        snake->y[0] += SNAKE_HEIGHT;
        break;
    case LEFT:
        snake->x[0] -= SNAKE_WIDTH;
        break;
    case RIGHT:
        snake->x[0] += SNAKE_WIDTH;
        break;
    }
}

void RenderSnake(SDL_Surface* screen, Snake* snake, Uint32 color) {
    for (int i = 0; i < snake->length; i++) {
        DrawRectangle(screen, snake->x[i], snake->y[i], SNAKE_WIDTH, SNAKE_HEIGHT, color, color);
    }
}

void ResetGame(Snake* snake, double* worldTime) {
    snake->length = 4;
    snake->x[0] = GAME_WIDTH / 2;
    snake->y[0] = SCREEN_HEIGHT / 2;
    snake->dir = RIGHT;
    snake->moveTimer = 0.0;
    snake->speed = SNAKE_SPEED;
    *worldTime = 0.0;
}


struct Point {
    int x;
    int y;
};

struct redPoint {
    int x;
    int y;
    double timer;
    double spawnDelay;
    bool active;
	int effectType;
	int shortenAmount;
	double slowdownFactor;
};


redPoint GenerateRedPoint() {
    redPoint r;
    r.x = (rand() % (GAME_WIDTH / SNAKE_WIDTH)) * SNAKE_WIDTH;
    r.y = (rand() % (SCREEN_HEIGHT / SNAKE_HEIGHT)) * SNAKE_HEIGHT;
    r.timer = REDPOINT_TIMER;
    r.spawnDelay = 0.0;
    r.active = true;
    r.effectType = rand() % 2 + 1;
    r.slowdownFactor = SLOWDOWN_FACTOR;
    r.shortenAmount = 1;
    return r;
}

void ShortenSnake(Snake* snake, int amount) {
    if (snake->length > amount) {
        snake->length -= amount;
    }
    else {
        snake->length = 1;
    }
}

void ApplySlowdownEffect(Snake* snake, double factor){
    snake->speed *= factor;
    
}


bool CheckIfSnakeAteRedPoint(Snake* snake, redPoint* point) {
    return (snake->x[0] == point->x && snake->y[0] == point->y);
}

Point GenerateRandomPoint() {
    Point p;
    p.x = (rand() % (GAME_WIDTH / SNAKE_WIDTH)) * SNAKE_WIDTH;
    p.y = (rand() % (SCREEN_HEIGHT / SNAKE_HEIGHT)) * SNAKE_HEIGHT;
    return p;
}

bool CheckIfSnakeAtePoint(Snake* snake, Point* point) {
    return (snake->x[0] == point->x && snake->y[0] == point->y);
}



void IncreaseSnakeLength(Snake* snake) {
    if (snake->length < 100) { 
        snake->length++;
    }
}

void RenderRedPoint(SDL_Surface* screen, redPoint* point) {
    int red = SDL_MapRGB(screen->format, 0xFF, 0x00, 0x00);
    DrawRectangle(screen, point->x, point->y, SNAKE_WIDTH, SNAKE_HEIGHT, red, red);
}

void RenderPoint(SDL_Surface* screen, Point* point) {
    int blue = SDL_MapRGB(screen->format, 0x00, 0x00, 0xFF);
    DrawRectangle(screen, point->x, point->y, SNAKE_WIDTH, SNAKE_HEIGHT, blue, blue);
}

bool CheckIfSnakeHitItself(Snake* snake) {

    for (int i = 1; i < snake->length; i++) {
        if (snake->x[0] == snake->x[i] && snake->y[0] == snake->y[i]) {
            return true; 
        }
    }
    return false;
}

void HandleGameOver(SDL_Surface* screen, SDL_Surface* charset, Snake* snake, double* worldTime, SDL_Texture* scrtex, SDL_Renderer* renderer, int* quit) {
    char text[128];
    int red = SDL_MapRGB(screen->format, 0xFF, 0x00, 0x00);

    snprintf(text, sizeof(text), "Game Over!");
    DrawString(screen, GAME_WIDTH - 270, 150, text, charset);
    snprintf(text, sizeof(text), "Press 'Esc' to quit or 'n' to start a new game.");
    DrawString(screen, GAME_WIDTH - 400, 170, text, charset);

    SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
    SDL_RenderCopy(renderer, scrtex, NULL, NULL);
    SDL_RenderPresent(renderer);
	int quit1 = 0;

    while (!(quit1)) { 
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    *quit = 1;
					quit1 = 1;
                    break;
                case SDLK_n:
                    ResetGame(snake, worldTime);
					quit1 = 1;
                    break;
                }
            }
        }
    }
}

void GameLoop(SDL_Surface* screen, SDL_Texture* scrtex, SDL_Renderer* renderer, SDL_Surface* charset) {
    int t1, t2, quit, frames, score = 0;
    double delta, worldTime, fpsTimer, fps, speed;
    SDL_Event event;

    Snake snake = { GAME_WIDTH / 2, SCREEN_HEIGHT / 2, RIGHT, 0.0 };
    Uint32 snakeColor = SDL_MapRGB(screen->format, 0x00, 0xFF, 0x00);

    Point point = GenerateRandomPoint();
    redPoint redpoint = GenerateRedPoint();
    ResetGame(&snake, &worldTime);
    t1 = SDL_GetTicks();
    frames = 0;
    fpsTimer = 0;
    fps = 0;
    worldTime = 0.0;
    speed = 0.1;
    quit = 0;

    while (!quit) {
        t2 = SDL_GetTicks();
        delta = (t2 - t1) / 1000.0;
        t1 = t2;
        worldTime += delta;
        fpsTimer += delta;
        frames++;

        if (fpsTimer > 1.0) {
            fps = frames;
            fpsTimer -= 1.0;
            frames = 0;
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_UP:
                    if (snake.dir != DOWN) snake.dir = UP;
                    break;
                case SDLK_DOWN:
                    if (snake.dir != UP) snake.dir = DOWN;
                    break;
                case SDLK_LEFT:
                    if (snake.dir != RIGHT) snake.dir = LEFT;
                    break;
                case SDLK_RIGHT:
                    if (snake.dir != LEFT) snake.dir = RIGHT;
                    break;
                case SDLK_ESCAPE:
                    quit = 1;
                    break;
                case SDLK_n:
                    ResetGame(&snake, &worldTime);
                    score = 0;
                    break;
                }
            }
        }

        if (CheckIfSnakeHitItself(&snake)) {
            HandleGameOver(screen, charset, &snake, &worldTime, scrtex, renderer, &quit);
            continue;
        }

        MoveSnake(&snake, delta, worldTime);

        if (CheckIfSnakeAtePoint(&snake, &point)) {
            IncreaseSnakeLength(&snake);
            point = GenerateRandomPoint();
            score++;
        }

        if (CheckIfSnakeAteRedPoint(&snake, &redpoint)) {
            if (redpoint.effectType == 1) {
                ShortenSnake(&snake, redpoint.shortenAmount);
            }
            else if (redpoint.effectType == 2) {
                ApplySlowdownEffect(&snake, redpoint.slowdownFactor);
            }
            redpoint.active = false;
            redpoint.x = 900;
            redpoint.spawnDelay = (rand() % 7) + 3;
            score++;
        }

        if (redpoint.active) {
            redpoint.timer -= delta;
            if (redpoint.timer <= 0) {
                redpoint.active = false;
                redpoint.spawnDelay = (rand() % 7) + 3;
            }
        }
        else {
            redpoint.spawnDelay -= delta;
            if (redpoint.spawnDelay <= 0) {
                redpoint = GenerateRedPoint();
            }
        }

        RenderGame(screen, charset, worldTime, fps, redpoint.active ? redpoint.timer / REDPOINT_TIMER : 0.0, score);

        RenderSnake(screen, &snake, snakeColor);
        RenderPoint(screen, &point);
        if (redpoint.active) {
            RenderRedPoint(screen, &redpoint);
        }

        SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
        SDL_RenderCopy(renderer, scrtex, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char* argv[]) {
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Surface* screen = NULL;
    SDL_Texture* scrtex = NULL;

    if (InitializeSDL(&window, &renderer, &screen, &scrtex)) {
        return 1;
    }

    SDL_Surface* charset = LoadCharset();
    if (charset == NULL) {
        SDL_Quit();
        return 1;
    }
    GameLoop(screen, scrtex, renderer, charset);

    SDL_FreeSurface(charset);
    SDL_DestroyTexture(scrtex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
