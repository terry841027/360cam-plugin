#include "Plugin.h"

static CFFGLPluginInfo PluginInfo(
    PluginFactory< X5FisheyeViewer >,
    "TRX5",
    "X5 Fisheye 360",
    2, 1,
    1, 0,
    FF_EFFECT,
    "Insta360 X5 Over/Under full 360 viewer",
    "FleetView"
);

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

static const char* kFrag = R"glsl(
#version 410 core
#define PI 3.14159265358979323846

uniform sampler2D InputTexture;
uniform float AspectRatio;   // full frame W/H
uniform float HalfAspect;   // (W) / (H/2)  — aspect ratio of each half
uniform int   Swap;
uniform float Pan;
uniform float Tilt;
uniform float Roll;
uniform float FOV;
uniform float LensFOV;

in  vec2 vUV;
out vec4 fragColor;

void main() {
    vec2 ndc = (vUV * 2.0 - 1.0) * vec2(AspectRatio, 1.0);
    float fovRad = FOV * PI / 180.0;
    vec3 ray = normalize(vec3(ndc, 1.0 / tan(fovRad * 0.5)));

    // Roll (Z axis)
    float cr = cos(Roll * PI / 180.0), sr = sin(Roll * PI / 180.0);
    ray = vec3(ray.x * cr - ray.y * sr, ray.x * sr + ray.y * cr, ray.z);

    // Tilt (X axis)
    float ct = cos(-Tilt * PI / 180.0), st = sin(-Tilt * PI / 180.0);
    ray = vec3(ray.x, ray.y * ct - ray.z * st, ray.y * st + ray.z * ct);

    // Pan (Y axis)
    float cp = cos(Pan * PI / 180.0), sp = sin(Pan * PI / 180.0);
    ray = vec3(ray.x * cp + ray.z * sp, ray.y, -ray.x * sp + ray.z * cp);

    // Front lens = top half (yOffset 0.5), rear lens = bottom half (yOffset 0.0)
    bool useFront = (Swap == 0) ? (ray.z >= 0.0) : (ray.z < 0.0);

    vec3 fisheyeRay;
    float yOffset;

    if (useFront) {
        fisheyeRay = ray;
        yOffset = 0.5;
    } else {
        // Mirror X to correct the rear lens horizontal flip
        fisheyeRay = vec3(-ray.x, ray.y, -ray.z);
        yOffset = 0.0;
    }

    float theta    = acos(clamp(fisheyeRay.z, -1.0, 1.0));
    float phi      = atan(fisheyeRay.y, fisheyeRay.x);
    float maxTheta = LensFOV * 0.5 * PI / 180.0;
    float r        = theta / maxTheta;

    if (r > 1.0) { fragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }

    // Each half is wide (HalfAspect >> 1), so the fisheye circle is
    // constrained by height.  radiusX corrects for non-square halves.
    float radiusX = 0.5 / HalfAspect;
    float radiusY = 0.5;

    vec2 sampleUV = vec2(
        0.5 + r * cos(phi) * radiusX,
        0.5 + r * sin(phi) * radiusY
    );

    // Map into the correct vertical half of the Over/Under frame
    sampleUV.y = sampleUV.y * 0.5 + yOffset;

    fragColor = texture(InputTexture, sampleUV);
}
)glsl";

X5FisheyeViewer::X5FisheyeViewer()
    : m_swap(0), m_pan(0.f), m_tilt(0.f), m_roll(0.f),
      m_fov(90.f), m_lensFOV(180.f), m_aspect(1.f)
{
    SetMinInputs(1);
    SetMaxInputs(1);
    SetParamInfo(PARAM_SWAP,    "Swap Lenses",  FF_TYPE_STANDARD, 0.0f);
    SetParamInfo(PARAM_PAN,     "Yaw",          FF_TYPE_STANDARD, 0.5f);
    SetParamInfo(PARAM_TILT,    "Pitch",        FF_TYPE_STANDARD, 0.5f);
    SetParamInfo(PARAM_ROLL,    "Roll",         FF_TYPE_STANDARD, 0.5f);
    SetParamInfo(PARAM_FOV,     "fov Out",      FF_TYPE_STANDARD, 0.5714f);
    SetParamInfo(PARAM_LENSFOV, "fov In",       FF_TYPE_STANDARD, 0.5f);
}

FFResult X5FisheyeViewer::InitGL(const FFGLViewportStruct* vp) {
    m_aspect = (vp->height > 0) ? (float)vp->width / vp->height : 1.f;
    if (!m_shader.Compile(kVert, kFrag)) return FF_FAIL;
    if (!m_quad.Initialise()) return FF_FAIL;
    return CFFGLPlugin::InitGL(vp);
}

FFResult X5FisheyeViewer::DeInitGL() {
    m_shader.FreeGLResources();
    m_quad.Release();
    return FF_SUCCESS;
}

FFResult X5FisheyeViewer::ProcessOpenGL(ProcessOpenGLStruct* pGL) {
    if (!pGL->numInputTextures || !pGL->inputTextures[0]) return FF_FAIL;
    FFGLTextureStruct& tex = *(pGL->inputTextures[0]);
    if (!tex.Handle) return FF_FAIL;

    float maxU       = tex.HardwareWidth  ? (float)tex.Width  / tex.HardwareWidth  : 1.f;
    float maxV       = tex.HardwareHeight ? (float)tex.Height / tex.HardwareHeight : 1.f;
    float halfAspect = (tex.Height > 0)   ? (float)tex.Width  / (tex.Height * 0.5f) : 1.f;

    ffglex::ScopedShaderBinding     shaderBinding(m_shader.GetGLID());
    ffglex::ScopedSamplerActivation samplerActivation(0);
    ffglex::Scoped2DTextureBinding  textureBinding(tex.Handle);

    m_shader.Set("InputTexture", 0);
    m_shader.Set("MaxUV",        maxU, maxV);
    m_shader.Set("AspectRatio",  m_aspect);
    m_shader.Set("HalfAspect",   halfAspect);
    m_shader.Set("Swap",         m_swap);
    m_shader.Set("Pan",          m_pan);
    m_shader.Set("Tilt",         m_tilt);
    m_shader.Set("Roll",         m_roll);
    m_shader.Set("FOV",          m_fov);
    m_shader.Set("LensFOV",      m_lensFOV);
    m_quad.Draw();

    return FF_SUCCESS;
}

FFResult X5FisheyeViewer::SetFloatParameter(unsigned int idx, float val) {
    switch (idx) {
        case PARAM_SWAP:    m_swap    = (val > 0.5f) ? 1 : 0; break;
        case PARAM_PAN:     m_pan     = (val - 0.5f) * 360.f; break;
        case PARAM_TILT:    m_tilt    = (val - 0.5f) * 180.f; break;
        case PARAM_ROLL:    m_roll    = (val - 0.5f) * 360.f; break;
        case PARAM_FOV:     m_fov     = val * 140.f + 10.f;   break;
        case PARAM_LENSFOV: m_lensFOV = val * 60.f  + 150.f;  break;
        default: return FF_FAIL;
    }
    return FF_SUCCESS;
}

float X5FisheyeViewer::GetFloatParameter(unsigned int idx) {
    switch (idx) {
        case PARAM_SWAP:    return (float)m_swap;
        case PARAM_PAN:     return m_pan     / 360.f + 0.5f;
        case PARAM_TILT:    return m_tilt    / 180.f + 0.5f;
        case PARAM_ROLL:    return m_roll    / 360.f + 0.5f;
        case PARAM_FOV:     return (m_fov     - 10.f)  / 140.f;
        case PARAM_LENSFOV: return (m_lensFOV - 150.f) / 60.f;
        default: return 0.f;
    }
}
