#include "storage_demo.h"

#include "render_utils.h"

namespace
{

static constexpr const char *kTitleFile = "storage-demo-title.txt";
static constexpr const char *kSaveDir = "showcase";
static constexpr const char *kSaveFile = "showcase/save-count.txt";
static constexpr const char *kDirectIOFile = "showcase/direct-io.txt";
static constexpr const char *kDirectIOPayload = "SDL Wii U native file IO OK\n";
static constexpr const char *kAsyncIOFile = "showcase/async-io.txt";
static constexpr const char *kAsyncIOPayload = "SDL Wii U async IO OK\n";

enum AsyncProbeToken
{
    ASYNC_PROBE_TITLE_READ = 1,
    ASYNC_PROBE_TITLE_CLOSE,
    ASYNC_PROBE_SAVE_WRITE,
    ASYNC_PROBE_SAVE_CLOSE,
    ASYNC_PROBE_VERIFY_READ,
    ASYNC_PROBE_VERIFY_CLOSE
};

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

void LogStorageSnapshot(const char *phase, const StorageDemoState *storage)
{
    if (storage && storage->space_remaining_known) {
        SDL_Log("[storage] %s title=%s user=%s count=%u free=%.1fMB raw=%s aio=%s",
                phase ? phase : "snapshot",
                storage->title_ready ? "ok" : "miss",
                storage->user_ready ? (storage->last_save_ok ? "flushed" : "open") : "fail",
                (unsigned int)storage->save_count,
                (double)storage->space_remaining / (1024.0 * 1024.0),
                storage->raw_io_status[0] ? storage->raw_io_status : "---",
                storage->async_status[0] ? storage->async_status : "---");
    } else {
        SDL_Log("[storage] %s title=%s user=%s count=%u free=--- raw=%s aio=%s",
                phase ? phase : "snapshot",
                (storage && storage->title_ready) ? "ok" : "miss",
                (storage && storage->user_ready) ? ((storage->last_save_ok) ? "flushed" : "open") : "fail",
                storage ? (unsigned int)storage->save_count : 0,
                (storage && storage->raw_io_status[0]) ? storage->raw_io_status : "---",
                (storage && storage->async_status[0]) ? storage->async_status : "---");
    }
}

void SetAsyncProbeStatus(StorageDemoState *storage, const char *status)
{
    if (!storage) {
        return;
    }

    CopyStorageDisplayString(storage->async_status, sizeof(storage->async_status), status);
}

void SetAsyncProbeError(StorageDemoState *storage, const char *status)
{
    if (!storage) {
        return;
    }

    CopyStorageErrorString(storage->async_error, sizeof(storage->async_error));
    if (status && *status) {
        SDL_snprintf(storage->async_status, sizeof(storage->async_status), "%s", status);
    } else {
        SDL_strlcpy(storage->async_status, "aio failure", sizeof(storage->async_status));
    }
}

void LogAsyncProbeOutcome(const char *label, const SDL_AsyncIOOutcome *outcome)
{
    SDL_Log("[storage][aio] %s result=%d bytes=%llu/%llu",
            label ? label : "task",
            outcome ? (int)outcome->result : -1,
            outcome ? (unsigned long long)outcome->bytes_transferred : 0,
            outcome ? (unsigned long long)outcome->bytes_requested : 0);
}

bool QueueAsyncClose(StorageDemoState *storage, SDL_AsyncIO *asyncio, bool flush, AsyncProbeToken token, const char *label)
{
    if (!storage || !storage->async_queue || !asyncio) {
        return false;
    }

    if (!SDL_CloseAsyncIO(asyncio, flush, storage->async_queue, reinterpret_cast<void *>(static_cast<uintptr_t>(token)))) {
        SetAsyncProbeError(storage, label);
        SDL_Log("[storage][aio] close queue failed (%s): %s", label ? label : "close", storage->async_error);
        return false;
    }

    return true;
}

bool StartAsyncTitleProbe(StorageDemoState *storage)
{
    const char *base_path = nullptr;
    char *title_path = nullptr;
    Uint8 *expected_bytes = nullptr;
    SDL_AsyncIO *asyncio = nullptr;
    size_t expected_size = 0;
    bool ok = false;

    if (!storage || !storage->async_queue) {
        return false;
    }

    base_path = SDL_GetBasePath();
    if (!base_path || SDL_asprintf(&title_path, "%s%s", base_path, kTitleFile) < 0) {
        SetAsyncProbeError(storage, "aio title path fail");
        goto done;
    }

    expected_bytes = static_cast<Uint8 *>(SDL_LoadFile(title_path, &expected_size));
    if (!expected_bytes || expected_size == 0) {
        SetAsyncProbeError(storage, "aio title prep fail");
        goto done;
    }

    storage->async_title_expected_size = SDL_min((Uint64)expected_size, (Uint64)(sizeof(storage->async_title_buffer) - 1));
    storage->async_title_hash = HashStorageBytes(expected_bytes, (size_t)storage->async_title_expected_size);
    SDL_zeroa(storage->async_title_buffer);

    asyncio = SDL_AsyncIOFromFile(title_path, "r");
    if (!asyncio) {
        SetAsyncProbeError(storage, "aio title open fail");
        goto done;
    }

    if (!SDL_ReadAsyncIO(asyncio,
                         storage->async_title_buffer,
                         0,
                         storage->async_title_expected_size,
                         storage->async_queue,
                         reinterpret_cast<void *>(static_cast<uintptr_t>(ASYNC_PROBE_TITLE_READ)))) {
        SetAsyncProbeError(storage, "aio title queue fail");
        (void)QueueAsyncClose(storage, asyncio, false, ASYNC_PROBE_TITLE_CLOSE, "aio title close fail");
        goto done;
    }

    if (!QueueAsyncClose(storage, asyncio, false, ASYNC_PROBE_TITLE_CLOSE, "aio title close fail")) {
        goto done;
    }

    SDL_Log("[storage][aio] title probe queued: %s (%llu bytes)",
            title_path,
            (unsigned long long)storage->async_title_expected_size);
    ok = true;

done:
    SDL_free(expected_bytes);
    SDL_free(title_path);
    return ok;
}

bool StartAsyncVerifyProbe(StorageDemoState *storage)
{
    char *pref_path = nullptr;
    char *save_path = nullptr;
    SDL_AsyncIO *asyncio = nullptr;
    const Uint64 payload_size = SDL_strlen(kAsyncIOPayload);
    bool ok = false;

    if (!storage || !storage->async_queue) {
        return false;
    }

    pref_path = SDL_GetPrefPath("wiiu", "sdl-demo");
    if (!pref_path || SDL_asprintf(&save_path, "%s%s", pref_path, kAsyncIOFile) < 0) {
        SetAsyncProbeError(storage, "aio verify path fail");
        goto done;
    }

    storage->async_verify_expected_size = payload_size;
    SDL_zeroa(storage->async_verify_buffer);
    asyncio = SDL_AsyncIOFromFile(save_path, "r");
    if (!asyncio) {
        SetAsyncProbeError(storage, "aio verify open fail");
        goto done;
    }

    if (!SDL_ReadAsyncIO(asyncio,
                         storage->async_verify_buffer,
                         0,
                         payload_size,
                         storage->async_queue,
                         reinterpret_cast<void *>(static_cast<uintptr_t>(ASYNC_PROBE_VERIFY_READ)))) {
        SetAsyncProbeError(storage, "aio verify queue fail");
        (void)QueueAsyncClose(storage, asyncio, false, ASYNC_PROBE_VERIFY_CLOSE, "aio verify close fail");
        goto done;
    }

    if (!QueueAsyncClose(storage, asyncio, false, ASYNC_PROBE_VERIFY_CLOSE, "aio verify close fail")) {
        goto done;
    }

    SDL_Log("[storage][aio] verify probe queued: %s (%llu bytes)",
            save_path,
            (unsigned long long)payload_size);
    ok = true;

done:
    SDL_free(save_path);
    SDL_free(pref_path);
    return ok;
}

bool StartAsyncSaveProbe(StorageDemoState *storage)
{
    char *pref_path = nullptr;
    char *save_dir = nullptr;
    char *save_path = nullptr;
    SDL_AsyncIO *asyncio = nullptr;
    const Uint64 payload_size = SDL_strlen(kAsyncIOPayload);
    bool ok = false;

    if (!storage || !storage->async_queue) {
        return false;
    }

    pref_path = SDL_GetPrefPath("wiiu", "sdl-demo");
    if (!pref_path) {
        SetAsyncProbeError(storage, "aio save pref fail");
        goto done;
    }

    if (SDL_asprintf(&save_dir, "%s%s/", pref_path, kSaveDir) < 0 || !SDL_CreateDirectory(save_dir)) {
        SetAsyncProbeError(storage, "aio save dir fail");
        goto done;
    }

    if (SDL_asprintf(&save_path, "%s%s", pref_path, kAsyncIOFile) < 0) {
        SetAsyncProbeError(storage, "aio save path fail");
        goto done;
    }

    asyncio = SDL_AsyncIOFromFile(save_path, "w+");
    if (!asyncio) {
        SetAsyncProbeError(storage, "aio save open fail");
        goto done;
    }

    if (!SDL_WriteAsyncIO(asyncio,
                          const_cast<char *>(kAsyncIOPayload),
                          0,
                          payload_size,
                          storage->async_queue,
                          reinterpret_cast<void *>(static_cast<uintptr_t>(ASYNC_PROBE_SAVE_WRITE)))) {
        SetAsyncProbeError(storage, "aio save queue fail");
        (void)QueueAsyncClose(storage, asyncio, false, ASYNC_PROBE_SAVE_CLOSE, "aio save close fail");
        goto done;
    }

    if (!QueueAsyncClose(storage, asyncio, true, ASYNC_PROBE_SAVE_CLOSE, "aio save close fail")) {
        goto done;
    }

    SDL_Log("[storage][aio] save probe queued: %s (%llu bytes)",
            save_path,
            (unsigned long long)payload_size);
    ok = true;

done:
    SDL_free(save_path);
    SDL_free(save_dir);
    SDL_free(pref_path);
    return ok;
}

void BeginAsyncProbe(StorageDemoState *storage)
{
    bool title_ok;
    bool save_ok;

    if (!storage) {
        return;
    }

    storage->async_status[0] = '\0';
    storage->async_error[0] = '\0';
    storage->async_started = false;
    storage->async_complete = false;
    storage->async_title_ok = false;
    storage->async_title_close_ok = false;
    storage->async_write_ok = false;
    storage->async_write_close_ok = false;
    storage->async_verify_ok = false;
    storage->async_verify_close_ok = false;
    storage->async_title_bytes = 0;
    storage->async_write_bytes = 0;
    storage->async_verify_bytes = 0;
    storage->async_title_expected_size = 0;
    storage->async_verify_expected_size = 0;

    storage->async_queue = SDL_CreateAsyncIOQueue();
    if (!storage->async_queue) {
        SetAsyncProbeError(storage, "aio queue fail");
        SDL_Log("[storage][aio] queue creation failed: %s", storage->async_error);
        return;
    }

    title_ok = StartAsyncTitleProbe(storage);
    save_ok = StartAsyncSaveProbe(storage);
    storage->async_started = title_ok || save_ok;

    if (!storage->async_started) {
        if (!storage->async_status[0]) {
            SetAsyncProbeStatus(storage, "aio unavailable");
        }
        storage->async_complete = true;
        return;
    }

    SetAsyncProbeStatus(storage, "aio title/save pending");
}

bool QueryStorageSpaceRemaining(SDL_Storage *storage, Uint64 *remaining)
{
    Uint64 value;
    const char *error;

    if (!storage || !remaining) {
        return false;
    }

    SDL_ClearError();
    value = SDL_GetStorageSpaceRemaining(storage);
    error = SDL_GetError();
    if (error && *error) {
        SDL_ClearError();
        *remaining = 0;
        return false;
    }

    *remaining = value;
    return true;
}

struct EnumerateCount
{
    Uint32 count;
};

SDL_EnumerationResult SDLCALL CountEntriesCallback(void *userdata, const char *dirname, const char *fname)
{
    EnumerateCount *counter = static_cast<EnumerateCount *>(userdata);
    (void)dirname;
    (void)fname;

    if (counter) {
        ++counter->count;
    }
    return SDL_ENUM_CONTINUE;
}

void RefreshStoragePaths(StorageDemoState *storage)
{
    const char *base_path;
    char *current_dir;
    char *pref_path;

    if (!storage) {
        return;
    }

    storage->base_path[0] = '\0';
    storage->current_dir[0] = '\0';
    storage->pref_path[0] = '\0';
    storage->pref_error[0] = '\0';

    SDL_Log("[storage] paths base begin");
    base_path = SDL_GetBasePath();
    if (base_path) {
        CopyStorageDisplayString(storage->base_path, sizeof(storage->base_path), base_path);
    }
    SDL_Log("[storage] paths base done: %s", storage->base_path[0] ? storage->base_path : "(null)");

    SDL_Log("[storage] paths cwd begin");
    current_dir = SDL_GetCurrentDirectory();
    if (current_dir) {
        CopyStorageDisplayString(storage->current_dir, sizeof(storage->current_dir), current_dir);
        SDL_free(current_dir);
    }
    SDL_Log("[storage] paths cwd done: %s", storage->current_dir[0] ? storage->current_dir : "(null)");

    SDL_Log("[storage] paths pref begin");
    pref_path = SDL_GetPrefPath("wiiu", "sdl-demo");
    if (pref_path) {
        CopyStorageDisplayString(storage->pref_path, sizeof(storage->pref_path), pref_path);
        SDL_free(pref_path);
        SDL_Log("[storage] paths pref done: %s", storage->pref_path);
    } else {
        CopyStorageErrorString(storage->pref_error, sizeof(storage->pref_error));
        SDL_Log("[storage] paths pref failed: %s", storage->pref_error);
    }
}

void RefreshRawFilesystemProbe(StorageDemoState *storage)
{
    EnumerateCount base_counter = {};
    EnumerateCount pref_counter = {};
    char *pref_path = nullptr;
    char *title_path = nullptr;
    char *save_dir = nullptr;
    char *save_path = nullptr;

    if (!storage) {
        return;
    }

    storage->raw_base_ok = false;
    storage->raw_pref_ok = false;
    storage->raw_title_io_ok = false;
    storage->raw_save_io_ok = false;
    storage->raw_base_entries = 0;
    storage->raw_pref_entries = 0;
    storage->raw_io_status[0] = '\0';

    RefreshStoragePaths(storage);

    if (storage->base_path[0]) {
        if (SDL_EnumerateDirectory(storage->base_path, CountEntriesCallback, &base_counter)) {
            storage->raw_base_ok = true;
            storage->raw_base_entries = base_counter.count;
        } else {
            CopyStorageErrorString(storage->raw_io_status, sizeof(storage->raw_io_status));
            SDL_ClearError();
        }

        if (SDL_asprintf(&title_path, "%s%s", storage->base_path, kTitleFile) >= 0) {
            size_t title_size = 0;
            void *title_bytes = SDL_LoadFile(title_path, &title_size);
            if (title_bytes && title_size > 0) {
                storage->raw_title_io_ok = true;
            } else if (!storage->raw_io_status[0]) {
                CopyStorageErrorString(storage->raw_io_status, sizeof(storage->raw_io_status));
            }
            SDL_free(title_bytes);
        }
    }

    pref_path = SDL_GetPrefPath("wiiu", "sdl-demo");
    if (pref_path) {
        if (SDL_EnumerateDirectory(pref_path, CountEntriesCallback, &pref_counter)) {
            storage->raw_pref_ok = true;
            storage->raw_pref_entries = pref_counter.count;
        } else if (!storage->raw_io_status[0]) {
            CopyStorageErrorString(storage->raw_io_status, sizeof(storage->raw_io_status));
            SDL_ClearError();
        }

        if (SDL_asprintf(&save_dir, "%s%s/", pref_path, kSaveDir) >= 0 &&
            SDL_CreateDirectory(save_dir) &&
            SDL_asprintf(&save_path, "%s%s", pref_path, kDirectIOFile) >= 0 &&
            SDL_SaveFile(save_path, kDirectIOPayload, SDL_strlen(kDirectIOPayload))) {
            size_t loaded_size = 0;
            char *loaded = static_cast<char *>(SDL_LoadFile(save_path, &loaded_size));
            if (loaded && loaded_size == SDL_strlen(kDirectIOPayload) &&
                SDL_memcmp(loaded, kDirectIOPayload, loaded_size) == 0) {
                storage->raw_save_io_ok = true;
            } else if (!storage->raw_io_status[0]) {
                CopyStorageErrorString(storage->raw_io_status, sizeof(storage->raw_io_status));
            }
            SDL_free(loaded);
        } else if (!storage->raw_io_status[0]) {
            CopyStorageErrorString(storage->raw_io_status, sizeof(storage->raw_io_status));
        }
    }

    if (storage->raw_base_ok && storage->raw_pref_ok && storage->raw_title_io_ok && storage->raw_save_io_ok) {
        SDL_snprintf(storage->raw_io_status, sizeof(storage->raw_io_status),
                     "cwd ok raw b%u p%u io ok",
                     (unsigned int)storage->raw_base_entries,
                     (unsigned int)storage->raw_pref_entries);
    } else if (!storage->raw_io_status[0]) {
        SDL_strlcpy(storage->raw_io_status, "raw probe incomplete", sizeof(storage->raw_io_status));
    }

    SDL_free(pref_path);
    SDL_free(title_path);
    SDL_free(save_dir);
    SDL_free(save_path);
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

void UpdateStorageDemo(AppState *state)
{
    StorageDemoState *storage;
    SDL_AsyncIOOutcome outcome;

    if (!state) {
        return;
    }

    storage = &state->storage;
    if (!storage->async_queue || storage->async_complete) {
        return;
    }

    while (SDL_GetAsyncIOResult(storage->async_queue, &outcome)) {
        const AsyncProbeToken token = static_cast<AsyncProbeToken>(reinterpret_cast<uintptr_t>(outcome.userdata));

        switch (token) {
        case ASYNC_PROBE_TITLE_READ:
            LogAsyncProbeOutcome("title-read", &outcome);
            storage->async_title_bytes = outcome.bytes_transferred;
            if (outcome.result == SDL_ASYNCIO_COMPLETE &&
                outcome.bytes_transferred == storage->async_title_expected_size &&
                HashStorageBytes(reinterpret_cast<const Uint8 *>(storage->async_title_buffer), (size_t)outcome.bytes_transferred) == storage->async_title_hash) {
                storage->async_title_ok = true;
            } else {
                SetAsyncProbeStatus(storage, "aio title mismatch");
            }
            break;

        case ASYNC_PROBE_TITLE_CLOSE:
            LogAsyncProbeOutcome("title-close", &outcome);
            storage->async_title_close_ok = (outcome.result == SDL_ASYNCIO_COMPLETE);
            break;

        case ASYNC_PROBE_SAVE_WRITE:
            LogAsyncProbeOutcome("save-write", &outcome);
            storage->async_write_bytes = outcome.bytes_transferred;
            if (outcome.result == SDL_ASYNCIO_COMPLETE &&
                outcome.bytes_transferred == SDL_strlen(kAsyncIOPayload)) {
                storage->async_write_ok = true;
            } else {
                SetAsyncProbeStatus(storage, "aio write fail");
            }
            break;

        case ASYNC_PROBE_SAVE_CLOSE:
            LogAsyncProbeOutcome("save-close", &outcome);
            storage->async_write_close_ok = (outcome.result == SDL_ASYNCIO_COMPLETE);
            if (storage->async_write_ok && storage->async_write_close_ok) {
                (void)StartAsyncVerifyProbe(storage);
            }
            break;

        case ASYNC_PROBE_VERIFY_READ:
            LogAsyncProbeOutcome("verify-read", &outcome);
            storage->async_verify_bytes = outcome.bytes_transferred;
            if (outcome.result == SDL_ASYNCIO_COMPLETE &&
                outcome.bytes_transferred == storage->async_verify_expected_size &&
                SDL_memcmp(storage->async_verify_buffer, kAsyncIOPayload, (size_t)outcome.bytes_transferred) == 0) {
                storage->async_verify_ok = true;
            } else {
                SetAsyncProbeStatus(storage, "aio verify fail");
            }
            break;

        case ASYNC_PROBE_VERIFY_CLOSE:
            LogAsyncProbeOutcome("verify-close", &outcome);
            storage->async_verify_close_ok = (outcome.result == SDL_ASYNCIO_COMPLETE);
            break;

        default:
            SDL_Log("[storage][aio] unexpected token=%u", (unsigned int)token);
            break;
        }
    }

    if (!storage->async_complete &&
        storage->async_title_ok &&
        storage->async_title_close_ok &&
        storage->async_write_ok &&
        storage->async_write_close_ok &&
        storage->async_verify_ok &&
        storage->async_verify_close_ok) {
        storage->async_complete = true;
        SDL_snprintf(storage->async_status, sizeof(storage->async_status),
                     "aio ok t%llu w%llu v%llu",
                     (unsigned long long)storage->async_title_bytes,
                     (unsigned long long)storage->async_write_bytes,
                     (unsigned long long)storage->async_verify_bytes);
        storage->async_error[0] = '\0';
        SDL_Log("[storage][aio] probe complete: %s", storage->async_status);
    } else if (!storage->async_complete && !storage->async_status[0]) {
        SetAsyncProbeStatus(storage, "aio pending");
    }
}

void ShutdownStorageDemo(AppState *state)
{
    if (!state) {
        return;
    }

    if (state->storage.async_queue) {
        SDL_DestroyAsyncIOQueue(state->storage.async_queue);
        state->storage.async_queue = nullptr;
    }
}

bool PersistStorageCounter(StorageDemoState *storage, Uint32 new_count)
{
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
        SDL_Log("[storage] persist-open failed: %s", storage->user_error);
        return false;
    }

    if (!SDL_StorageReady(user_storage)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_Log("[storage] persist-not-ready: %s", storage->user_error);
        SDL_CloseStorage(user_storage);
        return false;
    }

    if (!SDL_CreateStorageDirectory(user_storage, kSaveDir)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_Log("[storage] persist-mkdir failed: %s", storage->user_error);
        SDL_CloseStorage(user_storage);
        return false;
    }

    length = SDL_snprintf(buffer, sizeof(buffer), "%u", (unsigned int)new_count);
    if (length <= 0 || length >= (int)sizeof(buffer) ||
        !SDL_WriteStorageFile(user_storage, kSaveFile, buffer, (Uint64)length)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_Log("[storage] persist-write failed: %s", storage->user_error);
        SDL_CloseStorage(user_storage);
        return false;
    }

    storage->space_remaining_known = QueryStorageSpaceRemaining(user_storage, &storage->space_remaining);
    close_result = SDL_CloseStorage(user_storage);
    if (!close_result) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_Log("[storage] persist-close failed: %s", storage->user_error);
        return false;
    }

    storage->user_ready = true;
    storage->last_save_ok = true;
    storage->save_count = new_count;
    storage->last_write_ns = SDL_GetTicksNS();
    storage->user_error[0] = '\0';
    RefreshRawFilesystemProbe(storage);
    LogStorageSnapshot("persist", storage);
    return true;
}

bool InitStorageDemo(AppState *state)
{
    StorageDemoState *storage;
    SDL_Storage *title_storage;
    SDL_Storage *user_storage;
    Uint32 initial_count = 0;

    if (!state) {
        return false;
    }

    storage = &state->storage;
    ResetStorageDemoState(storage);
    SDL_Log("[storage] init paths begin");
    RefreshStoragePaths(storage);
    SDL_Log("[storage] init paths done");

    SDL_Log("[storage] init title-open begin");
    title_storage = SDL_OpenTitleStorage(nullptr, 0);
    if (!title_storage) {
        CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
    } else if (!SDL_StorageReady(title_storage)) {
        CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
        SDL_CloseStorage(title_storage);
        title_storage = nullptr;
    } else {
        Uint64 title_size = 0;

        if (SDL_GetStorageFileSize(title_storage, kTitleFile, &title_size) &&
            title_size > 0 && title_size < 4096) {
            Uint8 *bytes = static_cast<Uint8 *>(SDL_malloc((size_t)title_size + 1));

            if (bytes && SDL_ReadStorageFile(title_storage, kTitleFile, bytes, title_size)) {
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
    SDL_Log("[storage] init title-open done");

    SDL_Log("[storage] init user-open begin");
    user_storage = SDL_OpenUserStorage("wiiu", "sdl-demo", 0);
    if (!user_storage) {
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_Log("[storage] init user-open failed: %s", storage->user_error);
        return true;
    }
    SDL_Log("[storage] init user-open done");

    if (!SDL_StorageReady(user_storage)) {
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_Log("[storage] init user-not-ready: %s", storage->user_error);
        SDL_CloseStorage(user_storage);
        return true;
    }
    SDL_Log("[storage] init user-ready done");

    storage->user_ready = true;
    SDL_Log("[storage] init user-space begin");
    storage->space_remaining_known = QueryStorageSpaceRemaining(user_storage, &storage->space_remaining);
    SDL_Log("[storage] init user-space done: %s",
            storage->space_remaining_known ? "available" : "unsupported");
    SDL_Log("[storage] init read-counter begin");
    if (!ReadStoredCounter(user_storage, kSaveFile, &initial_count)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_Log("[storage] init read-counter failed: %s", storage->user_error);
    }
    SDL_Log("[storage] init read-counter done: %u", (unsigned int)initial_count);

    SDL_Log("[storage] init user-close begin");
    if (!SDL_CloseStorage(user_storage) && storage->user_ready) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_Log("[storage] init user-close failed: %s", storage->user_error);
    }
    SDL_Log("[storage] init user-close done");

    if (storage->user_ready) {
        SDL_Log("[storage] init persist begin");
        (void)PersistStorageCounter(storage, initial_count + 1);
        SDL_Log("[storage] init persist done");
    } else {
        SDL_Log("[storage] init raw-probe begin");
        RefreshRawFilesystemProbe(storage);
        SDL_Log("[storage] init raw-probe done");
    }

    SDL_Log("[storage] init aio begin");
    BeginAsyncProbe(storage);
    SDL_Log("[storage] init aio done: %s", storage->async_status[0] ? storage->async_status : "pending");

    LogStorageSnapshot("init", storage);

    return true;
}

bool RenderStorageWidget(AppState *state, const SDL_FRect &rect)
{
    StorageDemoState *storage = &state->storage;
    const float padding = 10.0f;
    const float line_height = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2.0f;
    const float text_x = rect.x + padding;
    const float text_y = rect.y + 10.0f;
    const float pulse_amount = (storage->last_write_ns > 0)
                                   ? SDL_clamp(1.0f - (float)((SDL_GetTicksNS() - storage->last_write_ns) / (double)(SDL_NS_PER_SECOND * 2)), 0.0f, 1.0f)
                                   : 0.0f;
    const Uint8 accent_r = storage->user_ready ? 120 : 255;
    const Uint8 accent_g = storage->user_ready ? 220 : 120;
    const Uint8 accent_b = storage->title_ready ? 255 : 140;
    char preview_line[40];
    char status_value[48];
    char status_line[54];
    char space_line[40];
    char cwd_value[36];
    char pref_value[36];
    char cwd_line[40];
    char pref_line[52];
    char async_value[44];
    char async_line[50];

    if (!storage->render_logged) {
        SDL_Log("[storage] render-window rect=(%.0f,%.0f %.0fx%.0f)",
                rect.x, rect.y, rect.w, rect.h);
        storage->render_logged = true;
    }

    CopyStorageDisplayString(preview_line, sizeof(preview_line),
                             storage->title_ready ? storage->title_preview : storage->title_error);
    CopyStorageDisplayString(status_value, sizeof(status_value),
                             storage->user_ready
                                 ? (storage->raw_io_status[0] ? storage->raw_io_status : "raw probe pending")
                                 : (storage->user_error[0] ? storage->user_error
                                                           : (storage->pref_error[0] ? storage->pref_error : "user storage unavailable")));
    SDL_snprintf(status_line, sizeof(status_line), "%s %s",
                 storage->user_ready ? "raw" : "save!",
                 status_value);
    if (storage->user_ready) {
        SDL_snprintf(space_line, sizeof(space_line), "free %.1fMB  P:+1  R:reset",
                     (double)storage->space_remaining / (1024.0 * 1024.0));
    } else {
        SDL_strlcpy(space_line, "free ---   P:+1  R:reset", sizeof(space_line));
    }
    if (storage->user_ready && !storage->space_remaining_known) {
        SDL_strlcpy(space_line, "free ---   P:+1  R:reset", sizeof(space_line));
    }
    CopyStorageDisplayString(cwd_value, sizeof(cwd_value), storage->current_dir[0] ? storage->current_dir : "---");
    SDL_snprintf(cwd_line, sizeof(cwd_line), "cwd %s", cwd_value);
    if (storage->pref_path[0]) {
        CopyStorageDisplayString(pref_value, sizeof(pref_value), storage->pref_path);
        SDL_snprintf(pref_line, sizeof(pref_line), "pref %s", pref_value);
    } else {
        CopyStorageDisplayString(pref_value, sizeof(pref_value),
                                 storage->pref_error[0] ? storage->pref_error : "unavailable");
        SDL_snprintf(pref_line, sizeof(pref_line), "pref! %s", pref_value);
    }
    if (storage->async_error[0]) {
        CopyStorageDisplayString(async_value, sizeof(async_value), storage->async_error);
        SDL_snprintf(async_line, sizeof(async_line), "aio! %s", async_value);
    } else {
        CopyStorageDisplayString(async_value, sizeof(async_value),
                                 storage->async_status[0] ? storage->async_status : "aio unavailable");
        SDL_snprintf(async_line, sizeof(async_line), "aio %s", async_value);
    }

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
        const bool lit = ((storage->title_signature >> (i * 4)) & 0xF) != 0;
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
        !SDL_RenderDebugText(state->renderer, text_x, text_y, "storage window") ||
        !SDL_RenderDebugTextFormat(state->renderer, text_x, text_y + line_height,
                                   "ttl %s %lluB save %s #%u",
                                   storage->title_ready ? "ok" : "miss",
                                   (unsigned long long)storage->title_bytes,
                                   storage->user_ready ? (storage->last_save_ok ? "flush" : "open") : "fail",
                                   (unsigned int)storage->save_count) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 2.0f,
                             preview_line) ||
         !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 3.0f, space_line) ||
         !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 4.0f, status_line) ||
         !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 5.0f, cwd_line) ||
         !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 6.0f, pref_line) ||
         !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 7.0f, async_line)) {
        return false;
    }

    return true;
}
