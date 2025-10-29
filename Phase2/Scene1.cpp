#include <windows.h>
#include <tchar.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"

#include "SceneSwitcher.h"
#include "Scenes.h"


extern VkResult CreateTexture2D(const char* path,
                                VkImage* outImg,
                                VkDeviceMemory* outMem,
                                VkImageView* outView,
                                VkSampler* outSampler);
extern VkResult CreateCubemap(const char* const faces[6],
                              VkImage* outImg,
                              VkDeviceMemory* outMem,
                              VkImageView* outView,
                              VkSampler* outSampler);

VkShaderModule gShaderModule_vertex_scene1 = VK_NULL_HANDLE;
VkShaderModule gShaderModule_fragment_scene1 = VK_NULL_HANDLE;

static const DWORD K_OVERLAY_LEAD_MS     = 450u;
static const DWORD K_OVERLAY_FADE_MS     = 450u;
static const DWORD K_HOLD_DURATION_MS    = 3000u;
static const int   K_PAN_REPEATS         = 12;
static const float K_MIN_SEP_DEG         = 90.0f;
static const int   K_OVERLAY_COUNT       = 12;
static const float K_OVERLAY_SIZE_FRAC0  = 0.55f;
static float       sOverlaySizeFrac      = K_OVERLAY_SIZE_FRAC0;
static float       sPanSpeedDegPerSec    = 30.0f;

static const char* const gSkyboxFaces[6] =
{
    "images_Scene1/right.png",
    "images_Scene1/left.png",
    "images_Scene1/top.png",
    "images_Scene1/bottom.png",
    "images_Scene1/front.png",
    "images_Scene1/back.png"
};

static const char* const gOverlayPathList[K_OVERLAY_COUNT] =
{
    "images_Scene1/01_Mesh.png", "images_Scene1/02_Vrushabh.png", "images_Scene1/03_Mithun.png",
    "images_Scene1/04_Karka.png","images_Scene1/05_Simha.png",    "images_Scene1/06_Kanya.png",
    "images_Scene1/07_Tula.png", "images_Scene1/08_Vruschik.png","images_Scene1/09_Dhanu.png",
    "images_Scene1/10_Makar.png","images_Scene1/11_Kumbh.png",   "images_Scene1/12_Meen.png"
};

static VkImage        sSkyImage     = VK_NULL_HANDLE;
static VkDeviceMemory sSkyMem       = VK_NULL_HANDLE;
static VkImageView    sSkyView      = VK_NULL_HANDLE;
static VkSampler      sSkySampler   = VK_NULL_HANDLE;

static VkImage        sOverlayImages[K_OVERLAY_COUNT];
static VkDeviceMemory sOverlayMem[K_OVERLAY_COUNT];
static VkImageView    sOverlayViews[K_OVERLAY_COUNT];
static VkSampler      sOverlaySamplers[K_OVERLAY_COUNT];
static int            sOverlayBound   = -1;
static int            sOverlayPending = -1;
static BOOL           sCmdBuffersDirty = FALSE;

typedef enum CameraPhaseTag
{
    CAM_PAN,
    CAM_HOLD,
    CAM_STOPPED
} CameraPhase;

static CameraPhase sCamPhase       = CAM_PAN;
static glm::quat   sCamQ           = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
static glm::quat   sCamQStart      = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
static glm::quat   sCamQTarget     = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
static DWORD       sPhaseStartMs   = 0;
static DWORD       sPanDurationMs  = 1200;
static int         sPansDone       = 0;

static float       sPendingBlendFade  = 0.0f;
static float       sRecordedBlendFade = 0.0f;

// === Showcase timeline ===
// Scene0 (Intro) : pause 2s, fade in over 3s, hold for 7s, transition overlay across 8s
// Scene1 (Main)  : run animation and linger 3s after it completes
// Scene2 (Final) : hold combined main scene + credits for 10s before focus pull to finale
// Scene3 (Finale): hold fully-focused composite for 5s, then fade to black over 3.5s
static const DWORD K_SCENE0_FADE_IN_DELAY_MS        = 2000u;
static const DWORD K_SCENE0_FADE_IN_MS              = 3000u;
static const DWORD K_SCENE0_HOLD_MS                 = 7000u;
static const DWORD K_SCENE0_OVERLAY_TRANSITION_MS   = 8000u;
static const DWORD K_SCENE1_FADE_TO_BLACK_MS        = 3000u;
static const DWORD K_SCENE1_POST_ANIM_EXTRA_MS      = 3000u;
static const DWORD K_SCENE2_HOLD_MS                 = 10000u;
static const DWORD K_SCENE2_FOCUS_PULL_MS           = 4500u;
static const DWORD K_SCENE3_HOLD_MS                 = 5000u;
static const DWORD K_SCENE3_FADE_TO_BLACK_MS        = 3500u;

typedef enum SequenceStateTag
{
    SEQUENCE_IDLE = 0,
    SEQUENCE_SCENE0_DELAY,
    SEQUENCE_SCENE0_FADE_IN,
    SEQUENCE_SCENE0_HOLD,
    SEQUENCE_SCENE0_OVERLAY_TRANSITION,
    SEQUENCE_SCENE1_HOLD,
    SEQUENCE_SCENE1_FADE_TO_BLACK,
    SEQUENCE_SCENE2_HOLD,
    SEQUENCE_SCENE2_FOCUS_PULL,
    SEQUENCE_SCENE3_HOLD,
    SEQUENCE_SCENE3_FADE_TO_BLACK,
    SEQUENCE_COMPLETE
} SequenceState;

static SequenceState sSequenceState = SEQUENCE_IDLE;
static DWORD         sSequenceStateStartMs = 0;
static BOOL          sScene1PostAnimHoldActive = FALSE;
static DWORD         sScene1PostAnimHoldStartMs = 0;
static BOOL          sScene1StartedViaOverlay = FALSE;

static float ease01(float x)
{
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return x * x * (3.0f - 2.0f * x);
}

static float frand01(void)
{
    return (float)rand() / (float)RAND_MAX;
}

static void BindOverlayTexture(int which)
{
    if (which < 0) return;
    if (which >= K_OVERLAY_COUNT) which = K_OVERLAY_COUNT - 1;
    if (sOverlayBound == which) return;
    if (sOverlayPending == which) return;
    sOverlayPending = which;
}

static glm::quat RandomOrientationQuat(void)
{
    float u1 = frand01();
    float u2 = frand01() * 2.0f * glm::pi<float>();
    float u3 = frand01() * 2.0f * glm::pi<float>();
    float s1 = sqrtf(1.0f - u1);
    float s2 = sqrtf(u1);
    float x = s1 * sinf(u2);
    float y = s1 * cosf(u2);
    float z = s2 * sinf(u3);
    float w = s2 * cosf(u3);
    return glm::normalize(glm::quat(w, x, y, z));
}

static glm::quat RandomOrientationFarFrom(const glm::quat from, float minSepDeg)
{
    glm::vec3 fPrev = from * glm::vec3(0.0f, 0.0f, -1.0f);
    float minDot = cosf(glm::radians(minSepDeg));
    for (int i = 0; i < 64; ++i)
    {
        glm::quat q = RandomOrientationQuat();
        glm::vec3 f = q * glm::vec3(0.0f, 0.0f, -1.0f);
        float d = glm::dot(glm::normalize(fPrev), glm::normalize(f));
        if (d <= minDot)
        {
            return glm::normalize(q);
        }
    }
    glm::quat flip = glm::rotation(fPrev, -fPrev);
    return glm::normalize(flip * from);
}

static DWORD CalcPanDurationMs(const glm::quat q0, const glm::quat q1)
{
    glm::quat d = glm::normalize(q1 * glm::conjugate(q0));
    float w = d.w;
    if (w < -1.0f) w = -1.0f;
    if (w >  1.0f) w =  1.0f;
    float angleRad = 2.0f * acosf(w);
    float angleDeg = glm::degrees(angleRad);
    float denom = sPanSpeedDegPerSec;
    if (denom < 1e-3f) denom = 1e-3f;
    float ms = (angleDeg / denom) * 1000.0f;
    if (ms < 120.0f)   ms = 120.0f;
    if (ms > 30000.0f) ms = 30000.0f;
    return (DWORD)(ms + 0.5f);
}

static void SetPanSpeedDegPerSec(float s)
{
    if (s < 1.0f)   s = 1.0f;
    if (s > 360.0f) s = 360.0f;

    DWORD now = GetTickCount();
    if (sCamPhase == CAM_PAN)
    {
        float oldDur = (float)sPanDurationMs;
        float prog = (now - sPhaseStartMs) / oldDur;
        if (prog < 0.0f) prog = 0.0f;
        if (prog > 1.0f) prog = 1.0f;
        sPanSpeedDegPerSec = s;
        sPanDurationMs = CalcPanDurationMs(sCamQStart, sCamQTarget);
        sPhaseStartMs = now - (DWORD)(prog * (float)sPanDurationMs);
    }
    else
    {
        sPanSpeedDegPerSec = s;
    }
}

static void SetOverlaySizeFrac(float frac)
{
    if (frac < 0.05f) frac = 0.05f;
    if (frac > 2.00f) frac = 2.00f;
    sOverlaySizeFrac = frac;
}

static void BeginNewPanInternal(void)
{
    int which = sPansDone;
    if (which >= K_OVERLAY_COUNT) which = K_OVERLAY_COUNT - 1;
    BindOverlayTexture(which);
    sCamQStart     = sCamQ;
    sCamQTarget    = RandomOrientationFarFrom(sCamQ, K_MIN_SEP_DEG);
    sPanDurationMs = CalcPanDurationMs(sCamQStart, sCamQTarget);
    sPhaseStartMs  = GetTickCount();
    sCamPhase      = CAM_PAN;
}

static float Clamp01(float x)
{
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static BOOL IsSequenceActiveInternal(void)
{
    return sSequenceState != SEQUENCE_IDLE;
}

static void UpdateBlendFadeInternal(float fade)
{
    float clamped = Clamp01(fade);
    if (fabsf(clamped - sPendingBlendFade) > 1e-6f)
    {
        sPendingBlendFade = clamped;
    }
    if (fabsf(sPendingBlendFade - sRecordedBlendFade) > 5e-4f)
    {
        sCmdBuffersDirty = TRUE;
    }
}

static float ComputeOverlayFadeForPan(void)
{
    DWORD now = GetTickCount();
    if (sCamPhase == CAM_PAN)
    {
        DWORD elapsed = now - sPhaseStartMs;
        DWORD lead = (K_OVERLAY_LEAD_MS < sPanDurationMs) ? K_OVERLAY_LEAD_MS : sPanDurationMs;
        if (elapsed <= sPanDurationMs - lead)
        {
            return 0.0f;
        }
        float u = (float)(elapsed - (sPanDurationMs - lead)) / (float)lead;
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;
        return ease01(u);
    }
    else if (sCamPhase == CAM_HOLD)
    {
        DWORD t = now - sPhaseStartMs;
        DWORD out = (K_OVERLAY_FADE_MS < K_HOLD_DURATION_MS) ? K_OVERLAY_FADE_MS : K_HOLD_DURATION_MS;
        if (t < (K_HOLD_DURATION_MS - out))
        {
            return 1.0f;
        }
        float u = (float)(K_HOLD_DURATION_MS - t) / (float)out;
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;
        return ease01(u);
    }
    return 0.0f;
}

static BOOL IsScene1AnimationComplete(void)
{
    return sCamPhase == CAM_STOPPED;
}

static void ResetScene1ForSequence(void)
{
    sOverlayPending = -1;
    sOverlayBound   = -1;
    sPansDone       = 0;
    sCamPhase       = CAM_STOPPED;
    sCamQ           = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    sCamQStart      = sCamQ;
    sCamQTarget     = sCamQ;

    BeginNewPanInternal();
    sCmdBuffersDirty = TRUE;
}

static void EnterSequenceState(SequenceState state)
{
    sSequenceState = state;
    sSequenceStateStartMs = GetTickCount();

    switch (state)
    {
    case SEQUENCE_SCENE0_DELAY:
        gActiveScene = ACTIVE_SCENE_SCENE0;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 0.0f; // remain black before reveal
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        break;
    case SEQUENCE_SCENE0_FADE_IN:
        gActiveScene = ACTIVE_SCENE_SCENE0;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        BeginScene0Audio();
        gCtx_Switcher.gFade = 0.0f; // fade will animate towards full visibility
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        break;
    case SEQUENCE_SCENE0_HOLD:
        gActiveScene = ACTIVE_SCENE_SCENE0;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 1.0f; // fully visible
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        break;
    case SEQUENCE_SCENE0_OVERLAY_TRANSITION:
        gActiveScene = ACTIVE_SCENE_SCENE1;
        gCtx_Switcher.gScene01DoubleExposureActive = TRUE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 0.0f;
        ResetScene1ForSequence();
        sScene1StartedViaOverlay = TRUE;
        sScene1PostAnimHoldActive = FALSE;
        sScene1PostAnimHoldStartMs = 0;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        break;
    case SEQUENCE_SCENE1_HOLD:
        gActiveScene = ACTIVE_SCENE_SCENE1;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 1.0f; // fully reveal Scene1 after overlay
        if (!sScene1StartedViaOverlay)
        {
            ResetScene1ForSequence();
        }
        else
        {
            sScene1StartedViaOverlay = FALSE;
        }
        sScene1PostAnimHoldActive = FALSE;
        sScene1PostAnimHoldStartMs = 0;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        break;
    case SEQUENCE_SCENE1_FADE_TO_BLACK:
        gActiveScene = ACTIVE_SCENE_SCENE1;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = TRUE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        break;
    case SEQUENCE_SCENE2_HOLD:
        gActiveScene = ACTIVE_SCENE_SCENE2;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 1.0f;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        break;
    case SEQUENCE_SCENE2_FOCUS_PULL:
        gActiveScene = ACTIVE_SCENE_SCENE3;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = TRUE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 1.0f;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        break;
    case SEQUENCE_SCENE3_HOLD:
        gActiveScene = ACTIVE_SCENE_SCENE3;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 1.0f;
        gCtx_Switcher.gFade = 1.0f;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        break;
    case SEQUENCE_SCENE3_FADE_TO_BLACK:
        gActiveScene = ACTIVE_SCENE_SCENE3;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        break;
    case SEQUENCE_COMPLETE:
        // End on black
        gActiveScene = ACTIVE_SCENE_NONE;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 0.0f;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        break;
    default:
        break;
    }
    // Ensure command buffers are re-recorded on state changes.
    sCmdBuffersDirty = TRUE;
}

static void StopShowcaseSequenceInternal(void)
{
    sSequenceState = SEQUENCE_IDLE;
    sScene1PostAnimHoldActive = FALSE;
    sScene1PostAnimHoldStartMs = 0;
    sScene1StartedViaOverlay = FALSE;
    gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
    gCtx_Switcher.gScene12CrossfadeActive = FALSE;
    gCtx_Switcher.gScene23FocusPullActive = FALSE;
    gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
    // Leave gFade as-is to remain on black if the sequence finished there.
    UpdateBlendFadeInternal(gCtx_Switcher.gFade);
}

static void UpdateShowcaseSequenceInternal(void)
{
    if (!IsSequenceActiveInternal())
    {
        return;
    }

    DWORD now = GetTickCount();
    DWORD elapsed = 0;
    float fadeProgress = 0.0f;

    switch (sSequenceState)
    {
    case SEQUENCE_SCENE0_DELAY:
        gCtx_Switcher.gFade = 0.0f; // stay on black until reveal
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        if (now - sSequenceStateStartMs >= K_SCENE0_FADE_IN_DELAY_MS)
        {
            BeginScene0Audio();
            EnterSequenceState(SEQUENCE_SCENE0_FADE_IN);
        }
        break;

    case SEQUENCE_SCENE0_FADE_IN:
        fadeProgress = Clamp01((now - sSequenceStateStartMs) / (float)K_SCENE0_FADE_IN_MS);
        gCtx_Switcher.gFade = fadeProgress; // fade from black to full visibility
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        if (fadeProgress >= 1.0f)
        {
            EnterSequenceState(SEQUENCE_SCENE0_HOLD);
        }
        break;

    case SEQUENCE_SCENE0_HOLD:
        elapsed = now - sSequenceStateStartMs;
        gCtx_Switcher.gFade = 1.0f; // fully visible
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        if (elapsed >= K_SCENE0_HOLD_MS) EnterSequenceState(SEQUENCE_SCENE0_OVERLAY_TRANSITION);
        break;

    case SEQUENCE_SCENE0_OVERLAY_TRANSITION:
    {
        DWORD elapsedOverlay = now - sSequenceStateStartMs;
        float duration = (float)K_SCENE0_OVERLAY_TRANSITION_MS;
        if (duration < 1.0f)
        {
            duration = 1.0f;
        }
        fadeProgress = Clamp01(elapsedOverlay / duration);
        gCtx_Switcher.gFade = fadeProgress;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        if (fadeProgress >= 1.0f)
        {
            EnterSequenceState(SEQUENCE_SCENE1_HOLD);
        }
        break;
    }

    case SEQUENCE_SCENE1_HOLD:
        gCtx_Switcher.gFade = 1.0f;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        if (IsScene1AnimationComplete())
        {
            if (!sScene1PostAnimHoldActive)
            {
                sScene1PostAnimHoldActive = TRUE;
                sScene1PostAnimHoldStartMs = now;
            }
            else if (now - sScene1PostAnimHoldStartMs >= K_SCENE1_POST_ANIM_EXTRA_MS)
            {
                EnterSequenceState(SEQUENCE_SCENE1_FADE_TO_BLACK);
            }
        }
        break;

    case SEQUENCE_SCENE1_FADE_TO_BLACK:
        fadeProgress = Clamp01((now - sSequenceStateStartMs) / (float)K_SCENE1_FADE_TO_BLACK_MS);
        gCtx_Switcher.gFade = 1.0f - fadeProgress;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        if (fadeProgress >= 1.0f) EnterSequenceState(SEQUENCE_SCENE2_HOLD);
        break;

    case SEQUENCE_SCENE2_HOLD:
        gCtx_Switcher.gFade = 1.0f;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        elapsed = now - sSequenceStateStartMs;
        if (elapsed >= K_SCENE2_HOLD_MS)
        {
            EnterSequenceState(SEQUENCE_SCENE2_FOCUS_PULL);
        }
        break;

    case SEQUENCE_SCENE2_FOCUS_PULL:
    {
        gCtx_Switcher.gFade = 1.0f;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        DWORD elapsedFocus = now - sSequenceStateStartMs;
        float duration = (float)K_SCENE2_FOCUS_PULL_MS;
        if (duration < 1.0f)
        {
            duration = 1.0f;
        }
        float u = Clamp01(elapsedFocus / duration);
        float focus = ease01(u);
        if (fabsf(focus - gCtx_Switcher.gScene23FocusPullFactor) > 5e-4f)
        {
            gCtx_Switcher.gScene23FocusPullFactor = focus;
            sCmdBuffersDirty = TRUE;
        }
        if (u >= 1.0f)
        {
            EnterSequenceState(SEQUENCE_SCENE3_HOLD);
        }
    }
        break;

    case SEQUENCE_SCENE3_HOLD:
        gCtx_Switcher.gFade = 1.0f;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        if (fabsf(1.0f - gCtx_Switcher.gScene23FocusPullFactor) > 5e-4f)
        {
            gCtx_Switcher.gScene23FocusPullFactor = 1.0f;
            sCmdBuffersDirty = TRUE;
        }
        elapsed = now - sSequenceStateStartMs;
        if (elapsed >= K_SCENE3_HOLD_MS)
        {
            EnterSequenceState(SEQUENCE_SCENE3_FADE_TO_BLACK);
        }
        break;

    case SEQUENCE_SCENE3_FADE_TO_BLACK:
        fadeProgress = Clamp01((now - sSequenceStateStartMs) / (float)K_SCENE3_FADE_TO_BLACK_MS);
        gCtx_Switcher.gFade = 1.0f - fadeProgress;
        UpdateBlendFadeInternal(gCtx_Switcher.gFade);
        if (fabsf(gCtx_Switcher.gScene23FocusPullFactor) > 5e-4f)
        {
            gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
            sCmdBuffersDirty = TRUE;
        }
        if (fadeProgress >= 1.0f) EnterSequenceState(SEQUENCE_COMPLETE);
        break;

    case SEQUENCE_COMPLETE:
        StopShowcaseSequenceInternal();
        break;

    default:
        break;
    }
}

VkResult Scene1_CreateTextures(void)
{
    VkResult r = CreateCubemap(gSkyboxFaces, &sSkyImage, &sSkyMem, &sSkyView, &sSkySampler);
    if (r != VK_SUCCESS) return r;
    for (int i = 0; i < K_OVERLAY_COUNT; ++i)
    {
        VkResult rr = CreateTexture2D(gOverlayPathList[i],
                                      &sOverlayImages[i],
                                      &sOverlayMem[i],
                                      &sOverlayViews[i],
                                      &sOverlaySamplers[i]);
        if (rr != VK_SUCCESS) return rr;
    }
    sOverlayBound = 0;
    sOverlayPending = -1;
    return VK_SUCCESS;
}

void Scene1_DestroyTextures(void)
{
    for (int i = 0; i < K_OVERLAY_COUNT; ++i)
    {
        if (sOverlaySamplers[i]) vkDestroySampler(gCtx_Switcher.vkDevice, sOverlaySamplers[i], NULL);
        if (sOverlayViews[i])    vkDestroyImageView(gCtx_Switcher.vkDevice, sOverlayViews[i], NULL);
        if (sOverlayMem[i])      vkFreeMemory(gCtx_Switcher.vkDevice, sOverlayMem[i], NULL);
        if (sOverlayImages[i])   vkDestroyImage(gCtx_Switcher.vkDevice, sOverlayImages[i], NULL);
        sOverlaySamplers[i] = VK_NULL_HANDLE;
        sOverlayViews[i]    = VK_NULL_HANDLE;
        sOverlayMem[i]      = VK_NULL_HANDLE;
        sOverlayImages[i]   = VK_NULL_HANDLE;
    }
    if (sSkySampler) vkDestroySampler(gCtx_Switcher.vkDevice, sSkySampler, NULL);
    if (sSkyView)    vkDestroyImageView(gCtx_Switcher.vkDevice, sSkyView, NULL);
    if (sSkyMem)     vkFreeMemory(gCtx_Switcher.vkDevice, sSkyMem, NULL);
    if (sSkyImage)   vkDestroyImage(gCtx_Switcher.vkDevice, sSkyImage, NULL);
    sSkySampler = VK_NULL_HANDLE;
    sSkyView    = VK_NULL_HANDLE;
    sSkyMem     = VK_NULL_HANDLE;
    sSkyImage   = VK_NULL_HANDLE;
}

void Scene1_BeginNewPan(void)
{
    BeginNewPanInternal();
}

void Scene1_UpdateCameraAnim(void)
{
    DWORD now = GetTickCount();
    if (sCamPhase == CAM_PAN)
    {
        float t = (now - sPhaseStartMs) / (float)sPanDurationMs;
        if (t >= 1.0f)
        {
            sCamQ = sCamQTarget;
            sCamPhase = CAM_HOLD;
            sPhaseStartMs = now;
        }
        else
        {
            float s = ease01(t);
            sCamQ = glm::slerp(sCamQStart, sCamQTarget, s);
        }
    }
    else if (sCamPhase == CAM_HOLD)
    {
        if (now - sPhaseStartMs >= K_HOLD_DURATION_MS)
        {
            sPansDone++;
            if (sPansDone >= K_PAN_REPEATS)
            {
                sCamPhase = CAM_STOPPED;
            }
            else
            {
                BeginNewPanInternal();
            }
        }
    }
}

VkResult Scene1_UpdateUniformBuffer(void)
{
    GlobalContext_MyUniformData scene1;
    memset(&scene1, 0, sizeof(scene1));
    scene1.modelMatrix = glm::mat4(1.0f);

    glm::mat4 V = glm::mat4_cast(glm::conjugate(sCamQ));
    V[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    scene1.viewMatrix = V;

    glm::mat4 P = glm::perspective(glm::radians(45.0f),
                                   (float)gCtx_Switcher.winWidth / (float)gCtx_Switcher.winHeight,
                                   0.1f,
                                   100.0f);
    P[1][1] *= -1.0f;
    scene1.projectionMatrix = P;

    scene1.fade = glm::vec4(ComputeOverlayFadeForPan(),
                             (float)gCtx_Switcher.winWidth,
                             (float)gCtx_Switcher.winHeight,
                             sOverlaySizeFrac);

    void* p = NULL;
    VkResult vkResult = vkMapMemory(gCtx_Switcher.vkDevice,
                                    gCtx_Switcher.uniformData_scene1.vkDeviceMemory,
                                    0,
                                    sizeof(scene1),
                                    0,
                                    &p);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Scene1_UpdateUniformBuffer() --> vkMapMemory() failed %d\n", vkResult);
        return vkResult;
    }
    memcpy(p, &scene1, sizeof(scene1));
    vkUnmapMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene1.vkDeviceMemory);
    return VK_SUCCESS;
}

void Scene1_StartSequence(void)
{
    sScene1StartedViaOverlay = FALSE;
    gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
    EnterSequenceState(SEQUENCE_SCENE0_DELAY);
}

void Scene1_StopSequence(void)
{
    StopShowcaseSequenceInternal();
}

void Scene1_UpdateSequence(void)
{
    UpdateShowcaseSequenceInternal();
}

BOOL Scene1_IsSequenceActive(void)
{
    return IsSequenceActiveInternal();
}

void Scene1_AdjustPanSpeed(float delta)
{
    SetPanSpeedDegPerSec(sPanSpeedDegPerSec + delta);
}

void Scene1_AdjustOverlaySize(float delta)
{
    SetOverlaySizeFrac(sOverlaySizeFrac + delta);
}

float Scene1_GetPendingBlendFade(void)
{
    return sPendingBlendFade;
}

void Scene1_CommitPendingBlendFade(void)
{
    sRecordedBlendFade = sPendingBlendFade;
}

void Scene1_UpdateBlendFade(float fade)
{
    UpdateBlendFadeInternal(fade);
}

BOOL Scene1_IsCommandBufferDirty(void)
{
    return sCmdBuffersDirty;
}

void Scene1_ClearCommandBufferDirty(void)
{
    sCmdBuffersDirty = FALSE;
}

void Scene1_MarkCommandBufferDirty(void)
{
    sCmdBuffersDirty = TRUE;
}

BOOL Scene1_HasPendingOverlay(void)
{
    return sOverlayPending >= 0;
}

VkResult Scene1_BindPendingOverlay(VkDescriptorSet descriptorSet)
{
    if (sOverlayPending < 0 || sOverlayPending >= K_OVERLAY_COUNT)
    {
        return VK_SUCCESS;
    }

    VkDescriptorImageInfo overlay;
    memset(&overlay, 0, sizeof(overlay));
    overlay.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    overlay.imageView   = sOverlayViews[sOverlayPending];
    overlay.sampler     = sOverlaySamplers[sOverlayPending];

    VkWriteDescriptorSet write;
    memset(&write, 0, sizeof(write));
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = 2;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &overlay;

    vkUpdateDescriptorSets(gCtx_Switcher.vkDevice, 1, &write, 0, NULL);

    sOverlayBound = sOverlayPending;
    sOverlayPending = -1;
    sCmdBuffersDirty = TRUE;
    return VK_SUCCESS;
}

void Scene1_GetSkyDescriptor(VkDescriptorImageInfo* info)
{
    if (!info) return;
    memset(info, 0, sizeof(*info));
    info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info->imageView = sSkyView;
    info->sampler = sSkySampler;
}

void Scene1_GetOverlayDescriptor(VkDescriptorImageInfo* info)
{
    if (!info) return;
    memset(info, 0, sizeof(*info));
    VkImageView view = (sOverlayBound >= 0) ? sOverlayViews[sOverlayBound] : sOverlayViews[0];
    VkSampler sampler = (sOverlayBound >= 0) ? sOverlaySamplers[sOverlayBound] : sOverlaySamplers[0];
    info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info->imageView = view;
    info->sampler = sampler;
}
