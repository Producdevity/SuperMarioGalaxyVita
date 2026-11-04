#include "Game/Effect/SyncBckEffectInfo.hpp"
#include "Game/Animation/XanimePlayer.hpp"
#include "Game/Animation/XanimeResource.hpp"
#include "Game/Util/StringUtil.hpp"

SyncBckEffectInfo::BckResourceInfo::BckResourceInfo(const XanimePlayer* pPlayer, const char* pName) : mName(pName), mResource() {
    mResource = static_cast< J3DAnmTransform* >(pPlayer->mResourceTable->findResMotion(pName));
}

bool SyncBckEffectInfo::BckResourceInfo::isLoop() const {
    return mResource->getAttribute() == J3DFrameCtrl::EMode_LOOP || mResource->getAttribute() == J3DFrameCtrl::EMode_LOOP_REVERSE;
}

SyncBckEffectInfo::SyncBckEffectInfo(const XanimePlayer* pPlayer, const char* pName, s32 capacity, f32 startFrame, f32 endFrame, bool continueAnimEnd)
    : mBckResources(), mCapacity(), mCount(), mStartFrame(startFrame), mEndFrame(endFrame), mContinueAnimEnd(continueAnimEnd) {
    mBckResources = new BckResourceInfo*[capacity];
    mCapacity = capacity;
    addBck(pPlayer, pName);
}

void SyncBckEffectInfo::addBck(const XanimePlayer* pPlayer, const char* pName) {
    BckResourceInfo* pInfo = new BckResourceInfo(pPlayer, pName);
    const s32 index = mCount++;
    mBckResources[index] = pInfo;
#ifdef TARGET_PC
    mPortLookupName = nullptr;
#endif
}

#ifdef TARGET_PC
s32 SyncBckEffectInfo::portFind(const char* pName) const {
    if (pName != mPortLookupName) {
        mPortLookupIndex = -1;
        for (s32 i = 0; i < mCount; i++) {
            if (MR::isEqualStringCase(pName, mBckResources[i]->mName)) {
                mPortLookupIndex = i;
                break;
            }
        }
        mPortLookupName = pName;
    }
    return mPortLookupIndex;
}
#endif

bool SyncBckEffectInfo::isRegisteredBck(const char* pName) const {
    if (pName == nullptr) {
        return false;
    }

#ifdef TARGET_PC
    return portFind(pName) >= 0;
#else
    for (BckResourceInfo** pInfo = mBckResources; pInfo != mBckResources + mCount; pInfo++) {
        if (MR::isEqualStringCase(pName, (*pInfo)->mName)) {
            return true;
        }
    }

    return false;
#endif
}

bool SyncBckEffectInfo::isBckLoop(const char* pName) const {
    if (pName == nullptr) {
        return false;
    }

#ifdef TARGET_PC
    const s32 index = portFind(pName);
    return index >= 0 && mBckResources[index]->isLoop();
#else
    BckResourceInfo* pResource = nullptr;

    for (BckResourceInfo** pInfo = mBckResources; pInfo != mBckResources + mCount; pInfo++) {
        if (MR::isEqualStringCase((*pInfo)->mName, pName)) {
            pResource = *pInfo;
            break;
        }
    }

    if (pResource == nullptr) {
        return false;
    }

    return pResource->isLoop();
#endif
}

namespace MR {
    namespace Effect {
        bool isExistSyncBckDeleteFrame(const SyncBckEffectInfo* pInfo) {
            const f32 endFrame = pInfo->mEndFrame;
            return 0.0f <= endFrame;
        }
    }  // namespace Effect
}  // namespace MR
