/*
 *
 *    Copyright (c) 2021-2022 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "AndroidOperationalCredentialsIssuer.h"
#include "AndroidDeviceControllerWrapper.h"
#include <algorithm>
#include <credentials/CHIPCert.h>
#include <credentials/DeviceAttestationConstructor.h>
#include <lib/core/CASEAuthTag.h>
#include <lib/core/CHIPTLV.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/PersistentStorageMacros.h>
#include <lib/support/SafeInt.h>
#include <lib/support/ScopedBuffer.h>
#include <lib/support/TestGroupData.h>

#include <lib/support/CHIPJNIError.h>
#include <lib/support/JniReferences.h>
#include <lib/support/JniTypeWrappers.h>

#include <crypto/PersistentStorageOperationalKeystore.h>
#include <lib/support/DefaultStorageKeyAllocator.h>
#include <lib/support/Span.h>

namespace chip {
namespace Controller {
constexpr const char kOperationalCredentialsIssuerKeypairStorage[]   = "AndroidDeviceControllerKey";
constexpr const char kOperationalCredentialsRootCertificateStorage[] = "AndroidCARootCert";
constexpr const char kOperationalCredentialsICACStorage[] = "AndroidICAC";

using namespace Credentials;
using namespace Crypto;
using namespace TLV;

static CHIP_ERROR N2J_CSRInfo(JNIEnv * env, jbyteArray nonce, jbyteArray elements, jbyteArray elementsSignature, jbyteArray csr,
                              jobject & outCSRInfo);

static CHIP_ERROR N2J_AttestationInfo(JNIEnv * env, jbyteArray challenge, jbyteArray nonce, jbyteArray elements,
                                      jbyteArray elementsSignature, jbyteArray dac, jbyteArray pai, jbyteArray cd,
                                      jbyteArray firmwareInfo, jobject & outAttestationInfo);

CHIP_ERROR AndroidOperationalCredentialsIssuer::getPhoneCertCSR(PersistentStorageDelegate & storage, jobject javaObjectRef)
{
    //    tianhang code begin
    ChipLogProgress(Controller, "AndroidOperationalCredentialsIssuer getPhoneCertCSR enter");
    PersistentStorageOperationalKeystore opKeystore;
    CHIP_ERROR err = opKeystore.Init(&storage);

    FabricIndex kFabricIndex    = 111;

    // Failure before Init of ActivateOpKeypairForFabric
    P256PublicKey placeHolderPublicKey;
    err = opKeystore.ActivateOpKeypairForFabric(kFabricIndex, placeHolderPublicKey);
    ChipLogProgress(Controller, "ActivateOpKeypairForFabric result %d", err == CHIP_ERROR_INCORRECT_STATE);

    // Failure before Init of NewOpKeypairForFabric
    uint8_t unusedCsrBuf[kMAX_CSR_Length];
    MutableByteSpan unusedCsrSpan{ unusedCsrBuf };
    err = opKeystore.NewOpKeypairForFabric(kFabricIndex, unusedCsrSpan);
    ChipLogProgress(Controller, "NewOpKeypairForFabric result %d", err == CHIP_ERROR_INCORRECT_STATE);

    ChipLogProgress(Controller, "NewOpKeypairForFabric scr2:");
    ChipLogByteSpan(Controller, unusedCsrSpan);

    jbyteArray javaCsr;
    JniReferences::GetInstance().GetEnvForCurrentThread()->ExceptionClear();
    JniReferences::GetInstance().N2J_ByteArray(JniReferences::GetInstance().GetEnvForCurrentThread(), unusedCsrSpan.data(),
                                           unusedCsrSpan.size(), javaCsr);
    jmethodID method;
    err            = JniReferences::GetInstance().FindMethod(JniReferences::GetInstance().GetEnvForCurrentThread(), javaObjectRef,
                                                  "onPhoneCSRGetComplete", "([B)V", &method);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogProgress(Controller, "Error invoking onOpCSRGenerationComplete 0: %" CHIP_ERROR_FORMAT, err.Format());
    }
    ChipLogProgress(Controller, "CallVoidMethod start");
    JNIEnv * env   = JniReferences::GetInstance().GetEnvForCurrentThread();
    env->CallVoidMethod(javaObjectRef, method, javaCsr);
    ChipLogProgress(Controller, "CallVoidMethod end");

//    tianhang code end
    return CHIP_NO_ERROR;
}

CHIP_ERROR AndroidOperationalCredentialsIssuer::Initialize(PersistentStorageDelegate & storage, AutoCommissioner * autoCommissioner,
                                                           jobject javaObjectRef)
{
    ChipLogProgress(Controller, "AndroidOperationalCredentialsIssuer Initialize enter1");
    using namespace ASN1;
    ASN1UniversalTime effectiveTime;

    // Initializing the default start validity to start of 2021. The default validity duration is 10 years.
    CHIP_ZERO_AT(effectiveTime);
    effectiveTime.Year  = 2021;
    effectiveTime.Month = 6;
    effectiveTime.Day   = 10;
    ReturnErrorOnFailure(ASN1ToChipEpochTime(effectiveTime, mNow));

    Crypto::P256SerializedKeypair serializedKey;
    uint16_t keySize = static_cast<uint16_t>(sizeof(serializedKey));

    if (storage.SyncGetKeyValue(kOperationalCredentialsIssuerKeypairStorage, &serializedKey, keySize) != CHIP_NO_ERROR)
    {
        // Storage doesn't have an existing keypair. Let's create one and add it to the storage.
        ReturnErrorOnFailure(mIssuer.Initialize());
        ReturnErrorOnFailure(mIssuer.Serialize(serializedKey));

        keySize = static_cast<uint16_t>(sizeof(serializedKey));
        ChipLogProgress(Controller, "AndroidOperationalCredentialsIssuer set new");
        ReturnErrorOnFailure(storage.SyncSetKeyValue(kOperationalCredentialsIssuerKeypairStorage, &serializedKey, keySize));
    }
    else
    {
        // Use the keypair from the storage
        ReturnErrorOnFailure(mIssuer.Deserialize(serializedKey));
    }

    mStorage          = &storage;
    mAutoCommissioner = autoCommissioner;
    mJavaObjectRef    = javaObjectRef;

    //    tianhang code begin

//    PersistentStorageOperationalKeystore opKeystore;
//    CHIP_ERROR err = opKeystore.Init(&storage);
//
//    FabricIndex kFabricIndex    = 111;
//
//    // Failure before Init of ActivateOpKeypairForFabric
//    P256PublicKey placeHolderPublicKey;
//    err = opKeystore.ActivateOpKeypairForFabric(kFabricIndex, placeHolderPublicKey);
//    ChipLogProgress(Controller, "ActivateOpKeypairForFabric result %d", err == CHIP_ERROR_INCORRECT_STATE);
//
//    // Failure before Init of NewOpKeypairForFabric
//    uint8_t unusedCsrBuf[kMAX_CSR_Length];
//    MutableByteSpan unusedCsrSpan{ unusedCsrBuf };
//    err = opKeystore.NewOpKeypairForFabric(kFabricIndex, unusedCsrSpan);
//    ChipLogProgress(Controller, "NewOpKeypairForFabric result %d", err == CHIP_ERROR_INCORRECT_STATE);
//
//    ChipLogProgress(Controller, "NewOpKeypairForFabric scr2:");
//    ChipLogByteSpan(Controller, unusedCsrSpan);
//
//    jbyteArray javaCsr;
//    JniReferences::GetInstance().GetEnvForCurrentThread()->ExceptionClear();
//    JniReferences::GetInstance().N2J_ByteArray(JniReferences::GetInstance().GetEnvForCurrentThread(), unusedCsrSpan.data(),
//                                           unusedCsrSpan.size(), javaCsr);
//    jmethodID method;
//    err            = JniReferences::GetInstance().FindMethod(JniReferences::GetInstance().GetEnvForCurrentThread(), mJavaObjectRef,
//                                                  "onOpCSRGenerationComplete", "([B)V", &method);
//    if (err != CHIP_NO_ERROR)
//    {
//        ChipLogProgress(Controller, "Error invoking onOpCSRGenerationComplete 0: %" CHIP_ERROR_FORMAT, err.Format());
//    }
//    ChipLogProgress(Controller, "CallVoidMethod start");
//    JNIEnv * env   = JniReferences::GetInstance().GetEnvForCurrentThread();
//    env->CallVoidMethod(mJavaObjectRef, method, javaCsr);
//    ChipLogProgress(Controller, "CallVoidMethod end");


//    tianhang code end

    mInitialized = true;
    return CHIP_NO_ERROR;
}

CHIP_ERROR doJavaPrintCert(const char * methodName, MutableByteSpan & rcac)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR AndroidOperationalCredentialsIssuer::GenerateNOCChainAfterValidation(NodeId nodeId, FabricId fabricId,
                                                                                const CATValues & cats,
                                                                                const Crypto::P256PublicKey & pubkey,
                                                                                MutableByteSpan & rcac, MutableByteSpan & icac,
                                                                                MutableByteSpan & noc)
{
    ChipDN rcac_dn;
    uint16_t rcacBufLen = static_cast<uint16_t>(std::min(rcac.size(), static_cast<size_t>(UINT16_MAX)));
    CHIP_ERROR err      = CHIP_NO_ERROR;
    PERSISTENT_KEY_OP(fabricId, kOperationalCredentialsRootCertificateStorage, key,
                      err = mStorage->SyncGetKeyValue(key, rcac.data(), rcacBufLen));
    ChipLogProgress(Controller, "mIssuerId: %u, fabricId: %" PRIu64 ", nodeId: %" PRIu64 " ", mIssuerId, fabricId, nodeId);
    ByteSpan pkBS(pubkey.ConstBytes(), pubkey.Length());
    ChipLogProgress(Controller, "pubkey:");
    ChipLogByteSpan(Controller, pkBS);

    if (err == CHIP_NO_ERROR)
    {
        uint64_t rcacId;
        // Found root certificate in the storage.
        rcac.reduce_size(rcacBufLen);
        ReturnErrorOnFailure(ExtractSubjectDNFromX509Cert(rcac, rcac_dn));
        ReturnErrorOnFailure(rcac_dn.GetCertChipId(rcacId));
        ChipLogProgress(Controller, "rcacId: %" PRIu64 " ", rcacId);
        //VerifyOrReturnError(rcacId == mIssuerId, CHIP_ERROR_INTERNAL);
    }
    // If root certificate not found in the storage, generate new root certificate.
    else
    {
        ReturnErrorOnFailure(rcac_dn.AddAttribute_MatterRCACId(mIssuerId));

        ChipLogProgress(Controller, "Generating RCAC");
        chip::Credentials::X509CertRequestParams rcac_request = { 0, mNow, mNow + mValidity, rcac_dn, rcac_dn };
        ReturnErrorOnFailure(NewRootX509Cert(rcac_request, mIssuer, rcac));

        VerifyOrReturnError(CanCastTo<uint16_t>(rcac.size()), CHIP_ERROR_INTERNAL);
        PERSISTENT_KEY_OP(fabricId, kOperationalCredentialsRootCertificateStorage, key,
                          ReturnErrorOnFailure(mStorage->SyncSetKeyValue(key, rcac.data(), static_cast<uint16_t>(rcac.size()))));
    }
    doJavaCertPrint("rcac", rcac);

    //icac.reduce_size(0);
    //read icac form storage
    ChipDN icac_dn;
    uint16_t icacBufLen = static_cast<uint16_t>(std::min(icac.size(), static_cast<size_t>(UINT16_MAX)));
    err      = CHIP_NO_ERROR;
//    ChipLogProgress(Controller, "sync befor icac:");
//    ChipLogByteSpan(Controller, icac);
    PERSISTENT_KEY_OP(fabricId, kOperationalCredentialsICACStorage, key,
                      err = mStorage->SyncGetKeyValue(key, icac.data(), icacBufLen));
    ChipLogProgress(Controller, "Read ICAC result: %d", err.AsInteger());
    ChipLogProgress(Controller, "sync after icac:");
    ChipLogByteSpan(Controller, icac);
    if (err == CHIP_NO_ERROR)
    {
        ChipLogProgress(Controller, "Read ICAC successfully");
        uint64_t icacId;
        // Found icac in the storage.
        icac.reduce_size(icacBufLen);
        ReturnErrorOnFailure(ExtractSubjectDNFromX509Cert(icac, icac_dn));
        ReturnErrorOnFailure(icac_dn.GetCertChipId(icacId));
        ChipLogProgress(Controller, "icacId: %" PRIu64 " ", icacId);
        mIssuerId = icacId;
        //VerifyOrReturnError(icacId == mIssuerId, CHIP_ERROR_INTERNAL);
    }
    doJavaCertPrint("icac", icac);

    ChipDN noc_dn;
    ReturnErrorOnFailure(noc_dn.AddAttribute_MatterFabricId(fabricId));
    ReturnErrorOnFailure(noc_dn.AddAttribute_MatterNodeId(nodeId));
    ReturnErrorOnFailure(noc_dn.AddCATs(cats));

    ChipLogProgress(Controller, "Generating NOC");
    chip::Credentials::X509CertRequestParams noc_request = { 1, mNow, mNow + mValidity, noc_dn, icac_dn };
    NewNodeOperationalX509Cert(noc_request, pubkey, mIssuer, noc);

    doJavaCertPrint("noc", noc);

    return CHIP_NO_ERROR;
}

void AndroidOperationalCredentialsIssuer::doJavaCertPrint(const char* bytes, MutableByteSpan & cert)
{
    JNIEnv * env   = JniReferences::GetInstance().GetEnvForCurrentThread();
    jbyteArray javaArr;
    JniReferences::GetInstance().GetEnvForCurrentThread()->ExceptionClear();
    JniReferences::GetInstance().N2J_ByteArray(JniReferences::GetInstance().GetEnvForCurrentThread(), cert.data(),
                                           cert.size(), javaArr);
    jobject debugText;
    debugText = env->NewStringUTF(bytes);

    jmethodID method;
    ChipError err            = JniReferences::GetInstance().FindMethod(JniReferences::GetInstance().GetEnvForCurrentThread(), mJavaObjectRef,
                                                  "javaPrintCsr", "(Ljava/lang/String;[B)V", &method);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogProgress(Controller, "Error invoking javaPrintCsr 0: %" CHIP_ERROR_FORMAT, err.Format());
    }
    ChipLogProgress(Controller, "CallVoidMethod javaPrintCsr start");

    env->CallVoidMethod(mJavaObjectRef, method, debugText, javaArr);
}

void AndroidOperationalCredentialsIssuer::getRemotePAA(Credentials::DeviceAttestationVerifier::AttestationInfo &info, ByteSpan paiCert, ByteSpan dacCert) {
    mAttestationInfo = &info;
    JNIEnv * env   = JniReferences::GetInstance().GetEnvForCurrentThread();

    jbyteArray javaArr;
    JniReferences::GetInstance().N2J_ByteArray(JniReferences::GetInstance().GetEnvForCurrentThread(), paiCert.data(),
                                               paiCert.size(), javaArr);

    jbyteArray javaArrDAC;
    JniReferences::GetInstance().N2J_ByteArray(JniReferences::GetInstance().GetEnvForCurrentThread(), dacCert.data(),
                                               dacCert.size(), javaArrDAC);

    jmethodID method;
    ChipError err            = JniReferences::GetInstance().FindMethod(JniReferences::GetInstance().GetEnvForCurrentThread(), mJavaObjectRef,
                                                                       "getRemotePAA", "([B[B)V", &method);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogProgress(Controller, "Error invoking getRemotePAA 0: %" CHIP_ERROR_FORMAT, err.Format());
    }
    ChipLogProgress(Controller, "CallVoidMethod getRemotePAA start 000");

    env->CallVoidMethod(mJavaObjectRef, method, javaArr, javaArrDAC);
    ChipLogProgress(Controller, "CallVoidMethod getRemotePAA end 000");
}

void AndroidOperationalCredentialsIssuer::doDACWithNoCert(uint16_t useChoose, ByteSpan paaCert) {
    ChipLogProgress(Controller, "doDACWithNoCert start pid: %u, use choose: %u", mAttestationInfo->productId, useChoose);
    DeviceAttestationVerifier::AttestationInfo info(mAttestationInfo->attestationElementsBuffer,mAttestationInfo->attestationChallengeBuffer,
                                                    mAttestationInfo->attestationSignatureBuffer, mAttestationInfo->paiDerBuffer, mAttestationInfo->dacDerBuffer,
                                                    mAttestationInfo->attestationNonceBuffer, mAttestationInfo->vendorId, mAttestationInfo->productId);
    mController->ValidateAttestationInfo(info, useChoose, paaCert);
}

CHIP_ERROR AndroidOperationalCredentialsIssuer::GenerateNOCChain(const ByteSpan & csrElements, const ByteSpan & csrNonce,
                                                                 const ByteSpan & attestationSignature,
                                                                 const ByteSpan & attestationChallenge, const ByteSpan & DAC,
                                                                 const ByteSpan & PAI,
                                                                 Callback::Callback<OnNOCChainGeneration> * onCompletion)
{
    if (mUseJavaCallbackForNOCRequest)
    {
        return CallbackGenerateNOCChain(csrElements, csrNonce, attestationSignature, attestationChallenge, DAC, PAI, onCompletion);
    }
    else
    {
        return LocalGenerateNOCChain(csrElements, csrNonce, attestationSignature, attestationChallenge, DAC, PAI, onCompletion);
    }
}

CHIP_ERROR AndroidOperationalCredentialsIssuer::CallbackGenerateNOCChain(const ByteSpan & csrElements, const ByteSpan & csrNonce,
                                                                         const ByteSpan & csrElementsSignature,
                                                                         const ByteSpan & attestationChallenge,
                                                                         const ByteSpan & DAC, const ByteSpan & PAI,
                                                                         Callback::Callback<OnNOCChainGeneration> * onCompletion)
{
    jmethodID method;
    CHIP_ERROR err = CHIP_NO_ERROR;
    JNIEnv * env   = JniReferences::GetInstance().GetEnvForCurrentThread();

    err = JniReferences::GetInstance().FindMethod(env, mJavaObjectRef, "onNOCChainGenerationNeeded",
                                                  "(Lchip/devicecontroller/CSRInfo;Lchip/devicecontroller/AttestationInfo;)V",
                                                  &method);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Controller, "Error invoking onNOCChainGenerationNeeded: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    mOnNOCCompletionCallback = onCompletion;

    env->ExceptionClear();

    jbyteArray javaCsrElements;
    JniReferences::GetInstance().N2J_ByteArray(env, csrElements.data(), csrElements.size(), javaCsrElements);

    jbyteArray javaCsrNonce;
    JniReferences::GetInstance().N2J_ByteArray(env, csrNonce.data(), csrNonce.size(), javaCsrNonce);

    jbyteArray javaCsrElementsSignature;
    JniReferences::GetInstance().N2J_ByteArray(env, csrElementsSignature.data(), csrElementsSignature.size(),
                                               javaCsrElementsSignature);

    ChipLogProgress(Controller, "Parsing Certificate Signing Request");
    TLVReader reader;
    reader.Init(csrElements);

    if (reader.GetType() == kTLVType_NotSpecified)
    {
        ReturnErrorOnFailure(reader.Next());
    }

    VerifyOrReturnError(reader.GetType() == kTLVType_Structure, CHIP_ERROR_WRONG_TLV_TYPE);
    VerifyOrReturnError(reader.GetTag() == AnonymousTag(), CHIP_ERROR_UNEXPECTED_TLV_ELEMENT);

    TLVType containerType;
    ReturnErrorOnFailure(reader.EnterContainer(containerType));
    ReturnErrorOnFailure(reader.Next(kTLVType_ByteString, TLV::ContextTag(1)));

    ByteSpan csr(reader.GetReadPoint(), reader.GetLength());
    reader.ExitContainer(containerType);

    jbyteArray javaCsr;
    JniReferences::GetInstance().N2J_ByteArray(env, csr.data(), csr.size(), javaCsr);

    P256PublicKey pubkey;
    ReturnErrorOnFailure(VerifyCertificateSigningRequest(csr.data(), csr.size(), pubkey));
    ChipLogProgress(chipTool, "VerifyCertificateSigningRequest");

    jobject csrInfo;
    err = N2J_CSRInfo(env, javaCsrNonce, javaCsrElements, javaCsrElementsSignature, javaCsr, csrInfo);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Controller, "Failed to create CSRInfo");
        return err;
    }

    jbyteArray javaAttestationChallenge;
    JniReferences::GetInstance().N2J_ByteArray(env, attestationChallenge.data(), attestationChallenge.size(),
                                               javaAttestationChallenge);

    const ByteSpan & attestationElements = mAutoCommissioner->GetAttestationElements();
    jbyteArray javaAttestationElements;
    JniReferences::GetInstance().N2J_ByteArray(env, attestationElements.data(), attestationElements.size(),
                                               javaAttestationElements);

    const ByteSpan & attestationNonce = mAutoCommissioner->GetAttestationNonce();
    jbyteArray javaAttestationNonce;
    JniReferences::GetInstance().N2J_ByteArray(env, attestationNonce.data(), attestationNonce.size(), javaAttestationNonce);

    const ByteSpan & attestationElementsSignature = mAutoCommissioner->GetAttestationSignature();
    jbyteArray javaAttestationElementsSignature;
    JniReferences::GetInstance().N2J_ByteArray(env, attestationElementsSignature.data(), attestationElementsSignature.size(),
                                               javaAttestationElementsSignature);

    jbyteArray javaDAC;
    JniReferences::GetInstance().N2J_ByteArray(env, DAC.data(), DAC.size(), javaDAC);

    jbyteArray javaPAI;
    JniReferences::GetInstance().N2J_ByteArray(env, PAI.data(), PAI.size(), javaPAI);

    ByteSpan certificationDeclarationSpan;
    ByteSpan attestationNonceSpan;
    uint32_t timestampDeconstructed;
    ByteSpan firmwareInfoSpan;
    DeviceAttestationVendorReservedDeconstructor vendorReserved;

    err = DeconstructAttestationElements(attestationElements, certificationDeclarationSpan, attestationNonceSpan,
                                         timestampDeconstructed, firmwareInfoSpan, vendorReserved);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Controller, "Failed to create parse attestation elements");
        return err;
    }

    jbyteArray javaCD;
    JniReferences::GetInstance().N2J_ByteArray(env, certificationDeclarationSpan.data(), certificationDeclarationSpan.size(),
                                               javaCD);

    jbyteArray javaFirmwareInfo;
    JniReferences::GetInstance().N2J_ByteArray(env, firmwareInfoSpan.data(), firmwareInfoSpan.size(), javaFirmwareInfo);

    jobject attestationInfo;
    err = N2J_AttestationInfo(env, javaAttestationChallenge, javaAttestationNonce, javaAttestationElements,
                              javaAttestationElementsSignature, javaDAC, javaPAI, javaCD, javaFirmwareInfo, attestationInfo);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Controller, "Failed to create AttestationInfo");
        return err;
    }

    env->CallVoidMethod(mJavaObjectRef, method, csrInfo, attestationInfo);
    return CHIP_NO_ERROR;
}

CHIP_ERROR AndroidOperationalCredentialsIssuer::NOCChainGenerated(CHIP_ERROR status, const ByteSpan & noc, const ByteSpan & icac,
                                                                  const ByteSpan & rcac, Optional<Crypto::AesCcm128KeySpan> ipk,
                                                                  Optional<NodeId> adminSubject)
{
    ReturnErrorCodeIf(mOnNOCCompletionCallback == nullptr, CHIP_ERROR_INCORRECT_STATE);

    Callback::Callback<OnNOCChainGeneration> * onCompletion = mOnNOCCompletionCallback;
    mOnNOCCompletionCallback                                = nullptr;

    // Call-back into commissioner with the generated data.
    onCompletion->mCall(onCompletion->mContext, status, noc, icac, rcac, ipk, adminSubject);
    return CHIP_NO_ERROR;
}

CHIP_ERROR AndroidOperationalCredentialsIssuer::LocalGenerateNOCChain(const ByteSpan & csrElements, const ByteSpan & csrNonce,
                                                                      const ByteSpan & attestationSignature,
                                                                      const ByteSpan & attestationChallenge, const ByteSpan & DAC,
                                                                      const ByteSpan & PAI,
                                                                      Callback::Callback<OnNOCChainGeneration> * onCompletion)
{
    ChipLogProgress(chipTool, "LocalGenerateNOCChain enter");
    jmethodID onDeviceNoCGenerationComplete;
    CHIP_ERROR err = CHIP_NO_ERROR;
    err            = JniReferences::GetInstance().FindMethod(JniReferences::GetInstance().GetEnvForCurrentThread(), mJavaObjectRef,
                                                  "onDeviceNoCGenerationComplete", "([B[B)V", &onDeviceNoCGenerationComplete);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Controller, "Error invoking onOpCSRGenerationComplete: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    NodeId assignedId;
    if (mNodeIdRequested)
    {
        assignedId       = mNextRequestedNodeId;
        mNodeIdRequested = false;
    }
    else
    {
        assignedId = mNextAvailableNodeId++;
    }

    TLVReader reader;
    reader.Init(csrElements);

    if (reader.GetType() == kTLVType_NotSpecified)
    {
        ReturnErrorOnFailure(reader.Next());
    }

    VerifyOrReturnError(reader.GetType() == kTLVType_Structure, CHIP_ERROR_WRONG_TLV_TYPE);
    VerifyOrReturnError(reader.GetTag() == AnonymousTag(), CHIP_ERROR_UNEXPECTED_TLV_ELEMENT);

    TLVType containerType;
    ReturnErrorOnFailure(reader.EnterContainer(containerType));
    ReturnErrorOnFailure(reader.Next(kTLVType_ByteString, TLV::ContextTag(1)));

    ByteSpan csr(reader.GetReadPoint(), reader.GetLength());
    reader.ExitContainer(containerType);

    P256PublicKey pubkey;
    ReturnErrorOnFailure(VerifyCertificateSigningRequest(csr.data(), csr.size(), pubkey));

    ChipLogProgress(chipTool, "VerifyCertificateSigningRequest");

    Platform::ScopedMemoryBuffer<uint8_t> noc;
    ReturnErrorCodeIf(!noc.Alloc(kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    MutableByteSpan nocSpan(noc.Get(), kMaxCHIPDERCertLength);

    Platform::ScopedMemoryBuffer<uint8_t> rcac;
    ReturnErrorCodeIf(!rcac.Alloc(kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    MutableByteSpan rcacSpan(rcac.Get(), kMaxCHIPDERCertLength);

    Platform::ScopedMemoryBuffer<uint8_t> icac;
    ReturnErrorCodeIf(!icac.Alloc(kMaxCHIPDERCertLength), CHIP_ERROR_NO_MEMORY);
    MutableByteSpan icacSpan(icac.Get(), kMaxCHIPDERCertLength);


    ReturnErrorOnFailure(
        GenerateNOCChainAfterValidation(assignedId, mNextFabricId, chip::kUndefinedCATs, pubkey, rcacSpan, icacSpan, nocSpan));

    // TODO: Force callers to set IPK if used before GenerateNOCChain will succeed.
    ByteSpan defaultIpkSpan = chip::GroupTesting::DefaultIpkValue::GetDefaultIpk();

    // The below static assert validates a key assumption in types used (needed for public API conformance)
    static_assert(CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES == kAES_CCM128_Key_Length, "IPK span sizing must match");

    // Prepare IPK to be sent back. A more fully-fledged operational credentials delegate
    // would obtain a suitable key per fabric.
    uint8_t ipkValue[CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES];
    Crypto::AesCcm128KeySpan ipkSpan(ipkValue);

    ReturnErrorCodeIf(defaultIpkSpan.size() != sizeof(ipkValue), CHIP_ERROR_INTERNAL);

    memcpy(&ipkValue[0], defaultIpkSpan.data(), defaultIpkSpan.size());

    // Call-back into commissioner with the generated data.
    onCompletion->mCall(onCompletion->mContext, CHIP_NO_ERROR, nocSpan, icacSpan, rcacSpan, MakeOptional(ipkSpan),
                        Optional<NodeId>(0xFFFF'FFFD'0001'0001ULL));

    jbyteArray javaNoc;
    JniReferences::GetInstance().GetEnvForCurrentThread()->ExceptionClear();
    JniReferences::GetInstance().N2J_ByteArray(JniReferences::GetInstance().GetEnvForCurrentThread(), nocSpan.data(),
                                               nocSpan.size(), javaNoc);
    jbyteArray javaIPK;
    JniReferences::GetInstance().GetEnvForCurrentThread()->ExceptionClear();
    JniReferences::GetInstance().N2J_ByteArray(JniReferences::GetInstance().GetEnvForCurrentThread(), defaultIpkSpan.data(),
                                               defaultIpkSpan.size(), javaIPK);

    JniReferences::GetInstance().GetEnvForCurrentThread()->CallVoidMethod(mJavaObjectRef, onDeviceNoCGenerationComplete, javaNoc, javaIPK);
    return CHIP_NO_ERROR;
}

CHIP_ERROR N2J_CSRInfo(JNIEnv * env, jbyteArray nonce, jbyteArray elements, jbyteArray csrElementsSignature, jbyteArray csr,
                       jobject & outCSRInfo)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    jmethodID constructor;
    jclass infoClass;

    err = JniReferences::GetInstance().GetClassRef(env, "chip/devicecontroller/CSRInfo", infoClass);
    JniClass attestationInfoClass(infoClass);
    SuccessOrExit(err);

    env->ExceptionClear();
    constructor = env->GetMethodID(infoClass, "<init>", "([B[B[B[B)V");
    VerifyOrExit(constructor != nullptr, err = CHIP_JNI_ERROR_METHOD_NOT_FOUND);

    outCSRInfo = (jobject) env->NewObject(infoClass, constructor, nonce, elements, csrElementsSignature, csr);

    VerifyOrExit(!env->ExceptionCheck(), err = CHIP_JNI_ERROR_EXCEPTION_THROWN);
exit:
    return err;
}

CHIP_ERROR N2J_AttestationInfo(JNIEnv * env, jbyteArray challenge, jbyteArray nonce, jbyteArray elements,
                               jbyteArray elementsSignature, jbyteArray dac, jbyteArray pai, jbyteArray cd, jbyteArray firmwareInfo,
                               jobject & outAttestationInfo)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    jmethodID constructor;
    jclass infoClass;

    err = JniReferences::GetInstance().GetClassRef(env, "chip/devicecontroller/AttestationInfo", infoClass);
    JniClass attestationInfoClass(infoClass);
    SuccessOrExit(err);

    env->ExceptionClear();
    constructor = env->GetMethodID(infoClass, "<init>", "([B[B[B[B[B[B[B[B)V");
    VerifyOrExit(constructor != nullptr, err = CHIP_JNI_ERROR_METHOD_NOT_FOUND);

    outAttestationInfo =
        (jobject) env->NewObject(infoClass, constructor, challenge, nonce, elements, elementsSignature, dac, pai, cd, firmwareInfo);

    VerifyOrExit(!env->ExceptionCheck(), err = CHIP_JNI_ERROR_EXCEPTION_THROWN);
exit:
    return err;
}

} // namespace Controller
} // namespace chip
