#include <SDL2/SDL.h>

typedef struct
{
    float x;
    float y;
}
Point;

static Point turn(const Point a, const float t)
{
    const Point b = { a.x * cosf(t) - a.y * sinf(t), a.x * sinf(t) + a.y * cosf(t) };
    return b;
}

// Right angle rotation.
static Point rag(const Point a)
{
    const Point b = { -a.y, a.x };
    return b;
}

static Point sub(const Point a, const Point b)
{
    const Point c = { a.x - b.x, a.y - b.y };
    return c;
}

static Point add(const Point a, const Point b)
{
    const Point c = { a.x + b.x, a.y + b.y };
    return c;
}

static Point mul(const Point a, const float n)
{
    const Point b = { a.x * n, a.y * n };
    return b;
}

static float mag(const Point a)
{
    return sqrtf(a.x * a.x + a.y * a.y);
}

static Point dvd(const Point a, const float n)
{
    const Point b = { a.x / n, a.y / n };
    return b;
}

static Point unt(const Point a)
{
    return dvd(a, mag(a));
}

static float slope(const Point a)
{
    return a.y / a.x;
}

// Fast floor (math).
static int fl(const float x)
{
    const int i = x;
    return i - (x < i);
}

// Fast ceil (math).
static int cl(const float x)
{
    const int i = x;
    return i + (x > i);
}

// Steps horizontally along a grid.
static Point sh(const Point a, const Point b)
{
    const float x = b.x > 0.0 ? fl(a.x + 1.0) : cl(a.x - 1.0);
    const float y = slope(b) * (x - a.x) + a.y;
    const Point c = { x, y };
    return c;
}

// Steps vertically along a grid.
static Point sv(const Point a, const Point b)
{
    const float y = b.y > 0.0 ? fl(a.y + 1.0) : cl(a.y - 1.0);
    const float x = (y - a.y) / slope(b) + a.x;
    const Point c = { x, y };
    return c;
}

static Point cmp(const Point a, const Point b, const Point c)
{
    return mag(sub(b, a)) < mag(sub(c, a)) ? b : c;
}

static char tile(const Point a, const char** const tiles)
{
    const int x = a.x;
    const int y = a.y;
    return tiles[y][x] - '0';
}

typedef struct
{
    char tile;
    float offset;
    Point where;
}
Hit;

// Returns the decimal portion of a float.
static float dec(const float x)
{
    return x - (int) x;
}

static Hit collision(const Point hook, const Point direction, const char** const walling)
{
    const float epsilon = 1e-4;
    const Hit hit = {
        tile(add(hook, mul(direction, epsilon)), walling),
        dec(hook.x) + dec(hook.y),
        hook
    };
    return hit;
}

static Hit cast(const Point ray, const Point direction, const char** const walling)
{
    const Point hook = cmp(ray, sh(ray, direction), sv(ray, direction));
    const Hit hit = collision(hook, direction, walling);
    return hit.tile ? hit : cast(hook, direction, walling);
}

typedef struct
{
    Point a;
    Point b;
}
Line;

static float focal(const Line l)
{
    return l.a.x / (l.b.y - l.a.y);
}

static float fcast(const Line fov, const int res, const int y)
{
    return focal(fov) * res / (2 * y - res);
}

static float ccast(const Line fov, const int res, const int y)
{
    return -fcast(fov, res, y);
}

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

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
}
Gpu;

static Gpu setup(const int res)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* const window = SDL_CreateWindow("water", 0, 0, res, res, SDL_WINDOW_SHOWN);
    SDL_Renderer* const renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* const texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, res, res);
    const Gpu gpu = { window, renderer, texture };
    return gpu;
}

static void release(const Gpu gpu)
{
    SDL_DestroyWindow(gpu.window);
    SDL_DestroyRenderer(gpu.renderer);
    SDL_DestroyTexture(gpu.texture);
    SDL_Quit();
}

static void present(const Gpu gpu)
{
    SDL_RenderCopyEx(gpu.renderer, gpu.texture, NULL, NULL, 90.0, NULL, SDL_FLIP_VERTICAL);
    SDL_RenderPresent(gpu.renderer);
}

typedef struct
{
    uint32_t* pixels;
    int width;
}
Display;

static Display lock(const Gpu gpu)
{
    void* screen;
    int pitch;
    SDL_LockTexture(gpu.texture, NULL, &screen, &pitch);
    const Display display = { (uint32_t*) screen, pitch / (int) sizeof(uint32_t) };
    return display;
}

static void put(const Display display, const int x, const int y, const uint32_t pixel)
{
    display.pixels[y + x * display.width] = pixel;
}

static void unlock(const Gpu gpu)
{
    SDL_UnlockTexture(gpu.texture);
}

typedef struct
{
    int top;
    int bot;
}
Wall;

static Wall project(const int res, const Line fov, const Point corrected)
{
    const int height = focal(fov) * res / corrected.x + 0.5;
    const int top = (res - height) / 2;
    const int bot = (res - top);
    const Wall wall = { top < 0 ? 0 : top, bot > res ? res : bot };
    return wall;
}

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

static Hero spin(const Hero hero)
{
    const uint8_t* key = SDL_GetKeyboardState(NULL);
    SDL_PumpEvents();
    Hero step = hero;
    if(key[SDL_SCANCODE_H]) step.theta -= 0.1;
    if(key[SDL_SCANCODE_L]) step.theta += 0.1;
    return step;
}

static Hero move(const Hero hero, const char** const walling)
{
    const uint8_t* key = SDL_GetKeyboardState(NULL);
    SDL_PumpEvents();
    Hero step = hero;
    // Accelerates if <WASD>.
    if(key[SDL_SCANCODE_W] || key[SDL_SCANCODE_S] || key[SDL_SCANCODE_D] || key[SDL_SCANCODE_A])
    {
        const Point reference = { 1.0, 0.0 };
        const Point direction = turn(reference, step.theta);
        const Point acceleration = mul(direction, step.acceleration);
        if(key[SDL_SCANCODE_W]) step.velocity = add(step.velocity, acceleration);
        if(key[SDL_SCANCODE_S]) step.velocity = sub(step.velocity, acceleration);
        if(key[SDL_SCANCODE_D]) step.velocity = add(step.velocity, rag(acceleration));
        if(key[SDL_SCANCODE_A]) step.velocity = sub(step.velocity, rag(acceleration));
    }
    // Otherwise, decelerates (exponential decay).
    else step.velocity = mul(step.velocity, 1.0 - step.acceleration / step.speed);
    // Caps velocity if top speed is exceeded.
    if(mag(step.velocity) > step.speed) step.velocity = mul(unt(step.velocity), step.speed);
    // Moves
    step.where = add(step.where, step.velocity);
    // Sets velocity to zero if there is a collision and puts hero back in bounds.
    const Point zero = { 0.0, 0.0};
    if(tile(step.where, walling)) step.velocity = zero, step.where = hero.where;
    return step;
}

typedef struct
{
    const char** ceiling;
    const char** walling;
    const char** flooring;
}
Map;

static uint32_t color(const char tile)
{
    return 0xAA << (8 * (tile - 1));
}

static void render(const Hero hero, const Map map, const int res, const Gpu gpu)
{
    const int t0 = SDL_GetTicks();
    const Line camera = rotate(hero.fov, hero.theta);
    const Display display = lock(gpu);
    for(int x = 0; x < res; x++)
    {
        // Casts a ray.
        const Point column = lerp(camera, x / (float) res);
        const Hit hit = cast(hero.where, column, map.walling);
        const Point ray = sub(hit.where, hero.where);
        const Point corrected = turn(ray, -hero.theta);
        const Wall wall = project(res, hero.fov, corrected);
        const Line trace = { hero.where, hit.where };
        // Renders ceiling. 
        for(int y = 0; y < wall.top; y++)
            // Subtracts one from y to prevent overcast edge cases.
            put(display, x, y, color(
                tile(lerp(trace, ccast(hero.fov, res, y - 1) / corrected.x), map.ceiling)
            ));
        // Renders wall.
        for(int y = wall.top; y < wall.bot; y++)
            put(display, x, y, color(
                hit.tile
            ));
        // Renders flooring.
        for(int y = wall.bot; y < res; y++)
            // Adds one to y to prevent overcast edge cases.
            put(display, x, y, color(
                tile(lerp(trace, fcast(hero.fov, res, y + 1) / corrected.x), map.flooring)
            ));
    }
    unlock(gpu);
    present(gpu);
    // Caps frame rate.
    const int t1 = SDL_GetTicks();
    const int ms = 15 - (t1 - t0);
    SDL_Delay(ms < 0 ? 0 : ms);
}

static int finished()
{
    SDL_Event event;
    SDL_PollEvent(&event);
    if(event.type == SDL_QUIT || event.key.keysym.sym == SDLK_END)
        return 1;
    return 0;
}

int main()
{
    const int res = 500;
    const Gpu gpu = setup(res);
    const char* ceiling[] = {
        "1111111111111111",
        "1222232232322321",
        "122222221111232111111111111111",
        "122221221232323232323232323231",
        "122222221111232111111111111111",
        "1222232232322321",
        "1111111111111111",
    };
    const char* walling[] = {
        "1111111111111111",
        "1000000000000001",
        "103330001111000111111111111111",
        "103000003000020000000000000001",
        "103330001111000111111111111111",
        "1000000000000001",
        "1111111111111111",
    };
    const char* flooring[] = {
        "1111111111111111",
        "1222232232322321",
        "122222221111232111111111111111",
        "122222221232323323232323232321",
        "122222221111232111111111111111",
        "1222232232322321",
        "1111111111111111",
    };
    const Map map = { ceiling, walling, flooring };
    Hero hero = {
        // Field of View.
        { { +1.0, -1.0 }, { +1.0, +1.0 } },
        // Where..
        { 3.5, 3.5 },
        // Velocity.
        { 0.0, 0.0 },
        // Speed.
        0.10,
        // Acceleration.
        0.01,
        // Theta (Radians).
        0.0
    };
    while(!finished())
    {
        hero = spin(move(hero, walling));
        render(hero, map, res, gpu);
    }
    release(gpu);
}
