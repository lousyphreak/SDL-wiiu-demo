#include "camera_demo.h"

#include <cstdarg>

#include "render_utils.h"

namespace
{

constexpr int CAMERA_STATUS_TEXT_CAPACITY = 96;

void SetCameraStatus(CameraDemoState *camera, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    SDL_vsnprintf(camera->status_text, CAMERA_STATUS_TEXT_CAPACITY, fmt, ap);
    va_end(ap);
}

void DiscoverCameraDevice(CameraDemoState *camera)
{
    int count = 0;
    SDL_CameraID *cameras = SDL_GetCameras(&count);
    SDL_CameraID selected = 0;

    camera->available = false;

    if (cameras && count > 0) {
        camera->available = true;
        selected = cameras[0];

        if (camera->device_id != 0) {
            for (int i = 0; i < count; ++i) {
                if (cameras[i] == camera->device_id) {
                    selected = camera->device_id;
                    break;
                }
            }
        }
    }

    camera->device_id = selected;
    SDL_free(cameras);
}

void DestroyPreviewTexture(CameraDemoState *camera)
{
    if (camera->preview_texture) {
        SDL_DestroyTexture(camera->preview_texture);
        camera->preview_texture = nullptr;
    }
}

void CloseCameraStream(CameraDemoState *camera)
{
    if (camera->camera) {
        SDL_CloseCamera(camera->camera);
        camera->camera = nullptr;
    }

    DestroyPreviewTexture(camera);
    SDL_zero(camera->spec);
    camera->permission = SDL_CAMERA_PERMISSION_STATE_PENDING;
    camera->frame_ready = false;
}

bool EnsurePreviewTexture(AppState *state)
{
    CameraDemoState *camera = &state->camera_demo;
    SDL_CameraSpec spec;

    if (!camera->camera || !SDL_GetCameraFormat(camera->camera, &spec)) {
        return false;
    }

    if (spec.format != SDL_PIXELFORMAT_NV12 && spec.format != SDL_PIXELFORMAT_NV21) {
        SetCameraStatus(camera, "camera format unsupported");
        return false;
    }

    if (camera->preview_texture &&
        camera->spec.format == spec.format &&
        camera->spec.width == spec.width &&
        camera->spec.height == spec.height) {
        return true;
    }

    DestroyPreviewTexture(camera);

    camera->preview_texture = SDL_CreateTexture(state->renderer, spec.format, SDL_TEXTUREACCESS_STREAMING, spec.width, spec.height);
    if (!camera->preview_texture) {
        SetCameraStatus(camera, "preview texture failed");
        return false;
    }

    camera->spec = spec;
    return true;
}

bool OpenCameraStream(AppState *state)
{
    CameraDemoState *camera = &state->camera_demo;
    SDL_CameraSpec requested = {};

    DiscoverCameraDevice(camera);
    if (!camera->available || camera->device_id == 0) {
        SetCameraStatus(camera, "camera unavailable");
        return false;
    }

    requested.format = SDL_PIXELFORMAT_NV12;
    requested.colorspace = SDL_COLORSPACE_BT601_LIMITED;
    requested.width = 640;
    requested.height = 480;
    requested.framerate_numerator = 30;
    requested.framerate_denominator = 1;

    camera->camera = SDL_OpenCamera(camera->device_id, &requested);
    if (!camera->camera) {
        SetCameraStatus(camera, "%s", SDL_GetError());
        return false;
    }

    camera->permission = SDL_GetCameraPermissionState(camera->camera);
    if (camera->permission == SDL_CAMERA_PERMISSION_STATE_DENIED) {
        SetCameraStatus(camera, "camera denied");
        CloseCameraStream(camera);
        return false;
    }

    if (camera->permission == SDL_CAMERA_PERMISSION_STATE_APPROVED) {
        if (!EnsurePreviewTexture(state)) {
            CloseCameraStream(camera);
            return false;
        }
        SetCameraStatus(camera, "camera live");
    } else {
        SetCameraStatus(camera, "camera starting...");
    }

    camera->enabled = true;
    return true;
}

void UpdateStatusForIdle(CameraDemoState *camera)
{
    if (!camera->available) {
        SetCameraStatus(camera, "camera unavailable");
    } else if (!camera->enabled) {
        SetCameraStatus(camera, "camera off");
    } else if (camera->permission == SDL_CAMERA_PERMISSION_STATE_PENDING) {
        SetCameraStatus(camera, "camera starting...");
    } else if (!camera->frame_ready) {
        SetCameraStatus(camera, "camera warming up...");
    } else {
        SetCameraStatus(camera, "camera live");
    }
}

} // namespace

void ResetCameraDemoState(CameraDemoState *camera)
{
    SDL_zerop(camera);
    camera->permission = SDL_CAMERA_PERMISSION_STATE_PENDING;
    SetCameraStatus(camera, "camera off");
}

bool InitCameraDemo(AppState *state)
{
    DiscoverCameraDevice(&state->camera_demo);
    UpdateStatusForIdle(&state->camera_demo);
    return true;
}

void ShutdownCameraDemo(AppState *state)
{
    if (!state) {
        return;
    }

    CloseCameraStream(&state->camera_demo);
    state->camera_demo.enabled = false;
}

bool HandleCameraDemoEvent(AppState *state, const SDL_Event *event)
{
    CameraDemoState *camera = &state->camera_demo;

    switch (event->type) {
    case SDL_EVENT_CAMERA_DEVICE_ADDED:
        DiscoverCameraDevice(camera);
        UpdateStatusForIdle(camera);
        return true;

    case SDL_EVENT_CAMERA_DEVICE_REMOVED:
        if (event->cdevice.which == camera->device_id) {
            CloseCameraStream(camera);
            DiscoverCameraDevice(camera);
            camera->enabled = false;
            UpdateStatusForIdle(camera);
        }
        return true;

    case SDL_EVENT_CAMERA_DEVICE_APPROVED:
        if (camera->camera && event->cdevice.which == SDL_GetCameraID(camera->camera)) {
            camera->permission = SDL_CAMERA_PERMISSION_STATE_APPROVED;
            if (!EnsurePreviewTexture(state)) {
                CloseCameraStream(camera);
                camera->enabled = false;
            }
            UpdateStatusForIdle(camera);
        }
        return true;

    case SDL_EVENT_CAMERA_DEVICE_DENIED:
        if (camera->camera && event->cdevice.which == SDL_GetCameraID(camera->camera)) {
            camera->permission = SDL_CAMERA_PERMISSION_STATE_DENIED;
            SetCameraStatus(camera, "camera denied");
            CloseCameraStream(camera);
            camera->enabled = false;
        }
        return true;

    default:
        return false;
    }
}

bool ToggleCameraDemo(AppState *state)
{
    CameraDemoState *camera = &state->camera_demo;

    if (camera->enabled) {
        CloseCameraStream(camera);
        camera->enabled = false;
        UpdateStatusForIdle(camera);
        return true;
    }

    return OpenCameraStream(state);
}

void UpdateCameraDemo(AppState *state)
{
    CameraDemoState *camera = &state->camera_demo;
    SDL_Surface *frame = nullptr;
    Uint64 timestamp_ns = 0;

    if (!camera->enabled || !camera->camera) {
        UpdateStatusForIdle(camera);
        return;
    }

    camera->permission = SDL_GetCameraPermissionState(camera->camera);
    if (camera->permission == SDL_CAMERA_PERMISSION_STATE_DENIED) {
        SetCameraStatus(camera, "camera denied");
        CloseCameraStream(camera);
        camera->enabled = false;
        return;
    }

    if (camera->permission != SDL_CAMERA_PERMISSION_STATE_APPROVED) {
        UpdateStatusForIdle(camera);
        return;
    }

    if (!EnsurePreviewTexture(state)) {
        return;
    }

    while (SDL_Surface *candidate = SDL_AcquireCameraFrame(camera->camera, &timestamp_ns)) {
        if (frame) {
            SDL_ReleaseCameraFrame(camera->camera, frame);
        }
        frame = candidate;
    }

    if (!frame) {
        UpdateStatusForIdle(camera);
        return;
    }

    const Uint8 *y_plane = static_cast<const Uint8 *>(frame->pixels);
    const Uint8 *uv_plane = y_plane + (frame->pitch * frame->h);
    if (!SDL_UpdateNVTexture(camera->preview_texture, nullptr, y_plane, frame->pitch, uv_plane, frame->pitch)) {
        SetCameraStatus(camera, "preview upload failed");
        SDL_ReleaseCameraFrame(camera->camera, frame);
        return;
    }

    camera->last_frame_timestamp_ns = timestamp_ns;
    camera->frame_ready = true;
    SetCameraStatus(camera, "camera live");
    SDL_ReleaseCameraFrame(camera->camera, frame);
}

bool RenderCameraDemo(AppState *state, int width, int height, int horizon)
{
    CameraDemoState *camera = &state->camera_demo;
    const float preview_w = SDL_min((float)width * 0.22f, 144.0f);
    const float preview_h = preview_w * 0.75f;
    const float panel_padding = 8.0f;
    const float panel_w = preview_w + panel_padding * 2.0f;
    const float panel_h = preview_h + 36.0f;
    const float panel_x = 20.0f;
    const float panel_y = (float)SDL_max(36, horizon - (int)preview_h - 22);
    const SDL_FRect panel = { panel_x, panel_y, panel_w, panel_h };
    const SDL_FRect preview = { panel_x + panel_padding, panel_y + 20.0f, preview_w, preview_h };

    if (!SetColor(state->renderer, 8, 12, 26, 214) ||
        !SDL_RenderFillRect(state->renderer, &panel) ||
        !SetColor(state->renderer, 80, 220, 255, 255) ||
        !SDL_RenderRect(state->renderer, &panel)) {
        return false;
    }

    if (camera->enabled && camera->preview_texture && camera->frame_ready) {
        if (!SDL_RenderTexture(state->renderer, camera->preview_texture, nullptr, &preview)) {
            return false;
        }
    } else {
        if (!SetColor(state->renderer, 10, 16, 34, 255) ||
            !SDL_RenderFillRect(state->renderer, &preview)) {
            return false;
        }

        for (int i = 0; i < 8; ++i) {
            const float y = preview.y + 8.0f + (float)i * ((preview.h - 16.0f) / 7.0f);
            if (!SetColor(state->renderer, 40, 110, 150, (Uint8)(80 + i * 12)) ||
                !SDL_RenderLine(state->renderer, preview.x + 8.0f, y, preview.x + preview.w - 8.0f, y)) {
                return false;
            }
        }
    }

    if (!SetColor(state->renderer, 255, 255, 255, 255) ||
        !SDL_RenderDebugText(state->renderer, panel_x + 10.0f, panel_y + 6.0f, "camera") ||
        !SDL_RenderDebugText(state->renderer, panel_x + 10.0f, panel_y + panel_h - 16.0f, camera->status_text) ||
        !SDL_RenderDebugText(state->renderer, panel_x + panel_w - 78.0f, panel_y + panel_h - 16.0f, "C / B")) {
        return false;
    }

    return true;
}
