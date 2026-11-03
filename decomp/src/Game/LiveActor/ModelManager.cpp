#include "Game/LiveActor/ModelManager.hpp"
#include "Game/Animation/BckCtrl.hpp"
#include "Game/Animation/BpkPlayer.hpp"
#include "Game/Animation/BrkPlayer.hpp"
#include "Game/Animation/BtkPlayer.hpp"
#include "Game/Animation/BtpPlayer.hpp"
#include "Game/Animation/BvaPlayer.hpp"
#include "Game/Animation/XanimeCore.hpp"
#include "Game/Animation/XanimePlayer.hpp"
#include "Game/Animation/XanimeResource.hpp"
#include "Game/LiveActor/DisplayListMaker.hpp"
#include "Game/System/ResourceHolder.hpp"
#include "Game/Util/ModelUtil.hpp"
#include "Game/Util/MutexHolder.hpp"
#include "Game/Util/ObjUtil.hpp"
#include <JSystem/J3DGraphAnimator/J3DModel.hpp>
#include <cstdio>
#ifdef TARGET_PC
extern "C" int port_matdl_defer(void);  // (platform/src/port/perf.cpp)
extern "C" int port_matdl_may_defer(void);
extern "C" void port_matdl_late(unsigned models);
extern "C" void port_early_done_paid(int site, s64 ns);
extern "C" int GXPortPicturesUnread(void);  // (decomp/src/RVL_SDK/gx/GXMisc.c)
extern "C" int GXPictureRead(u32 picture);

namespace {
    // (a fixed array: `new` here allocates from the game's current heap, a scene's, which goes with the scene)
    ModelManager* sDeferredDL[4096];
    u32 sDeferredCount = 0;
    bool sMakingDeferred = false;  // (updateDeferredDL: the updates a picture is about to draw with)

    // (PETARI_EARLY_DONE) Whether DisplayListMaker::diff would wait now for the renderer to read a picture that drew from one of
    // the lists it writes.
    bool listsUnread(const DisplayListMaker* pMaker) {
        J3DModel* pModel = pMaker->mModel;
        for (u16 i = 0; i < pModel->getModelData()->getMaterialNum(); i++) {
            if (pMaker->mCurFlag[i] && !GXPictureRead(pModel->getMatPacket(i)->mpInitShapePacket->getDisplayListObj()->portWriteReader())) {
                return true;
            }
        }
        return false;
    }
}

void ModelManager::updateDeferredDL() {
    unsigned late = 0;
    sMakingDeferred = true;
    for (u32 i = 0; i < sDeferredCount; i++) {
        if (sDeferredDL[i]->mDLDeferred) {
            sDeferredDL[i]->updateDL(true);
            ++late;
        }
    }
    sMakingDeferred = false;
    sDeferredCount = 0;
    if (late) {
        port_matdl_late(late);
    }
}

void ModelManager::dropDeferredDL() {
    for (u32 i = 0; i < sDeferredCount; i++) {
        sDeferredDL[i]->mDLDeferred = false;
    }
    sDeferredCount = 0;
}
#endif

ModelManager::ModelManager()
    : mBtkPlayer(), mBrkPlayer(), mBtpPlayer(), mBpkPlayer(), mBvaPlayer(), mXanimeResourceTable(), mXanimePlayer(), mModel(), mModelResourceHolder(),
      mDisplayListMaker() {
}

void ModelManager::update() {
    XanimePlayer* pXanimePlayer = mXanimePlayer;

    if (pXanimePlayer != nullptr) {
        pXanimePlayer->updateBeforeMovement();
        pXanimePlayer->updateAfterMovement();
    }

    if (mBtkPlayer != nullptr) {
        mBtkPlayer->update();
    }

    if (mBrkPlayer != nullptr) {
        mBrkPlayer->update();
    }

    if (mBtpPlayer != nullptr) {
        mBtpPlayer->update();
    }

    if (mBpkPlayer != nullptr) {
        mBpkPlayer->update();
    }

    if (mBvaPlayer != nullptr) {
        mBvaPlayer->update();
    }
}

void ModelManager::calcAnim() {
    calc();
#ifdef TARGET_PC
    // A turn without a picture whose next turn has none either: that turn writes the material display lists again before
    // anything is drawn with them, so this update would never be seen (port_matdl_defer). If no later turn makes it, the
    // next drawn turn does, before it draws. A model's first update is always made (what it sets up may be read sooner).
    if (mDLMade && (mDLDeferred || sDeferredCount < sizeof(sDeferredDL) / sizeof(*sDeferredDL)) && port_matdl_defer()) {
        if (!mDLDeferred) {
            mDLDeferred = true;
            sDeferredDL[sDeferredCount++] = this;
        }
        return;
    }
#endif
    updateDL(true);
}

void ModelManager::calcView() {
    getJ3DModel()->viewCalc();
}

void ModelManager::entry() {
    getJ3DModel()->entry();
}

void ModelManager::newDifferedDLBuffer() {
    mDisplayListMaker->newDifferedDisplayList();
}

void ModelManager::updateDL(bool isDiffDL) {
#ifdef TARGET_PC
    if (isDiffDL) {
        // (PETARI_EARLY_DONE) An update whose lists' write would wait for the renderer to read the last picture is left to the
        // next drawn turn, as above: by then the picture has usually been read. Every caller (calcAnim, MarioActor::calcAnim's
        // MR::updateModelDiffDL, ...) runs after the picture of its turn is drawn.
        if (!sMakingDeferred && mDLMade && (mDLDeferred || sDeferredCount < sizeof(sDeferredDL) / sizeof(*sDeferredDL)) &&
            mDisplayListMaker != nullptr && GXPortPicturesUnread() && port_matdl_may_defer() && listsUnread(mDisplayListMaker)) {
            if (!mDLDeferred) {
                mDLDeferred = true;
                sDeferredDL[sDeferredCount++] = this;
            }
            port_early_done_paid(7, 0);
            return;
        }
        mDLDeferred = false;
        mDLMade = true;
    }
#endif
    OSLockMutex(&MR::MutexHolder< 0 >::sMutex);

    bool isUpdateDL = mDisplayListMaker != nullptr && mDisplayListMaker->isValidDiff();

    if (isUpdateDL) {
        mDisplayListMaker->update();

        if (mBtkPlayer != nullptr) {
            mBtkPlayer->beginDiff();
        }

        if (mBrkPlayer != nullptr) {
            mBrkPlayer->beginDiff();
        }

        if (mBtpPlayer != nullptr) {
            mBtpPlayer->beginDiff();
        }

        if (mBpkPlayer != nullptr) {
            mBpkPlayer->beginDiff();
        }
    }

    getJ3DModel()->calcMaterial();

    if (isUpdateDL) {
        if (isDiffDL) {
            mDisplayListMaker->diff();
        }

        if (mBtkPlayer != nullptr) {
            mBtkPlayer->endDiff();
        }

        if (mBrkPlayer != nullptr) {
            mBrkPlayer->endDiff();
        }

        if (mBtpPlayer != nullptr) {
            mBtpPlayer->endDiff();
        }

        if (mBpkPlayer != nullptr) {
            mBpkPlayer->endDiff();
        }
    }

    OSUnlockMutex(&MR::MutexHolder< 0 >::sMutex);
}

void ModelManager::startBck(const char* pBckName, const char* pActorAnimName) {
    mXanimePlayer->changeAnimationBck(pBckName);
    mXanimePlayer->changeInterpoleFrame(0);
    changeBckSetting(pBckName, pActorAnimName, nullptr);
}

void ModelManager::startBckWithInterpole(const char* pBckName, s32 interpole) {
    mXanimePlayer->changeAnimationBck(pBckName);
    mXanimePlayer->changeInterpoleFrame(0);
    mXanimePlayer->changeInterpoleFrame(interpole);
}

void ModelManager::startBtk(const char* pBtkName) {
    stopBtk();
    mBtkPlayer->start(pBtkName);
    mDisplayListMaker->onCurFlagBtk(mBtkPlayer->mAnmRes);
}

void ModelManager::startBrk(const char* pBrkName) {
    stopBrk();
    mBrkPlayer->start(pBrkName);
    mDisplayListMaker->onCurFlagBrk(mBrkPlayer->mAnmRes);
}

void ModelManager::startBtp(const char* pBtpName) {
    stopBtp();
    mBtpPlayer->start(pBtpName);
    mDisplayListMaker->onCurFlagBtp(mBtpPlayer->mAnmRes);
}

void ModelManager::startBpk(const char* pBpkName) {
    stopBpk();
    mBpkPlayer->start(pBpkName);
    mDisplayListMaker->onCurFlagBpk(mBpkPlayer->mAnmRes);
}

void ModelManager::startBva(const char* pBvaName) {
    stopBva();
    mBvaPlayer->start(pBvaName);
}

void ModelManager::stopBtk() {
    if (mBtkPlayer->mAnmRes != nullptr) {
        mDisplayListMaker->offCurFlagBtk(mBtkPlayer->mAnmRes);
        mBtkPlayer->stop();
    }
}

void ModelManager::stopBrk() {
    if (mBrkPlayer->mAnmRes != nullptr) {
        mDisplayListMaker->offCurFlagBrk(mBrkPlayer->mAnmRes);
        mBrkPlayer->stop();
    }
}

void ModelManager::stopBtp() {
    if (mBtpPlayer->mAnmRes != nullptr) {
        mDisplayListMaker->offCurFlagBtp(mBtpPlayer->mAnmRes);
        mBtpPlayer->stop();
    }
}

void ModelManager::stopBpk() {
    if (mBpkPlayer->mAnmRes != nullptr) {
        mDisplayListMaker->offCurFlagBpk(mBpkPlayer->mAnmRes);
        mBpkPlayer->stop();
    }
}

void ModelManager::stopBva() {
    if (mBvaPlayer->mAnmRes != nullptr) {
        mBvaPlayer->stop();
    }
}

J3DFrameCtrl* ModelManager::getBckCtrl() const {
    return mXanimePlayer->_20;
}

J3DFrameCtrl* ModelManager::getBtkCtrl() const {
    return &mBtkPlayer->mFrameCtrl;
}

J3DFrameCtrl* ModelManager::getBrkCtrl() const {
    return &mBrkPlayer->mFrameCtrl;
}

J3DFrameCtrl* ModelManager::getBtpCtrl() const {
    return &mBtpPlayer->mFrameCtrl;
}

J3DFrameCtrl* ModelManager::getBpkCtrl() const {
    return &mBpkPlayer->mFrameCtrl;
}

J3DFrameCtrl* ModelManager::getBvaCtrl() const {
    return &mBvaPlayer->mFrameCtrl;
}

bool ModelManager::isBckStopped() const {
    return mXanimePlayer->_24[mXanimePlayer->_54].checkState(1);
}

bool ModelManager::isBtkStopped() const {
    if (mBtkPlayer != nullptr) {
        return mBtkPlayer->isStop();
    }

    return false;
}

bool ModelManager::isBrkStopped() const {
    if (mBrkPlayer != nullptr) {
        return mBrkPlayer->isStop();
    }

    return false;
}

bool ModelManager::isBtpStopped() const {
    if (mBtpPlayer != nullptr) {
        return mBtpPlayer->isStop();
    }

    return false;
}

bool ModelManager::isBpkStopped() const {
    if (mBpkPlayer != nullptr) {
        return mBpkPlayer->isStop();
    }

    return false;
}

bool ModelManager::isBvaStopped() const {
    if (mBvaPlayer != nullptr) {
        return mBvaPlayer->isStop();
    }

    return false;
}

bool ModelManager::isBtkPlaying(const char* pBtkName) const {
    return mBtkPlayer->isPlaying(pBtkName);
}

bool ModelManager::isBrkPlaying(const char* pBrkName) const {
    return mBrkPlayer->isPlaying(pBrkName);
}

bool ModelManager::isBpkPlaying(const char* pBpkName) const {
    return mBpkPlayer->isPlaying(pBpkName);
}

bool ModelManager::isBtpPlaying(const char* pBtpName) const {
    return mBtpPlayer->isPlaying(pBtpName);
}

bool ModelManager::isBvaPlaying(const char* pBvaName) const {
    return mBvaPlayer->isPlaying(pBvaName);
}

void ModelManager::initJointTransform() {
    mXanimePlayer->mCore->enableJointTransform(getJ3DModelData());
}

XjointTransform* ModelManager::getJointTransform(const char* pJointName) {
    s32 index = getJ3DModel()->getModelData()->getJointName()->getIndex(pJointName);
    return mXanimePlayer->mCore->getJointTransform(index);
}

ResourceHolder* ModelManager::getResourceHolder() const {
    if (mXanimeResourceTable == nullptr) {
        return mModelResourceHolder;
    }

    return mXanimeResourceTable->mResourceHolder;
}

ResourceHolder* ModelManager::getModelResourceHolder() const {
    return mModelResourceHolder;
}

J3DModel* ModelManager::getJ3DModel() const {
    if (mXanimePlayer == nullptr) {
        return mModel;
    }

    return mXanimePlayer->mModel;
}

J3DModelData* ModelManager::getJ3DModelData() const {
    return getJ3DModel()->mModelData;
}

const char* ModelManager::getPlayingBckName() const {
    if (mXanimePlayer != nullptr) {
        return mXanimePlayer->getCurrentBckName();
    }

    return nullptr;
}

void ModelManager::initModelAndAnimation(ResourceHolder* pModelHolder, const char* pModelName, ResourceHolder* pAnimationHolder, J3DMdlFlag flags) {
    mModelResourceHolder = pModelHolder;
    if (pAnimationHolder->mMotionResTable->mCount == 0) {
        mModel = MR::newJ3DModel(pModelHolder, pModelName, flags);
    } else {
        mXanimeResourceTable = MR::newXanimeResourceTable(pAnimationHolder);
        mXanimePlayer = MR::newXanimePlayer(pModelHolder, pModelName, pAnimationHolder, flags, mXanimeResourceTable);
    }
}

void ModelManager::initMaterialAnm() {
    ResourceHolder* pResourceHolder = getResourceHolder();
    J3DModelData* pModelData = getJ3DModelData();

    if (pResourceHolder->mMaterialBuf == nullptr) {
        pResourceHolder->newMaterialAnmBuffer(pModelData);
    }

    if (pResourceHolder->mBtkResTable->mCount != 0) {
        mBtkPlayer = new BtkPlayer(pResourceHolder, pModelData);
    }

    if (pResourceHolder->mBrkResTable->mCount != 0) {
        mBrkPlayer = new BrkPlayer(pResourceHolder, pModelData);
    }

    if (pResourceHolder->mBtpResTable->mCount != 0) {
        mBtpPlayer = new BtpPlayer(pResourceHolder, pModelData);
    }

    if (pResourceHolder->mBpkResTable->mCount != 0) {
        mBpkPlayer = new BpkPlayer(pResourceHolder, pModelData);
    }
}

void ModelManager::initVisibilityAnm() {
    ResourceHolder* pResourceHolder = getResourceHolder();

    if (pResourceHolder->mBvaResTable->mCount != 0) {
        mBvaPlayer = new BvaPlayer(pResourceHolder->getBvaResTable(), getJ3DModel());
    }
}

void ModelManager::calc() {
    if (mXanimePlayer != nullptr) {
        mXanimePlayer->calcAnm(0);
    }

    if (mBvaPlayer != nullptr) {
        mBvaPlayer->calc();
    }

    OSLockMutex(&MR::MutexHolder< 0 >::sMutex);
    getJ3DModel()->calc();
    OSUnlockMutex(&MR::MutexHolder< 0 >::sMutex);

    if (mXanimePlayer != nullptr) {
        mXanimePlayer->clearAnm(0);
    }
}

void ModelManager::changeBckSetting(const char* pBckName, const char* pActorAnimName, XanimePlayer* pXanimePlayer) {
    if (pXanimePlayer == nullptr) {
        pXanimePlayer = mXanimePlayer;
    }

    BckCtrl* pBckCtrl = getResourceHolder()->mBckCtrl;

    if (pBckCtrl != nullptr) {
        if (pActorAnimName == nullptr) {
            pActorAnimName = pBckName;
        }

        pBckCtrl->changeBckSetting(pActorAnimName, pXanimePlayer);
    }
}

void ModelManager::init(const char* pModelName, const char* pAnimationName, bool deferDisplayList) {
    char modelArchive[64];
    snprintf(modelArchive, sizeof(modelArchive), "%s.arc", pModelName);
    ResourceHolder* modelHolder = MR::createAndAddResourceHolder(modelArchive);
    ResourceHolder* animationHolder;
    if (pAnimationName == nullptr) {
        animationHolder = modelHolder;
    } else {
        char animationArchive[64];
        snprintf(animationArchive, sizeof(animationArchive), "%s.arc", pAnimationName);
        animationHolder = MR::createAndAddResourceHolder(animationArchive);
    }

    bool hasMaterialAnm = animationHolder->isExistMaterialAnm();
    bool hasDiffMaterial = DisplayListMaker::isExistDiffMaterial(static_cast< J3DModelData* >(modelHolder->mModelResTable->getRes(pModelName)));
    J3DMdlFlag flags = J3DMdlFlag_UseSharedDL;
    if (deferDisplayList || hasMaterialAnm || hasDiffMaterial) {
        flags = static_cast< J3DMdlFlag >(flags | J3DMdlFlag_DifferedDLBuffer);
    }

    initModelAndAnimation(modelHolder, pModelName, animationHolder, flags);
    initVisibilityAnm();
    if (hasMaterialAnm) {
        initMaterialAnm();
    }

    if (flags & J3DMdlFlag_DifferedDLBuffer) {
        mDisplayListMaker = new DisplayListMaker(getJ3DModel(), getResourceHolder());
        if (!deferDisplayList) {
            mDisplayListMaker->newDifferedDisplayList();
        }
    }
}
