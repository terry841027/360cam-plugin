#include "Plugin.h"
#include <ffgl/FFGLLib.h>

static CFFGLPluginInfo PluginInfo(
    PluginFactory< X5FisheyeViewer >,
    "TRX5",                                                    // 4-char unique ID
    "X5 Fisheye Viewer",                                       // display name
    2, 1,                                                      // FFGL API version
    1, 0,                                                      // plugin version
    FF_EFFECT,
    "Insta360 X5 SBS dual-fisheye to rectilinear. Pan/Tilt/Roll/FOV control.",
    "FleetView"
);

// ── Vertex shader ────────────────────────────────────────────────────────────
static const char* kVert = R"glsl(
#version 410 core
uniform vec2 MaxUV;
layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV0;
out vec2 vUV;
void main() {
    gl_Position = vPosition;
    vUV = vUV0 * MaxUV;
}
)glsl";

// ── Fragment shader ──────────────────────────────────────────────────────────
static const char* kFrag = R"glsl(
#version 410 core
#define PI 3.14159265358979323846

uniform sampler2D InputTexture;
uniform float AspectRatio;
uniform int   Lens;       // 0 = left half, 1 = right half
uniform float Pan;        // degrees
uniform float Tilt;       // degrees
uniform float Roll;       // degrees
uniform float FOV;        // output FOV degrees
uniform float LensFOV;    // fisheye lens FOV degrees (equidistant model)

in  vec2 vUV;
out vec4 fragColor;

void main() {
    // NDC [-1,1] with aspect correction
    vec2 ndc = (vUV * 2.0 - 1.0) * vec2(AspectRatio, 1.0);

    // Build viewing ray
    float fovRad = FOV * PI / 180.0;
    vec3 ray = normalize(vec3(ndc, 1.0 / tan(fovRad * 0.5)));

    // Roll (Z axis)
    float cr = cos(Roll * PI / 180.0), sr = sin(Roll * PI / 180.0);
    ray = vec3(ray.x * cr - ray.y * sr,
               ray.x * sr + ray.y * cr,
               ray.z);

    // Tilt (X axis)
    float ct = cos(-Tilt * PI / 180.0), st = sin(-Tilt * PI / 180.0);
    ray = vec3(ray.x,
               ray.y * ct - ray.z * st,
               ray.y * st + ray.z * ct);

    // Pan (Y axis)
    float cp = cos(Pan * PI / 180.0), sp = sin(Pan * PI / 180.0);
    ray = vec3( ray.x * cp + ray.z * sp,
                ray.y,
               -ray.x * sp + ray.z * cp);

    // Equidistant fisheye: r = theta / maxTheta
    float theta    = acos(clamp(ray.z, -1.0, 1.0));
    float phi      = atan(ray.y, ray.x);
    float maxTheta = LensFOV * 0.5 * PI / 180.0;
    float r        = theta / maxTheta;

    if (r > 1.0) { fragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }

    // UV inside one fisheye circle [0,1]
    vec2 sampleUV = vec2(0.5 + r * cos(phi) * 0.5,
                         0.5 + r * sin(phi) * 0.5);

    // Remap to left (0.0–0.5) or right (0.5–1.0) half of SBS frame
    sampleUV.x = sampleUV.x * 0.5 + float(Lens) * 0.5;

    fragColor = texture(InputTexture, sampleUV);
}
)glsl";

// ── Constructor ──────────────────────────────────────────────────────────────
X5FisheyeViewer::X5FisheyeViewer()
    : m_lens(0), m_pan(0.f), m_tilt(0.f), m_roll(0.f),
      m_fov(90.f), m_lensFOV(180.f), m_aspect(1.f)
{
    SetMinInputs(1);
    SetMaxInputs(1);

    // FFGL sliders are [0.0, 1.0]; real units are mapped in SetFloatParameter.
    SetParamInfo(PARAM_LENS,    "Lens  (0=Left / 1=Right)", FF_TYPE_STANDARD, 0.0f);
    SetParamInfo(PARAM_PAN,     "Pan",                       FF_TYPE_STANDARD, 0.5f);    // 0.5 → 0°
    SetParamInfo(PARAM_TILT,    "Tilt",                      FF_TYPE_STANDARD, 0.5f);    // 0.5 → 0°
    SetParamInfo(PARAM_ROLL,    "Roll",                      FF_TYPE_STANDARD, 0.5f);    // 0.5 → 0°
    SetParamInfo(PARAM_FOV,     "FOV",                       FF_TYPE_STANDARD, 0.5714f); // ≈ 90°
    SetParamInfo(PARAM_LENSFOV, "Lens FOV (fisheye corr.)",  FF_TYPE_STANDARD, 0.5f);   // 180°
}

// ── InitGL / DeInitGL ────────────────────────────────────────────────────────
FFResult X5FisheyeViewer::InitGL(const FFGLViewportStruct* vp) {
    m_aspect = (vp->height > 0) ? (float)vp->width / vp->height : 1.f;
    if (!m_shader.Compile(kVert, kFrag)) return FF_FAIL;
    if (!m_quad.Initialise())            return FF_FAIL;
    return FF_SUCCESS;
}

FFResult X5FisheyeViewer::DeInitGL() {
    m_shader.FreeGLResources();
    m_quad.Release();
    return FF_SUCCESS;
}

// ── ProcessOpenGL ────────────────────────────────────────────────────────────
FFResult X5FisheyeViewer::ProcessOpenGL(ProcessOpenGLStruct* pGL) {
    if (!pGL->numInputTextures || !pGL->inputTextures[0]) return FF_FAIL;

    FFGLTextureStruct& tex = *(pGL->inputTextures[0]);
    if (!tex.Handle) return FF_FAIL;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex.Handle);

    float maxU = tex.HardwareWidth  ? (float)tex.Width  / tex.HardwareWidth  : 1.f;
    float maxV = tex.HardwareHeight ? (float)tex.Height / tex.HardwareHeight : 1.f;

    m_shader.Use();
    m_shader.Set("InputTexture", 0);
    m_shader.Set("MaxUV",        maxU, maxV);
    m_shader.Set("AspectRatio",  m_aspect);
    m_shader.Set("Lens",         m_lens);
    m_shader.Set("Pan",          m_pan);
    m_shader.Set("Tilt",         m_tilt);
    m_shader.Set("Roll",         m_roll);
    m_shader.Set("FOV",          m_fov);
    m_shader.Set("LensFOV",      m_lensFOV);

    m_quad.Draw();
    m_shader.UnUse();

    return FF_SUCCESS;
}

// ── Parameter get / set ──────────────────────────────────────────────────────
// Slider [0,1] → real units:
//   Pan/Roll : (v - 0.5) * 360  →  [-180, 180]
//   Tilt     : (v - 0.5) * 180  →  [ -90,  90]
//   FOV      : v * 140 + 10     →  [  10, 150]
//   LensFOV  : v * 60  + 150    →  [ 150, 210]

FFResult X5FisheyeViewer::SetFloatParameter(unsigned int idx, float val) {
    switch (idx) {
        case PARAM_LENS:    m_lens    = (val > 0.5f) ? 1 : 0;   break;
        case PARAM_PAN:     m_pan     = (val - 0.5f) * 360.f;   break;
        case PARAM_TILT:    m_tilt    = (val - 0.5f) * 180.f;   break;
        case PARAM_ROLL:    m_roll    = (val - 0.5f) * 360.f;   break;
        case PARAM_FOV:     m_fov     = val * 140.f + 10.f;     break;
        case PARAM_LENSFOV: m_lensFOV = val * 60.f  + 150.f;    break;
        default: return FF_FAIL;
    }
    return FF_SUCCESS;
}

float X5FisheyeViewer::GetFloatParameter(unsigned int idx) {
    switch (idx) {
        case PARAM_LENS:    return (float)m_lens;
        case PARAM_PAN:     return m_pan     / 360.f + 0.5f;
        case PARAM_TILT:    return m_tilt    / 180.f + 0.5f;
        case PARAM_ROLL:    return m_roll    / 360.f + 0.5f;
        case PARAM_FOV:     return (m_fov     - 10.f)  / 140.f;
        case PARAM_LENSFOV: return (m_lensFOV - 150.f) / 60.f;
        default: return 0.f;
    }
}
