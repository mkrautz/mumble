/* Copyright (C) 2015, Mikkel Krautz <mikkel@krautz.dk>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MUMBLE_MUMBLE_XBOXINPUT_H_
#define MUMBLE_MUMBLE_XBOXINPUT_H_

/// XBOXINPUT_MAX_DEVICES defines the maximum
/// number of devices that can be connected
/// to the system at once.
///
/// In typical operation, one calls GetState()
/// in a loop bounded by XBOXINPUT_MAX_DEVICES.
#define XBOXINPUT_MAX_DEVICES        4

/// XBOXINPUT_TRIGGER_THRESHOLD defines the thresold
/// that an analog trigger (leftTrigger and
/// rightTrigger of XboxInputState) must have exceeded
/// in order to count as a button press.
#define XBOXINPUT_TRIGGER_THRESHOLD  30

/// XboxInputState represents the state of an
/// Xbox controller as returned by GetState().
struct XboxInputState {
	uint32_t  packetNumber;
	uint16_t  buttons;
	uint8_t   leftTrigger;
	uint8_t   rightTrigger;
	uint16_t  leftThumbX;
	uint16_t  leftThumbY;
	uint16_t  rightThumbY;
	uint16_t  rightThumbX;
};

class XboxInput {
	public:
		XboxInput();
		virtual ~XboxInput();

		/// s_XboxInputGuid is the GUID used by GlobalShortcut_win
		/// to distinguish XboxInputLibrary's events from other event
		/// soures.
		static const QUuid s_XboxInputGuid;

		/// isValid determines wheter the XboxInputLibrary
		/// is usable.
		bool isValid() const;

		/// Query the state of the Xbox controller at deviceIndex.
		/// If the function succeeds, it returns 0 (Windows's ERROR_SUCCESS).
		/// If no device is connected, it returns 0x48F (Windows's ERROR_DEVICE_NOT_CONNECTED).
		uint32_t (*GetState)(uint32_t deviceIndex, XboxInputState *state);

	protected:
		/// m_getStateFunc represents XInputGetState from the XInput DLL.
		uint32_t (*m_getStateFunc)(uint32_t deviceIndex, XboxInputState *state);

		/// m_getStateFuncEx represents XInputGetStateEx, which is optionally
		/// available in the XInput DLL.
		uint32_t (*m_getStateExFunc)(uint32_t deviceIndex, XboxInputState *state);

		/// m_xinputlib is the handle to the XInput DLL as returned by
		/// LoadLibrary.
		HMODULE m_xinputlib;

		/// m_valid determines whether or not the XboxInputLibrary
		/// is valid for use.
		bool m_valid;
};

#endif
