#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct
{
    float x;
    float y;
}
Point;

typedef struct
{
    int tile;
    Point where;
}
Hit;

typedef struct
{
    Point a;
    Point b;
}
Line;

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int xres;
    int yres;
}
Gpu;

typedef struct
{
    uint32_t* pixels;
    int width;
}
Display;

typedef struct
{
    int top;
    int bot;
    float size;
}
Wall;

typedef struct
{
    Line fov;
    Point where;
    Point velocity;
    float speed;
    float acceleration;
    float theta;
}
Hero;

typedef struct
{
    const char** ceiling;
    const char** walling;
    const char** floring;
}
Map;

// Rotates the player by some radian value.
static Point turn(const Point a, const float t)
{
    const Point b = { a.x * cosf(t) - a.y * sinf(t), a.x * sinf(t) + a.y * cosf(t) };
    return b;
}

// Rotates a point 90 degrees.
static Point rag(const Point a)
{
    const Point b = { -a.y, a.x };
    return b;
}

// Subtracts two points.
static Point sub(const Point a, const Point b)
{
    const Point c = { a.x - b.x, a.y - b.y };
    return c;
}

// Adds two points.
static Point add(const Point a, const Point b)
{
    const Point c = { a.x + b.x, a.y + b.y };
    return c;
}

// Multiplies a point by a scalar value.
static Point mul(const Point a, const float n)
{
    const Point b = { a.x * n, a.y * n };
    return b;
}

// Returns the magnitude of a point.
static float mag(const Point a)
{
    return sqrtf(a.x * a.x + a.y * a.y);
}

// Returns the unit vector of a point.
static Point unit(const Point a)
{
    return mul(a, 1.0f / mag(a));
}

// Returns the slope of a point.
static float slope(const Point a)
{
    return a.y / a.x;
}

// Fast floor (math.h is too slow).
static int fl(const float x)
{
    return (int) x - (x < (int) x);
}

// Fast ceil (math.h is too slow).
static int cl(const float x)
{
    return (int) x + (x > (int) x);
}

// Steps horizontally along a square grid.
static Point sh(const Point a, const Point b)
{
    const float x = b.x > 0.0f ? fl(a.x + 1.0f) : cl(a.x - 1.0f);
    const float y = slope(b) * (x - a.x) + a.y;
    const Point c = { x, y };
    return c;
}

// Steps vertically along a square grid.
static Point sv(const Point a, const Point b)
{
    const float y = b.y > 0.0f ? fl(a.y + 1.0f) : cl(a.y - 1.0f);
    const float x = (y - a.y) / slope(b) + a.x;
    const Point c = { x, y };
    return c;
}

// Returns a decimal value of the ascii tile value on the map.
static int tile(const Point a, const char** const tiles)
{
    const int x = a.x;
    const int y = a.y;
    return tiles[y][x] - '0';
}

// Floating point decimal.
static float dec(const float x)
{
    return x - (int) x;
}

// Casts a ray from <where> in unit <direction> until a <walling> tile is hit.
static Hit cast(const Point where, const Point direction, const char** const walling)
{
    // Determine whether to step horizontally or vertically on the grid.
    const Point hor = sh(where, direction);
    const Point ver = sv(where, direction);
    const Point ray = mag(sub(hor, where)) < mag(sub(ver, where)) ? hor : ver;
    // Due to floating point error, the step may not make it to the next grid square.
    // Three directions (dy, dx, dc) of a tiny step will be added to the ray
    // depending on if the ray hit a horizontal wall, a vertical wall, or the corner
    // of two walls, respectively.
    const Point dc = mul(direction, 0.01f);
    const Point dx = { dc.x, 0.0f };
    const Point dy = { 0.0f, dc.y };
    const Point test = add(ray,
        // Tiny step for corner of two grid squares.
        mag(sub(hor, ver)) < 1e-3f ? dc :
        // Tiny step for vertical grid square.
        dec(ray.x) == 0.0f ? dx :
        // Tiny step for a horizontal grid square.
        dy);
    const Hit hit = { tile(test, walling), ray };
    // If a wall was not hit, then continue advancing the ray.
    return hit.tile ? hit : cast(ray, direction, walling);
}

// Party casting. Returns a percentage of <y> related to <yres> for ceiling and
// floor casting when lerping the floor or ceiling.
static float pcast(const float size, const int yres, const int y)
{
    return size / (2 * (y + 1) - yres);
}

// Rotates a line by some radian amount.
static Line rotate(const Line l, const float t)
{
    const Line line = { turn(l.a, t), turn(l.b, t) };
    return line;
}

// Linear interpolation.
static Point lerp(const Line l, const float n)
{
    return add(l.a, mul(sub(l.b, l.a), n));
}

// Setups the software gpu.
static Gpu setup(const int xres, const int yres, const bool vsync)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* const window = SDL_CreateWindow(
        "littlewolf",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        xres, yres,
        SDL_WINDOW_SHOWN);
    SDL_Renderer* const renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0x0));
    // Notice the flip between xres and yres.
    // The texture is 90 degrees flipped on its side for fast cache access.
    SDL_Texture* const texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        yres, xres);
    if(window == NULL || renderer == NULL || texture == NULL)
    {
        puts(SDL_GetError());
        exit(1);
    }
    const Gpu gpu = { window, renderer, texture, xres, yres };
    return gpu;
}

// Presents the software gpu to the window.
// Calls the real GPU to rotate texture back 90 degrees before presenting.
static void present(const Gpu gpu)
{
    const SDL_Rect dst = {
        (gpu.xres - gpu.yres) / 2,
        (gpu.yres - gpu.xres) / 2,
        gpu.yres, gpu.xres,
    };
    SDL_RenderCopyEx(gpu.renderer, gpu.texture, NULL, &dst, -90, NULL, SDL_FLIP_NONE);
    SDL_RenderPresent(gpu.renderer);
}

// Locks the gpu, returning a pointer to video memory.
static Display lock(const Gpu gpu)
{
    void* screen;
    int pitch;
    SDL_LockTexture(gpu.texture, NULL, &screen, &pitch);
    const Display display = { (uint32_t*) screen, pitch / (int) sizeof(uint32_t) };
    return display;
}

// Places a pixels in gpu video memory.
static void put(const Display display, const int x, const int y, const uint32_t pixel)
{
    display.pixels[y + x * display.width] = pixel;
}

// Unlocks the gpu, making the pointer to video memory ready for presentation
static void unlock(const Gpu gpu)
{
    SDL_UnlockTexture(gpu.texture);
}

// Spins the hero when keys h,l are held down.
static Hero spin(Hero hero, const uint8_t* key)
{
    if(key[SDL_SCANCODE_H]) hero.theta -= 0.1f;
    if(key[SDL_SCANCODE_L]) hero.theta += 0.1f;
    return hero;
}

// Moves the hero when w,a,s,d are held down. Handles collision detection for the walls.
static Hero move(Hero hero, const char** const walling, const uint8_t* key)
{
    const Point last = hero.where, zero = { 0.0f, 0.0f };
    // Accelerates with key held down.
    if(key[SDL_SCANCODE_W] || key[SDL_SCANCODE_S] || key[SDL_SCANCODE_D] || key[SDL_SCANCODE_A])
    {
        const Point reference = { 1.0f, 0.0f };
        const Point direction = turn(reference, hero.theta);
        const Point acceleration = mul(direction, hero.acceleration);
        if(key[SDL_SCANCODE_W]) hero.velocity = add(hero.velocity, acceleration);
        if(key[SDL_SCANCODE_S]) hero.velocity = sub(hero.velocity, acceleration);
        if(key[SDL_SCANCODE_D]) hero.velocity = add(hero.velocity, rag(acceleration));
        if(key[SDL_SCANCODE_A]) hero.velocity = sub(hero.velocity, rag(acceleration));
    }
    // Otherwise, decelerates (exponential decay).
    else hero.velocity = mul(hero.velocity, 1.0f - hero.acceleration / hero.speed);
    // Caps velocity if top speed is exceeded.
    if(mag(hero.velocity) > hero.speed) hero.velocity = mul(unit(hero.velocity), hero.speed);
    // Moves.
    hero.where = add(hero.where, hero.velocity);
    // Sets velocity to zero if there is a collision and puts hero back in bounds.
    if(tile(hero.where, walling))
    {
        hero.velocity = zero;
        hero.where = last;
    }
    return hero;
}

// Returns a color value (RGB) from a decimal tile value.
static uint32_t color(const int tile)
{
    switch(tile)
    {
    default:
    case 1: return 0x00AA0000; // Red.
    case 2: return 0x0000AA00; // Green.
    case 3: return 0x000000AA; // Blue.
    }
}

// Calculates wall size using the <corrected> ray to the wall.
static Wall project(const int xres, const int yres, const float focal, const Point corrected)
{
    // Normal distance of corrected ray is clamped to some small value else wall size will shoot to infinity.
    const float normal = corrected.x < 1e-2f ? 1e-2f : corrected.x;
    const float size = 0.5f * focal * xres / normal;
    const int top = (yres + size) / 2.0f;
    const int bot = (yres - size) / 2.0f;
    // Top and bottom values are clamped to screen size else renderer will waste cycles
    // (or segfault) when rasterizing pixels off screen.
    const Wall wall = { top > yres ? yres : top, bot < 0 ? 0 : bot, size };
    return wall;
}

// Renders the entire scene from the <hero> perspective given a <map> and a software <gpu>.
static void render(const Hero hero, const Map map, const Gpu gpu)
{
    const int t0 = SDL_GetTicks();
    const Line camera = rotate(hero.fov, hero.theta);
    const Display display = lock(gpu);
    // Ray cast for all columns of the window.
    for(int x = 0; x < gpu.xres; x++)
    {
        const Point direction = lerp(camera, x / (float) gpu.xres);
        const Hit hit = cast(hero.where, direction, map.walling);
        const Point ray = sub(hit.where, hero.where);
        const Line trace = { hero.where, hit.where };
        const Point corrected = turn(ray, -hero.theta);
        const Wall wall = project(gpu.xres, gpu.yres, hero.fov.a.x, corrected);
        // Renders flooring.
        for(int y = 0; y < wall.bot; y++)
            put(display, x, y, color(tile(lerp(trace, -pcast(wall.size, gpu.yres, y)), map.floring)));
        // Renders wall.
        for(int y = wall.bot; y < wall.top; y++)
            put(display, x, y, color(hit.tile));
        // Renders ceiling.
        for(int y = wall.top; y < gpu.yres; y++)
            put(display, x, y, color(tile(lerp(trace, +pcast(wall.size, gpu.yres, y)), map.ceiling)));
    }
    unlock(gpu);
    present(gpu);
    // Caps frame rate to ~60 fps if the vertical sync (VSYNC) init failed.
    const int t1 = SDL_GetTicks();
    const int ms = 16 - (t1 - t0);
    SDL_Delay(ms < 0 ? 0 : ms);
}

static bool done()
{
    SDL_Event event;
    SDL_PollEvent(&event);
    return event.type == SDL_QUIT
        || event.key.keysym.sym == SDLK_END
        || event.key.keysym.sym == SDLK_ESCAPE;
}

// Changes the field of view. A focal value of 1.0 is 90 degrees.
static Line viewport(const float focal)
{
    const Line fov = {
        { focal, -1.0f },
        { focal, +1.0f },
    };
    return fov;
}

static Hero born(const float focal)
{
    const Hero hero = {
        viewport(focal),
        // Where.
        { 3.5f, 3.5f },
        // Velocity.
        { 0.0f, 0.0f },
        // Speed.
        0.10f,
        // Acceleration.
        0.015f,
        // Theta radians.
        0.0f
    };
    return hero;
}

// Builds the map. Note the static prefix for the parties. Map lives in .bss in private.
static Map build()
{
    static const char* ceiling[] = {
        "111111111111111111111111111111111111111111111",
        "122223223232232111111111111111222232232322321",
        "122222221111232111111111111111222222211112321",
        "122221221232323232323232323232222212212323231",
        "122222221111232111111111111111222222211112321",
        "122223223232232111111111111111222232232322321",
        "111111111111111111111111111111111111111111111",
    };
    static const char* walling[] = {
        "111111111111111111111111111111111111111111111",
        "100000000000000111111111111111000000000000001",
        "103330001111000111111111111111033300011110001",
        "103000000000000000000000000000030000030000001",
        "103330001111000111111111111111033300011110001",
        "100000000000000111111111111111000000000000001",
        "111111111111111111111111111111111111111111111",
    };
    static const char* floring[] = {
        "111111111111111111111111111111111111111111111",
        "122223223232232111111111111111222232232322321",
        "122222221111232111111111111111222222211112321",
        "122222221232323323232323232323222222212323231",
        "122222221111232111111111111111222222211112321",
        "122223223232232111111111111111222232232322321",
        "111111111111111111111111111111111111111111111",
    };
    const Map map = { ceiling, walling, floring };
    return map;
}

// Get Psyched!
int main(int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    const Gpu gpu = setup(700, 400, true);
    const Map map = build();
    Hero hero = born(0.8f);
    while(!done())
    {
        const uint8_t* key = SDL_GetKeyboardState(NULL);
        hero = spin(hero, key);
        hero = move(hero, map.walling, key);
        render(hero, map, gpu);
    }
    // No need to free anything - gives quick exit.
    return 0;
}
