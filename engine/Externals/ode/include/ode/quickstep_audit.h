#ifndef ODE_QUICKSTEP_AUDIT_H
#define ODE_QUICKSTEP_AUDIT_H

#include <ode/common.h>
#include <stdint.h>

// Diagnostic-only observer. All pointers are borrowed for this callback only.
// No allocation, logging, file I/O or Core dependency is added to ODE.
enum dQuickStepAuditPhase
{
    dQuickStepAuditEntry = 0,
    dQuickStepAuditPreSOR = 1,
    dQuickStepAuditPostSOR = 2
};

struct dQuickStepAuditView
{
    dQuickStepAuditPhase phase;
    int nb, nj, m;
    dxBody* const* bodies;
    dxJoint* const* joints;
    // PRE/POST nj is the active compacted joint count. Number of rows for
    // joint i is offsets[i+1]-offsets[i], or m-offsets[i] for the final joint.
    const int* offsets;
    // PRE/POST only: the original borrowed dxJoint::Info1[nj] array. Kept
    // opaque here because the nested type is internal to ODE joint.h. An
    // ODE-aware consumer may cast it back to that exact type; never reinvoke
    // getInfo1/getInfo2 to reconstruct these already-computed values.
    const void* joint_info1;
    const dReal* invI; // Global-frame 3x4 per body; copy only the nine matrix entries.
    const dReal* J; // PRE only: m x 12, before SOR scales/overwrites it.
    const int* jb; // PRE only: m x 2, second body may be -1.
    const dReal* rhs; // PRE only, complete unscaled SOR RHS.
    const dReal* cfm; // PRE only, already scaled by inverse timestep.
    const dReal* lo;
    const dReal* hi;
    const int* findex;
    // PRE is null when warm_starting==0: that production stack array is then
    // uninitialized until SOR zeros it. Never read it; record explicit zeros.
    // Copy initialized joint->lambda[6] separately when recording raw caches.
    const dReal* lambda;
    const dReal* cforce; // POST only: nb x 6, inv(M) * J' * lambda.
    dReal stepsize, sor_w, erp, global_cfm;
    dReal gravity[3];
    int iterations, warm_starting;
    uint64_t rng_state;
};

struct dQuickStepAuditObserver
{
    void* context;
    void (*observe)(void* context, const dQuickStepAuditView* view);
};

// TLS opt-in scope. Returns the previous pointer for caller-side RAII restore.
// The observer and context must outlive the synchronous step on this thread.
// The callback must not throw, mutate solver inputs or invoke joint callbacks.
extern "C" ODE_API const dQuickStepAuditObserver* dSetQuickStepAuditObserver(
    const dQuickStepAuditObserver* observer) noexcept;

#endif
