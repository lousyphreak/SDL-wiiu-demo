#include "storage_demo.h"

#include "render_utils.h"

namespace
{

void CopyStorageDisplayString(char *destination, size_t destination_size, const char *source)
{
    if (!destination || destination_size == 0) {
        return;
    }

    destination[0] = '\0';
    if (!source || !*source) {
        return;
    }

    if (SDL_strlen(source) < destination_size) {
        SDL_strlcpy(destination, source, destination_size);
        return;
    }

    if (destination_size <= 4) {
        SDL_strlcpy(destination, source, destination_size);
        return;
    }

    SDL_memcpy(destination, source, destination_size - 4);
    destination[destination_size - 4] = '.';
    destination[destination_size - 3] = '.';
    destination[destination_size - 2] = '.';
    destination[destination_size - 1] = '\0';
}

void CopyStorageErrorString(char *destination, size_t destination_size)
{
    const char *error = SDL_GetError();
    CopyStorageDisplayString(destination, destination_size, (error && *error) ? error : "unknown error");
}

void SanitizeStoragePreview(char *text)
{
    if (!text) {
        return;
    }

    for (char *cursor = text; *cursor; ++cursor) {
        if (*cursor == '\r' || *cursor == '\n' || *cursor == '\t') {
            *cursor = ' ';
        }
    }
}

Uint32 HashStorageBytes(const Uint8 *bytes, size_t length)
{
    Uint32 hash = 2166136261u;

    for (size_t i = 0; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

void RefreshStoragePaths(StorageDemoState *storage)
{
    const char *base_path;
    char *pref_path;

    if (!storage) {
        return;
    }

    base_path = SDL_GetBasePath();
    if (base_path) {
        CopyStorageDisplayString(storage->base_path, sizeof(storage->base_path), base_path);
    }

    pref_path = SDL_GetPrefPath("wiiu", "sdl-demo");
    if (pref_path) {
        CopyStorageDisplayString(storage->pref_path, sizeof(storage->pref_path), pref_path);
        SDL_free(pref_path);
    }
}

bool ReadStoredCounter(SDL_Storage *storage, const char *path, Uint32 *count)
{
    SDL_PathInfo info;
    char buffer[32];

    if (!storage || !path || !count) {
        return false;
    }

    *count = 0;
    if (!SDL_GetStoragePathInfo(storage, path, &info)) {
        SDL_ClearError();
        return true;
    }

    if (info.type != SDL_PATHTYPE_FILE || info.size == 0 || info.size >= sizeof(buffer)) {
        return true;
    }

    if (!SDL_ReadStorageFile(storage, path, buffer, info.size)) {
        return false;
    }

    buffer[(size_t)info.size] = '\0';
    *count = (Uint32)SDL_strtoul(buffer, nullptr, 10);
    return true;
}

} // namespace

void ResetStorageDemoState(StorageDemoState *storage)
{
    if (!storage) {
        return;
    }

    SDL_zero(*storage);
}

bool PersistStorageCounter(StorageDemoState *storage, Uint32 new_count)
{
    static constexpr const char *save_dir = "showcase";
    static constexpr const char *save_file = "showcase/save-count.txt";
    SDL_Storage *user_storage;
    char buffer[32];
    int length;
    bool close_result;

    if (!storage) {
        return false;
    }

    storage->last_save_ok = false;
    user_storage = SDL_OpenUserStorage("wiiu", "sdl-demo", 0);
    if (!user_storage) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        return false;
    }

    if (!SDL_StorageReady(user_storage)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_CloseStorage(user_storage);
        return false;
    }

    if (!SDL_CreateStorageDirectory(user_storage, save_dir)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_CloseStorage(user_storage);
        return false;
    }

    length = SDL_snprintf(buffer, sizeof(buffer), "%u", (unsigned int)new_count);
    if (length <= 0 || length >= (int)sizeof(buffer) ||
        !SDL_WriteStorageFile(user_storage, save_file, buffer, (Uint64)length)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_CloseStorage(user_storage);
        return false;
    }

    storage->space_remaining = SDL_GetStorageSpaceRemaining(user_storage);
    close_result = SDL_CloseStorage(user_storage);
    if (!close_result) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        return false;
    }

    storage->user_ready = true;
    storage->last_save_ok = true;
    storage->save_count = new_count;
    storage->last_write_ns = SDL_GetTicksNS();
    storage->user_error[0] = '\0';
    RefreshStoragePaths(storage);
    return true;
}

bool InitStorageDemo(AppState *state)
{
    static constexpr const char *title_file = "storage-demo-title.txt";
    static constexpr const char *save_file = "showcase/save-count.txt";
    StorageDemoState *storage;
    SDL_Storage *title_storage;
    SDL_Storage *user_storage;
    Uint32 initial_count = 0;

    if (!state) {
        return false;
    }

    storage = &state->storage;
    ResetStorageDemoState(storage);
    RefreshStoragePaths(storage);

    title_storage = SDL_OpenTitleStorage(nullptr, 0);
    if (!title_storage) {
        CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
    } else if (!SDL_StorageReady(title_storage)) {
        CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
        SDL_CloseStorage(title_storage);
        title_storage = nullptr;
    } else {
        Uint64 title_size = 0;

        if (SDL_GetStorageFileSize(title_storage, title_file, &title_size) &&
            title_size > 0 && title_size < 4096) {
            Uint8 *bytes = static_cast<Uint8 *>(SDL_malloc((size_t)title_size + 1));

            if (bytes && SDL_ReadStorageFile(title_storage, title_file, bytes, title_size)) {
                bytes[(size_t)title_size] = '\0';
                storage->title_ready = true;
                storage->title_bytes = title_size;
                storage->title_signature = HashStorageBytes(bytes, (size_t)title_size);
                CopyStorageDisplayString(storage->title_preview, sizeof(storage->title_preview), (const char *)bytes);
                SanitizeStoragePreview(storage->title_preview);
            } else {
                CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
            }
            SDL_free(bytes);
        } else {
            CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
        }

        if (!SDL_CloseStorage(title_storage) && storage->title_ready) {
            storage->title_ready = false;
            CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
        }
    }

    user_storage = SDL_OpenUserStorage("wiiu", "sdl-demo", 0);
    if (!user_storage) {
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        return true;
    }

    if (!SDL_StorageReady(user_storage)) {
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_CloseStorage(user_storage);
        return true;
    }

    storage->user_ready = true;
    storage->space_remaining = SDL_GetStorageSpaceRemaining(user_storage);
    if (!ReadStoredCounter(user_storage, save_file, &initial_count)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
    }

    if (!SDL_CloseStorage(user_storage) && storage->user_ready) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
    }

    if (storage->user_ready) {
        (void)PersistStorageCounter(storage, initial_count + 1);
    }

    return true;
}

bool RenderStorageWidget(AppState *state, const SDL_FRect &rect)
{
    const StorageDemoState &storage = state->storage;
    const float padding = 10.0f;
    const float line_height = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2.0f;
    const float text_x = rect.x + padding;
    const float text_y = rect.y + 10.0f;
    const float pulse_amount = (storage.last_write_ns > 0)
                                   ? SDL_clamp(1.0f - (float)((SDL_GetTicksNS() - storage.last_write_ns) / (double)(SDL_NS_PER_SECOND * 2)), 0.0f, 1.0f)
                                   : 0.0f;
    const Uint8 accent_r = storage.user_ready ? 120 : 255;
    const Uint8 accent_g = storage.user_ready ? 220 : 120;
    const Uint8 accent_b = storage.title_ready ? 255 : 140;

    if (!SetColor(state->renderer, 8, 12, 28, 228) ||
        !SDL_RenderFillRect(state->renderer, &rect) ||
        !SetColor(state->renderer, accent_r, accent_g, accent_b, 255) ||
        !SDL_RenderRect(state->renderer, &rect)) {
        return false;
    }

    {
        const SDL_FRect accent_strip = { rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, 4.0f };
        if (!SetColor(state->renderer, accent_r, accent_g, accent_b, 255) ||
            !SDL_RenderFillRect(state->renderer, &accent_strip)) {
            return false;
        }
    }

    for (int i = 0; i < 8; ++i) {
        const bool lit = ((storage.title_signature >> (i * 4)) & 0xF) != 0;
        const SDL_FRect pip = {
            rect.x + rect.w - 86.0f + i * 9.0f,
            rect.y + 10.0f,
            6.0f,
            6.0f
        };

        if (!SetColor(state->renderer,
                      lit ? (Uint8)(80 + i * 18) : 28,
                      lit ? (Uint8)(120 + i * 12) : 32,
                      lit ? 255 : 44,
                      255) ||
            !SDL_RenderFillRect(state->renderer, &pip)) {
            return false;
        }
    }

    if (pulse_amount > 0.0f) {
        const float radius = 8.0f + pulse_amount * 12.0f;
        if (!SetColor(state->renderer,
                      (Uint8)(90 + pulse_amount * 140.0f),
                      (Uint8)(90 + pulse_amount * 100.0f),
                      255,
                      255) ||
            !FillCircle(state->renderer, rect.x + rect.w - 24.0f, rect.y + rect.h - 24.0f, radius)) {
            return false;
        }
    }

    if (!SetColor(state->renderer, 230, 240, 255, 255) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y, "storage") ||
        !SDL_RenderDebugTextFormat(state->renderer, text_x, text_y + line_height,
                                   "title %s  %llu bytes",
                                   storage.title_ready ? "ok" : "miss",
                                   (unsigned long long)storage.title_bytes) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 2.0f,
                             storage.title_ready ? storage.title_preview : storage.title_error) ||
        !SDL_RenderDebugTextFormat(state->renderer, text_x, text_y + line_height * 3.0f,
                                   "save  %s  count %u  free %.1f MB",
                                   storage.user_ready ? (storage.last_save_ok ? "flushed" : "open") : "fail",
                                   (unsigned int)storage.save_count,
                                   (double)storage.space_remaining / (1024.0 * 1024.0)) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 4.0f,
                             storage.user_ready ? storage.pref_path : storage.user_error) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 5.0f,
                             "NORTH/P +1   WEST/R reset   title via SDL_OpenTitleStorage")) {
        return false;
    }

    return true;
}
