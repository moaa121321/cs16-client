// NoRecoil.h
#ifndef __NORECOIL_H__
#define __NORECOIL_H__

#ifdef __cplusplus
extern "C" {
#endif

extern cvar_t *cl_norecoil;
void ApplyNoRecoil(float frametime, float *punchangle, float *viewangle);
void NoRecoil_Init(void);

#ifdef __cplusplus
}
#endif

#endif
