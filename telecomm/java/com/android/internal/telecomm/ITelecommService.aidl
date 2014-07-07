/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.internal.telecomm;

import android.content.ComponentName;
import android.telecomm.PhoneAccount;

/**
 * Interface used to interact with Telecomm. Mostly this is used by TelephonyManager for passing
 * commands that were previously handled by ITelephony.
 * {@hide}
 */
interface ITelecommService {
    /**
     * Brings the in-call screen to the foreground if there is an active call.
     *
     * @param showDialpad if true, make the dialpad visible initially.
     */
    void showCallScreen(boolean showDialpad);

    /**
     * Gets a list of accounts.
     */
    List<PhoneAccount> getAccounts();

    /**
     * Sets the enabled state of a given account.
     */
    void setEnabled(in PhoneAccount account, boolean enabled);

    /**
     * Sets a given account as the system default.
     */
    void setSystemDefault(in PhoneAccount account);

    /**
     * Returns the component name of the default phone application.
     */
    ComponentName getDefaultPhoneApp();

    //
    // Internal system apis relating to call management.
    //

    /**
     * @see TelecommManager#silenceRinger
     */
    void silenceRinger();

    /**
     * @see TelecommManager#isInAPhoneCall
     */
    boolean isInAPhoneCall();

    /**
     * @see TelecomManager#isRinging
     */
    boolean isRinging();

    /**
     * @see TelecommManager#endCall
     */
    boolean endCall();

    /**
     * @see TelecommManager#acceptRingingCall
     */
    void acceptRingingCall();

    /**
     * @see PhoneManager#cancelMissedCallsNotification
     */
    void cancelMissedCallsNotification();

    /**
     * @see PhoneManager#handlePinMmi
     */
    boolean handlePinMmi(String dialString);
}
