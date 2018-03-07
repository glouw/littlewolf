#include <SDL2/SDL.h>

typedef struct
{
    float x;
    float y;
}
Point;

static Point turn(const Point a, const float t)
{
    const Point b = {
        a.x * cosf(t) - a.y * sinf(t),
        a.x * sinf(t) + a.y * cosf(t),
    };
    return b;
}

// Right angle rotation.
static Point rag(const Point a)
{
    const Point b = {
        -a.y,
        +a.x,
    };
    return b;
}

static Point sub(const Point a, const Point b)
{
    const Point c = {
        a.x - b.x,
        a.y - b.y,
    };
    return c;
}

static Point add(const Point a, const Point b)
{
    const Point c = {
        a.x + b.x,
        a.y + b.y,
    };
    return c;
}

static Point mul(const Point a, const float n)
{
    const Point b = {
        a.x * n,
        a.y * n,
    };
    return b;
}

static float mag(const Point a)
{
    return sqrtf(a.x * a.x + a.y * a.y);
}

static Point dvd(const Point a, const float n)
{
    const Point b = {
        a.x / n,
        a.y / n,
    };
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
    return (int) x - (x < (int) x);
}

// Fast ceil (math).
static int cl(const float x)
{
    return (int) x + (x > (int) x);
}

// Steps horizontally along a grid.
static Point sh(const Point a, const Point b)
{
    const float x = b.x > 0.0f ? fl(a.x + 1.0f) : cl(a.x - 1.0f);
    const float y = slope(b) * (x - a.x) + a.y;
    const Point c = { x, y };
    return c;
}

// Steps vertically along a grid.
static Point sv(const Point a, const Point b)
{
    const float y = b.y > 0.0f ? fl(a.y + 1.0f) : cl(a.y - 1.0f);
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
    Point where;
}
Hit;

// Floating point decimal.
static float dec(const float x)
{
    return x - (int) x;
}

static Hit cast(const Point where, const Point direction, const char** const walling)
{
    const Point ray = cmp(where, sh(where, direction), sv(where, direction));
    const Point delta = mul(direction, 0.01f);
    const Point dx = { delta.x, 0.0f };
    const Point dy = { 0.0f, delta.y };
    const Point test = add(ray, dec(ray.x) == 0.0f ? dx : dec(ray.y) == 0.0f ? dy : delta);
    const Hit hit = { tile(test, walling), ray };
    return hit.tile ? hit : cast(ray, direction, walling);
}

typedef struct
{
    Point a;
    Point b;
}
Line;

// Party casting (flooring and ceiling)
static float pcast(const float size, const int res, const int y)
{
    return size / (2 * y - res);
}

static Line rotate(const Line l, const float t)
{
    const Line line = {
        turn(l.a, t),
        turn(l.b, t),
    };
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
    int res;
}
Gpu;

static Gpu setup(const int res)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* const window = SDL_CreateWindow(
        "littlewolf", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, res, res, SDL_WINDOW_SHOWN);
    SDL_Renderer* const renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* const texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, res, res);
    const Gpu gpu = { window, renderer, texture, res };
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
    float size;
}
Wall;

static Wall project(const int res, const Line fov, const Point corrected)
{
    const float size = 0.5f * fov.a.x * res / corrected.x;
    const int top = (res - size) / 2.0f;
    const int bot = (res + size) / 2.0f;
    const Wall wall = { top < 0 ? 0 : top, bot > res ? res : bot, size };
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

static Hero spin(Hero hero, const uint8_t* key)
{
    if(key[SDL_SCANCODE_H]) hero.theta -= 0.1f;
    if(key[SDL_SCANCODE_L]) hero.theta += 0.1f;
    return hero;
}

static Hero move(Hero hero, const char** const walling, const uint8_t* key)
{
    const Point last = hero.where;
    // Accelerates if <WASD>.
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
    if(mag(hero.velocity) > hero.speed) hero.velocity = mul(unt(hero.velocity), hero.speed);
    // Moves
    hero.where = add(hero.where, hero.velocity);
    // Sets velocity to zero if there is a collision and puts hero back in bounds.
    const Point zero = { 0.0f, 0.0f };
    if(tile(hero.where, walling)) hero.velocity = zero, hero.where = last;
    return hero;
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

static void render(const Hero hero, const Map map, const Gpu gpu)
{
    const int t0 = SDL_GetTicks();
    const Line camera = rotate(hero.fov, hero.theta);
    const Display display = lock(gpu);
    for(int x = 0; x < gpu.res; x++)
    {
        // Casts a ray.
        const Point column = lerp(camera, x / (float) gpu.res);
        const Hit hit = cast(hero.where, column, map.walling);
        const Point ray = sub(hit.where, hero.where);
        const Point corrected = turn(ray, -hero.theta);
        const Wall wall = project(gpu.res, hero.fov, corrected);
        const Line trace = { hero.where, hit.where };
        // Renders ceiling.
        for(int y = 0; y < wall.top; y++)
            put(display, x, y, color(tile(lerp(trace, -pcast(wall.size, gpu.res, y + 1)), map.ceiling)));
        // Renders wall.
        for(int y = wall.top; y < wall.bot; y++)
            put(display, x, y, color(hit.tile));
        // Renders flooring.
        for(int y = wall.bot; y < gpu.res; y++)
            put(display, x, y, color(tile(lerp(trace, +pcast(wall.size, gpu.res, y + 0)), map.flooring)));
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
    if(event.type == SDL_QUIT
    || event.key.keysym.sym == SDLK_END
    || event.key.keysym.sym == SDLK_ESCAPE)
        return 1;
    return 0;
}

int main()
{
    const Gpu gpu = setup(500);
    const char* ceiling[] = {
        "1111111111111111             1111111111111111",
        "1222232232322321             1222232232322321",
        "122222221111232111111111111111222222211112321",
        "122221221232323232323232323232222212212323231",
        "122222221111232111111111111111222222211112321",
        "1222232232322321             1222232232322321",
        "1111111111111111             1111111111111111",
    };
    const char* walling[] = {
        "1111111111111111             1111111111111111",
        "1000000000000001             1000000000000001",
        "103330001111000111111111111111033300011110001",
        "103000000000000000000000000000030000030000001",
        "103330001111000111111111111111033300011110001",
        "1000000000000001             1000000000000001",
        "1111111111111111             1111111111111111",
    };
    const char* flooring[] = {
        "1111111111111111             1111111111111111",
        "1222232232322321             1222232232322321",
        "122222221111232111111111111111222222211112321",
        "122222221232323323232323232323222222212323231",
        "122222221111232111111111111111222222211112321",
        "1222232232322321             1222232232322321",
        "1111111111111111             1111111111111111",
    };
    const Map map = { ceiling, walling, flooring };
    Hero hero = {
        // Field of View.
        { { +1.0f, -1.0f }, { +1.0f, +1.0f } },
        // Where..
        { 3.5f, 3.5f },
        // Velocity.
        { 0.0f, 0.0f },
        // Speed.
        0.10f,
        // Acceleration.
        0.01f,
        // Theta (Radians).
        0.0f
    };
    while(!finished())
    {
        const uint8_t* key = SDL_GetKeyboardState(NULL);
        hero = spin(hero, key);
        hero = move(hero, walling, key);
        render(hero, map, gpu);
    }
    release(gpu);
}
