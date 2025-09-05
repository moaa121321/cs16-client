// NoRecoil.cpp
#include <math.h>
#include "hud.h"
#include "cl_util.h"
#include "cvardef.h"
#include "NoRecoil.h"

cvar_t *cl_norecoil = NULL;

// Vektör kopyalama fonksiyonu
static inline void VectorCopy(const float *src, float *dst) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

// Vektör uzunluğu hesaplama
static inline float VectorLength(const float *v) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

// Vektör ölçeklendirme
static inline void VectorScale(const float *in, float scale, float *out) {
    out[0] = in[0] * scale;
    out[1] = in[1] * scale;
    out[2] = in[2] * scale;
}

// Maximum değer bulma
static inline float max(float a, float b) {
    return a > b ? a : b;
}

void ApplyNoRecoil(float frametime, float *punchangle, float *viewangle) {
    if (!cl_norecoil || cl_norecoil->value == 0.0f)
        return;
    
    float punch[3], length;
    VectorCopy(punchangle, punch);
    length = VectorLength(punch);
    length -= (10.0f + length * 0.5f) * frametime;
    length = max(length, 0.0f);
    VectorScale(punch, length, punch);
    viewangle[0] += punch[0] * 2.0f;
    viewangle[1] += punch[1] * 2.0f;
}

void NoRecoil_Init(void) {
    cl_norecoil = CVAR_CREATE("cl_norecoil", "0", FCVAR_ARCHIVE);
}
