#include "Game/Gravity.hpp"
#include "Game/Util.hpp"
#ifdef TARGET_PC
#include <cstring>
#endif

void PlanetGravity_FORCE_MATCH_SDATA2() {
    (void)1.0f;
    (void)0.0f;
}

PlanetGravity::PlanetGravity() {
    mRange = -1.0f;
    mDistant = 0.0f;
    mPriority = 0;
    mGravityId = -1;
    mHost = this;
    mGravityType = GRAVITY_TYPE_NORMAL;
    mGravityPower = GRAVITY_POWER_NORMAL;
    mActivated = true;
    mIsInverse = false;
    mValidFollower = true;
    mIsRegistered = false;
    mAppeared = true;
}

void PlanetGravity::setPriority(s32 priority) {
    mPriority = priority;
}

#ifdef TARGET_PC
extern "C" int port_gravity_cull_mode(void);  // (platform/src/port/perf.cpp)
extern "C" void port_gravity_cull_verified(int rejectedEarly, int mismatching);

bool PlanetGravity::sOutOfReachTest = false;
bool PlanetGravity::sOutOfReachHit = false;

namespace {
    s32 sCullMode = -1;

    // PETARI_GRAVITY_CULL=2: each field evaluated without and with isOutOfReach; the results must be the same, bit for bit.
    bool calcOwnGravityVectorVerified(const PlanetGravity* pGravity, TVec3f* pDest, f32* pScalar, const TVec3f& rPosition) {
        TVec3f gravity = *pDest;
        f32 scalar = *pScalar;
        PlanetGravity::sOutOfReachTest = false;
        const bool inReach = pGravity->calcOwnGravityVector(pDest, pScalar, rPosition);
        PlanetGravity::sOutOfReachTest = true;
        PlanetGravity::sOutOfReachHit = false;
        const bool tested = pGravity->calcOwnGravityVector(&gravity, &scalar, rPosition);
        PlanetGravity::sOutOfReachTest = false;
        const bool same = inReach == tested && (!inReach || (memcmp(&gravity, pDest, sizeof(gravity)) == 0 && memcmp(&scalar, pScalar, sizeof(scalar)) == 0));
        port_gravity_cull_verified(PlanetGravity::sOutOfReachHit, !same);
        return inReach;
    }
}  // namespace
#endif

bool PlanetGravity::calcGravity(TVec3f* pDest, const TVec3f& rPosition) const {
    // Calculate raw gravity vector
    f32 radius = 0.0f;
    TVec3f gravity;
    gravity.x = 0.0f;
    gravity.y = 0.0f;
    gravity.z = 0.0f;

#ifdef TARGET_PC
    if (sCullMode < 0) {
        sCullMode = port_gravity_cull_mode();
        sOutOfReachTest = sCullMode == 1;
    }
    if (sCullMode == 2) {
        if (!calcOwnGravityVectorVerified(this, &gravity, &radius, rPosition))
            return false;
    } else
#endif
    if (!calcOwnGravityVector(&gravity, &radius, rPosition))
        return false;

    // Adjust radius
    radius -= mDistant;

    if (radius < 1.0f) {
        radius = 1.0f;
    }

    // Apply gravity speed
    f32 scalar = 4000000.0f / (radius * radius);
    gravity.x *= scalar;
    gravity.y *= scalar;
    gravity.z *= scalar;

    // Invert vector if necessary
    if (mIsInverse) {
        gravity = -gravity;
    }

    // Set result vector
    pDest->x = gravity.x;
    pDest->y = gravity.y;
    pDest->z = gravity.z;

    return true;
}

bool PlanetGravity::isInRangeSquare(f32 radius) const {
    f32 range = mRange;

    if (range < 0.0f) {
        return true;
    } else {
        f32 distance = range + mDistant;
        return radius < distance * distance;
    }
}

bool PlanetGravity::isInRangeDistance(f32 radius) const {
    f32 range = mRange;

    if (range < 0.0f) {
        return true;
    } else {
        f32 distance = range + mDistant;
        return radius < distance;
    }
}

bool PlanetGravity::calcGravityFromMassPosition(TVec3f* pDirection, f32* pScalar, const TVec3f& rPosition, const TVec3f& rMassPosition) const {
    TVec3f direction = rMassPosition - rPosition;
    f32 scalar;

    MR::separateScalarAndDirection(&scalar, &direction, direction);

    if (!isInRangeDistance(scalar))
        return false;

    if (pDirection) {
        *pDirection = direction;
    }
    if (pScalar) {
        *pScalar = scalar;
    }

    return true;
}

void PlanetGravity::updateMtx(const TPos3f& rMtx) {
}

void PlanetGravity::updateIdentityMtx() {
    TPos3f mtx;
    mtx.identity();
    updateMtx(mtx);
}
