/**
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#import "MTRDeviceAttestationDelegateBridge.h"
#import "MTRDeviceAttestationDelegate_Internal.h"
#import "MTRError_Internal.h"
#import "NSDataSpanConversion.h"
#include <controller/CHIPDeviceController.h>
#include <crypto/CHIPCryptoPAL.h>

void MTRDeviceAttestationDelegateBridge::OnDeviceAttestationCompleted(chip::Controller::DeviceCommissioner * deviceCommissioner,
    chip::DeviceProxy * device, const chip::Credentials::DeviceAttestationVerifier::AttestationDeviceInfo & info,
    chip::Credentials::AttestationVerificationResult attestationResult)
{
    dispatch_async(mQueue, ^{
        NSLog(@"MTRDeviceAttestationDelegateBridge::OnDeviceAttestationFailed completed with result: %hu", attestationResult);

        mResult = attestationResult;

        id<MTRDeviceAttestationDelegate> strongDelegate = mDeviceAttestationDelegate;
        if ([strongDelegate respondsToSelector:@selector(deviceAttestationCompletedForController:
                                                                                          device:attestationDeviceInfo:error:)]) {
            MTRDeviceController * strongController = mDeviceController;
            if (strongController) {
                NSData * dacData = AsData(info.dacDerBuffer());
                NSData * paiData = AsData(info.paiDerBuffer());
                NSData * cdData = info.cdBuffer().HasValue() ? AsData(info.cdBuffer().Value()) : nil;
                MTRDeviceAttestationDeviceInfo * deviceInfo =
                    [[MTRDeviceAttestationDeviceInfo alloc] initWithDACCertificate:dacData
                                                                 dacPAICertificate:paiData
                                                            certificateDeclaration:cdData];
                NSError * error = (attestationResult == chip::Credentials::AttestationVerificationResult::kSuccess)
                    ? nil
                    : [MTRError errorForCHIPErrorCode:CHIP_ERROR_INTEGRITY_CHECK_FAILED];
                [strongDelegate deviceAttestationCompletedForController:mDeviceController
                                                                 device:device
                                                  attestationDeviceInfo:deviceInfo
                                                                  error:error];
            }
        } else if ((attestationResult != chip::Credentials::AttestationVerificationResult::kSuccess) &&
            [strongDelegate respondsToSelector:@selector(deviceAttestationFailedForController:device:error:)]) {

            MTRDeviceController * strongController = mDeviceController;
            if (strongController) {
                NSError * error = [MTRError errorForCHIPErrorCode:CHIP_ERROR_INTEGRITY_CHECK_FAILED];
                [strongDelegate deviceAttestationFailedForController:mDeviceController device:device error:error];
            }
        }
    });
}


void MTRDeviceAttestationDelegateBridge::getRemotePAAThenContinueVerify(chip::Controller::DeviceCommissioner * deviceCommissioner, chip::Credentials::DeviceAttestationVerifier::AttestationInfo & info) {
    chip::Credentials::DeviceAttestationVerifier::AttestationInfo theInfo = info;
    dispatch_async(mQueue, ^{
        id<MTRDeviceAttestationDelegate> strongDelegate = mDeviceAttestationDelegate;
        if ([strongDelegate respondsToSelector:@selector(deviceAttestationAskForRemotePAAForSubjectKeyID:subject:completion:)]) {
            uint8_t akidBuf[chip::Crypto::kAuthorityKeyIdentifierLength];
            NSData *subjectKeyID;
            chip::MutableByteSpan akid(akidBuf);
            if (chip::ExtractAKIDFromX509Cert(theInfo.paiDerBuffer, akid) == CHIP_NO_ERROR) {
                subjectKeyID = [AsData(akid) copy];
            }
            
            uint8_t subjectBuf[100]; //max 68?
            NSData *subjectData;
            chip::MutableByteSpan subject(subjectBuf);
            if (chip::ExtractIssuerFromX509Cert(theInfo.paiDerBuffer, subject) == CHIP_NO_ERROR) {
                subjectData = [AsData(subject) copy];
            }
            
            [strongDelegate deviceAttestationAskForRemotePAAForSubjectKeyID:subjectKeyID subject:subjectData completion:^(NSData * _Nonnull paaData) {
                //always should use USE_CHOOSE_GOON_COMMISSION_WITH_PAA, because the result with paa check will delegate on OnDeviceAttestationCompleted method
                dispatch_async(mQueue, ^{
                    chip::ByteSpan paaCert = AsByteSpan(paaData);
                    deviceCommissioner->ValidateAttestationInfo(theInfo, chip::Controller::USE_CHOOSE_GOON_COMMISSION_WITH_PAA, paaCert);
                });
            }];
        } else {
            //always should use USE_CHOOSE_GOON_COMMISSION_WITH_PAA, because the result with paa check will delegate on OnDeviceAttestationCompleted method
            chip::ByteSpan paaCertEmpty;
            deviceCommissioner->ValidateAttestationInfo(theInfo, chip::Controller::USE_CHOOSE_GOON_COMMISSION_WITH_PAA, paaCertEmpty);
        }
    });
}
