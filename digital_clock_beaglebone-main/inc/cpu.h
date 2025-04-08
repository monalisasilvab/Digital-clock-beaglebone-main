#ifndef	CPU_H_
#define CPU_H_

#ifdef __cplusplus
extern "C" {
#endif
/*****************************************************************************
**                           FUNCTION PROTOTYPES
*****************************************************************************/
void CPUSwitchToUserMode(void);
void CPUSwitchToPrivilegedMode(void);

/****************************************************************************/
/*
** Functions used internally
*/
void CPUAbortHandler(void);
void CPUirqd(void);
void CPUirqe(void);
void CPUfiqd(void);
void CPUfiqe(void);
unsigned int CPUIntStatus(void);

#ifdef __cplusplus
}
#endif
#endif /* CPU_H_ */