#pragma once
#include <FFGLSDK.h>

enum ParamIndex {
    PARAM_SWAP = 0,
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
    int   m_swap;
    float m_pan;
    float m_tilt;
    float m_roll;
    float m_fov;
    float m_lensFOV;
    float m_aspect;
};
