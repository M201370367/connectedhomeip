/*
 *   Copyright (c) 2021 Project CHIP Authors
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
package chip.platform;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

/** Android implementation of a key/value store using SharedPreferences. */
public class PreferencesKeyValueStoreManager implements KeyValueStoreManager {
  private final String TAG = KeyValueStoreManager.class.getSimpleName();
  private String mChipSp = "chip.platform.KeyValueStore";
  private SharedPreferences preferences;

  private final static String  kOperationalCredentialsIssuerKeypairStorage   = "AndroidDeviceControllerKey";
  public final static String  kOperationalCredentialsRootCertificateStorage = "AndroidCARootCert1";
  public final static String  kOperationalCredentialsICACStorage = "AndroidICAC1";

  @Deprecated
  public PreferencesKeyValueStoreManager(Context context) {
    preferences = context.getSharedPreferences(mChipSp, Context.MODE_PRIVATE);
  }

  public PreferencesKeyValueStoreManager(Context context, String chiSp) {
    mChipSp = chiSp;
    preferences = context.getSharedPreferences(mChipSp, Context.MODE_PRIVATE);
  }

  @Override
  public String get(String key) {
    String value = preferences.getString(key, null);
    if (value == null) {
      Log.d(TAG, "Key '" + key + "' not found in shared preferences");
    }
    Log.d(TAG, "Key '" + key + "' : " + value);
    return value;
  }

  @Override
  public void set(String key, String value) {
    preferences.edit().putString(key, value).apply();
  }

  @Override
  public void delete(String key) {
    preferences.edit().remove(key).apply();
  }

  public boolean isIssueKeyExist() {
    String value = preferences.getString(kOperationalCredentialsIssuerKeypairStorage, null);
    if (value == null) {
      Log.d(TAG, "Key '" + kOperationalCredentialsIssuerKeypairStorage + "' not found in shared preferences");
      return false;
    }
    Log.d(TAG, "Key '" + kOperationalCredentialsIssuerKeypairStorage + "' " + value);
    return true;
  }

  public boolean isRCACExist() {
    String value = preferences.getString(kOperationalCredentialsRootCertificateStorage, null);
    if (value == null) {
      Log.d(TAG, "Key '" + kOperationalCredentialsRootCertificateStorage + "' not found in shared preferences");
      return false;
    }
    Log.d(TAG, "Key '" + kOperationalCredentialsRootCertificateStorage + "' " + value);
    return true;
  }

  public boolean isICACExist() {
    String value = preferences.getString(kOperationalCredentialsICACStorage, null);
    if (value == null) {
      Log.d(TAG, "Key '" + kOperationalCredentialsICACStorage + "' not found in shared preferences");
      return false;
    }
    Log.d(TAG, "Key '" + kOperationalCredentialsICACStorage + "' " + value);
    return true;
  }
}
