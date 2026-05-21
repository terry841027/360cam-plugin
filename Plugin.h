#pragma once
#include <ffgl/FFGL.h>
#include <ffglex/FFGLShader.h>
#include <ffglex/FFGLScreenQuad.h>

enum ParamIndex {
    PARAM_LENS = 0,
    PARAM_PAN,
    PARAM_TILT,
    PARAM_ROLL,
    PARAM_FOV,
    PARAM_LENSFOV,
    PARAM_COUNT
};

class X5FisheyeViewer : public CFFGLPlugin {
public:
    X5FisheyeViewer();

    FFResult InitGL(const FFGLViewportStruct* vp) override;
    FFResult ProcessOpenGL(ProcessOpenGLStruct* pGL) override;
    FFResult DeInitGL() override;

    FFResult SetFloatParameter(unsigned int index, float value) override;
    float    GetFloatParameter(unsigned int index) override;

private:
    ffglex::FFGLShader     m_shader;
    ffglex::FFGLScreenQuad m_quad;

    int   m_lens;       // 0 = left, 1 = right
    float m_pan;        // degrees [-180, 180]
    float m_tilt;       // degrees [-90,   90]
    float m_roll;       // degrees [-180, 180]
    float m_fov;        // degrees [10,  150]
    float m_lensFOV;    // degrees [150, 210]
    float m_aspect;
};
