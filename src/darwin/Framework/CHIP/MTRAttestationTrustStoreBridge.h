/**
 *
 *    Copyright (c) 2022 Project CHIP Authors
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

#import "MTRError_Internal.h"
#include <credentials/attestation_verifier/DeviceAttestationVerifier.h>

NS_ASSUME_NONNULL_BEGIN

class MTRAttestationTrustStoreBridge : public chip::Credentials::AttestationTrustStore {
public:
    MTRAttestationTrustStoreBridge(NSArray<NSData *> * paaCerts)
        : mPaaCerts(paaCerts)
    {
    }
    ~MTRAttestationTrustStoreBridge() {};

    CHIP_ERROR GetProductAttestationAuthorityCert(
        const chip::ByteSpan & skid, chip::MutableByteSpan & outPaaDerBuffer) const override;


    CHIP_ERROR SetOfficialPAACert(chip::ByteSpan * derCerts, size_t numCerts) override
    {
        mOfficialPAA = derCerts;
        mNumOfficialPAA = numCerts;
        return CHIP_NO_ERROR;
    }
private:
    NSArray <NSData *> * mPaaCerts;
    chip::ByteSpan * mOfficialPAA;
    size_t mNumOfficialPAA;
};

NS_ASSUME_NONNULL_END
