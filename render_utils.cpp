#include "render_utils.h"

#include <vector>

bool SetColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

bool FillCircle(SDL_Renderer *renderer, float center_x, float center_y, float radius)
{
    const int limit = (int)SDL_ceilf(radius);
    std::vector<SDL_FRect> spans;

    spans.reserve((size_t)(limit * 2 + 1));

    for (int y = -limit; y <= limit; ++y) {
        const float dy = (float)y;
        const float dx = SDL_sqrtf(SDL_max(0.0f, (radius * radius) - (dy * dy)));
        spans.push_back({
            center_x - dx,
            center_y + dy,
            (dx * 2.0f) + 1.0f,
            1.0f
        });
    }

    return SDL_RenderFillRects(renderer, spans.data(), (int)spans.size());
}
