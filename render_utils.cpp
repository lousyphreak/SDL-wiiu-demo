#include "render_utils.h"

bool SetColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

bool FillCircle(SDL_Renderer *renderer, float center_x, float center_y, float radius)
{
    const int limit = (int)SDL_ceilf(radius);

    for (int y = -limit; y <= limit; ++y) {
        const float dy = (float)y;
        const float dx = SDL_sqrtf(SDL_max(0.0f, (radius * radius) - (dy * dy)));
        const SDL_FRect span = {
            center_x - dx,
            center_y + dy,
            (dx * 2.0f) + 1.0f,
            1.0f
        };

        if (!SDL_RenderFillRect(renderer, &span)) {
            return false;
        }
    }

    return true;
}
