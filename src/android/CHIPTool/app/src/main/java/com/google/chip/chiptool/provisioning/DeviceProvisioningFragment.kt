/*
 *   Copyright (c) 2020 Project CHIP Authors
 *   All rights reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

package com.google.chip.chiptool.provisioning

import android.app.Dialog
import android.bluetooth.BluetoothGatt
import android.content.DialogInterface
import android.os.Bundle
import android.util.Base64
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import chip.devicecontroller.ChipDeviceController
import chip.devicecontroller.ControllerParams
import chip.devicecontroller.NetworkCredentials
import chip.platform.PreferencesKeyValueStoreManager
import com.google.chip.chiptool.ChipClient
import com.google.chip.chiptool.GenericChipDeviceListener
import com.google.chip.chiptool.R
import com.google.chip.chiptool.bluetooth.BluetoothManager
import com.google.chip.chiptool.setuppayloadscanner.CHIPDeviceInfo
import com.google.chip.chiptool.util.DeviceIdUtil
import com.google.chip.chiptool.util.FragmentUtil
import kotlinx.android.synthetic.main.single_fragment_container.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch

@ExperimentalCoroutinesApi
class DeviceProvisioningFragment : Fragment() {

  private lateinit var deviceInfo: CHIPDeviceInfo

  private var gatt: BluetoothGatt? = null

  private val networkCredentials: NetworkCredentials?
    get() = arguments?.getParcelable(ARG_NETWORK_CREDENTIALS)

  private lateinit var scope: CoroutineScope

  private var mPreferencesKeyValueStoreManager: PreferencesKeyValueStoreManager? = null

  private var deviceController:ChipDeviceController? = null

  override fun onCreateView(
    inflater: LayoutInflater,
    container: ViewGroup?,
    savedInstanceState: Bundle?
  ): View {
    scope = viewLifecycleOwner.lifecycleScope
    deviceInfo = checkNotNull(requireArguments().getParcelable(ARG_DEVICE_INFO))
    return inflater.inflate(R.layout.single_fragment_container, container, false).apply {
      if (savedInstanceState == null) {
        mPreferencesKeyValueStoreManager = PreferencesKeyValueStoreManager(requireContext(), "", "1")
        Log.i(TAG, "ketExist: " + mPreferencesKeyValueStoreManager?.isIssueKeyExist + ", rootcaExist=" + mPreferencesKeyValueStoreManager?.isRCACExist + ", icacExist=" + mPreferencesKeyValueStoreManager?.isICACExist);

        if (true) {
          var rcac = "MIIBmjCCAUGgAwIBAgIGAYRR26mpMAoGCCqGSM49BAMCMCIxIDAeBgorBgEEAYKifAEEDBBDQUNBQ0FDQTAwMDAwMDAxMB4XDTIyMTEwNzExMzEwMVoXDTQyMTEwNzExMzEwMVowIjEgMB4GCisGAQQBgqJ8AQQMEENBQ0FDQUNBMDAwMDAwMDEwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASZERVkqyowUG80RKZ85TKGwNPoTPNNjR6ytJ3fWBNNjTprWHjVdczT02q8CkLh8wQbxq0Tk3a38uEk9SV98A5Xo2MwYTAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAfBgNVHSMEGDAWgBQ1G9emyITHOPdk/Sjm12jwcxfqvTAdBgNVHQ4EFgQUNRvXpsiExzj3ZP0o5tdo8HMX6r0wCgYIKoZIzj0EAwIDRwAwRAIgdLWxV/gFD+lK5qsq+UEhX4xujfTm8Gd8meVI/ysjylYCIGjHRJ9SR55+gUqkPX683SBHsIj4kRv8VREmr8S4lEmy"
          mPreferencesKeyValueStoreManager?.set(mPreferencesKeyValueStoreManager?.kOperationalCredentialsRootCertificateStorage, Base64.encodeToString(Base64.decode(rcac, Base64.NO_WRAP), Base64.NO_WRAP))
        }
        if (true) {
          var icac = "MIIBnDCCAUGgAwIBAgIGAYRR26nEMAoGCCqGSM49BAMCMCIxIDAeBgorBgEEAYKifAEEDBBDQUNBQ0FDQTAwMDAwMDAxMB4XDTIyMTEwNzExMzEwMVoXDTQyMTEwNzExMzEwMVowIjEgMB4GCisGAQQBgqJ8AQMMEENBQ0FDQUNBMDAwMDAwMDMwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASyNyzm9wZE7VHKxhutZ88pY19T3SjA8A+68JOugUj/90o23WKk6IwHGZkaWsgtTsfZoaURefqSqH/eEJqZRgRIo2MwYTAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAfBgNVHSMEGDAWgBQ1G9emyITHOPdk/Sjm12jwcxfqvTAdBgNVHQ4EFgQUz3Sd1Ls4pQRE4RIoWYFI2tV6JekwCgYIKoZIzj0EAwIDSQAwRgIhAP6BN+u8SfiwnSfGL/fGKpUpO4rb14GDTziiAyzAFw4uAiEAu5BTjt2p30ZgL3reQGv7lmPyDj17PWadcqutnKdwOc4="
          mPreferencesKeyValueStoreManager?.set(mPreferencesKeyValueStoreManager?.kOperationalCredentialsICACStorage, Base64.encodeToString(Base64.decode(icac, Base64.NO_WRAP), Base64.NO_WRAP))
        }
        initWhitoutCertBtn.setOnClickListener {
          deviceController = ChipClient.getDeviceControllerWithoutInitCert(requireContext())
        }

        getCsrBtn.setOnClickListener {
          deviceController?.getPhoneCsr(ControllerParams.newBuilder().setControllerVendorId(ChipClient.VENDOR_ID).build(), object : ChipDeviceController.ICSRHandler{
            override fun onGet(csr: ByteArray?) {

            }
          })
        }

        initCertBtn.setOnClickListener {
          deviceController?.initLocalPhoneCert(ControllerParams.newBuilder().setControllerVendorId(ChipClient.VENDOR_ID).build());
        }


        newControllerBtn.setOnClickListener {
          deviceController = ChipClient.getDeviceController(requireContext())
        }

        pairDeviceBtn.setOnClickListener {
          if (deviceInfo.ipAddress != null) {
            pairDeviceWithAddress()
          } else {
            startConnectingToDevice()
          }
        }

      }
    }
  }

  override fun onStop() {
    super.onStop()
    gatt = null
  }

  private fun pairDeviceWithAddress() {
    // IANA CHIP port
    val port = 5540
    val id = DeviceIdUtil.getNextAvailableId(requireContext())
    DeviceIdUtil.setNextAvailableId(requireContext(), id + 1)
    deviceController?.setCompletionListener(ConnectionCallback())
    deviceController?.pairDeviceWithAddress(
      id,
      deviceInfo.ipAddress,
      port,
      deviceInfo.discriminator,
      deviceInfo.setupPinCode,
      null
    )
  }

  private fun startConnectingToDevice() {
    if (gatt != null) {
      return
    }

    scope.launch {
      val bluetoothManager = BluetoothManager()

      showMessage(
        R.string.rendezvous_over_ble_scanning_text,
        deviceInfo.discriminator.toString()
      )
      val device = bluetoothManager.getBluetoothDevice(requireContext(), deviceInfo.discriminator) ?: run {
        showMessage(R.string.rendezvous_over_ble_scanning_failed_text)
        return@launch
      }

      showMessage(
        R.string.rendezvous_over_ble_connecting_text,
        device.name ?: device.address.toString()
      )
      gatt = bluetoothManager.connect(requireContext(), device)

      showMessage(R.string.rendezvous_over_ble_pairing_text)
      deviceController?.setCompletionListener(ConnectionCallback())

      val deviceId = DeviceIdUtil.getNextAvailableId(requireContext())
      val connId = bluetoothManager.connectionId
      deviceController?.pairDevice(gatt, connId, deviceId, deviceInfo.setupPinCode, networkCredentials)
      DeviceIdUtil.setNextAvailableId(requireContext(), deviceId + 1)
    }
  }

  private fun showMessage(msgResId: Int, stringArgs: String? = null) {
    requireActivity().runOnUiThread {
      val context = requireContext()
      val msg = context.getString(msgResId, stringArgs)
      Log.i(TAG, "showMessage:$msg")
      Toast.makeText(context, msg, Toast.LENGTH_SHORT)
        .show()
    }
  }

  inner class ConnectionCallback : GenericChipDeviceListener() {
    override fun onConnectDeviceComplete() {
      Log.d(TAG, "onConnectDeviceComplete")
    }

    override fun onStatusUpdate(status: Int) {
      Log.d(TAG, "Pairing status update: $status")
    }

    override fun onCommissioningComplete(nodeId: Long, errorCode: Int) {
      if (errorCode == STATUS_PAIRING_SUCCESS) {
        FragmentUtil.getHost(this@DeviceProvisioningFragment, Callback::class.java)
          ?.onCommissioningComplete(0)
      } else {
        showMessage(R.string.rendezvous_over_ble_pairing_failure_text)
      }
    }

    override fun onPairingComplete(code: Int) {
      Log.d(TAG, "onPairingComplete: $code")

      if (code != STATUS_PAIRING_SUCCESS) {
        showMessage(R.string.rendezvous_over_ble_pairing_failure_text)
      }
    }

    override fun onOpCSRGenerationComplete(csr: ByteArray) {
      Log.d(TAG, String(csr))
    }

    override fun onDeviceNoCGenerationComplete(deviceNoc: ByteArray?, ipk: ByteArray?) {

    }

    override fun getRemotePAA(byteArray: ByteArray) {
      requireActivity().runOnUiThread {
        Log.d(TAG, "getRemotePAA")
        deviceController?.doDACWithNoCert(3, null)
      }

    }

    override fun onPairingDeleted(code: Int) {
      Log.d(TAG, "onPairingDeleted: $code")
    }

    override fun onCloseBleComplete() {
      Log.d(TAG, "onCloseBleComplete")
    }

    override fun onError(error: Throwable?) {
      Log.d(TAG, "onError: $error")
    }
  }

  /** Callback from [DeviceProvisioningFragment] notifying any registered listeners. */
  interface Callback {
    /** Notifies that commissioning has been completed. */
    fun onCommissioningComplete(code: Int)
  }

  companion object {
    private const val TAG = "DeviceProvisioningFragment"
    private const val ARG_DEVICE_INFO = "device_info"
    private const val ARG_NETWORK_CREDENTIALS = "network_credentials"
    private const val STATUS_PAIRING_SUCCESS = 0

    /**
     * Return a new instance of [DeviceProvisioningFragment]. [networkCredentials] can be null for
     * IP commissioning.
     */
    fun newInstance(
      deviceInfo: CHIPDeviceInfo,
      networkCredentials: NetworkCredentials?,
    ): DeviceProvisioningFragment {
      return DeviceProvisioningFragment().apply {
        arguments = Bundle(2).apply {
          putParcelable(ARG_DEVICE_INFO, deviceInfo)
          putParcelable(ARG_NETWORK_CREDENTIALS, networkCredentials)
        }
      }
    }
  }
}
