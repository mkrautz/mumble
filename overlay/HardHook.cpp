/* Copyright (C) 2005-2011, Thorvald Natvig <thorvald@natvig.com>

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

#include "HardHook.h"
#include "ods.h"

#include <MinHook.h>

static LONG minhook_init_once = 0;

// EnsureMinHookInitialized ensures that the MinHook
// library is initialized. If MinHook is already
// initialized, calling this function is a no-op.
static void EnsureMinHookInitialized() {
  // Ensure MH_Initialize is only called once.
  if (InterlockedCompareExchange(&minhook_init_once, 1, 0) == 0) {
    MH_Initialize();
  }
}

/**
 * @brief Constructs a new hook without actually injecting.
 */
HardHook::HardHook()
  : m_func(NULL)
  , m_replacement(NULL)
  , call(NULL) {
    EnsureMinHookInitialized();
}

/**
 * @brief Constructs a new hook by injecting given replacement function into func.
 * @see HardHook::setup
 * @param func Funktion to inject replacement into.
 * @param replacement Function to inject into func.
 */
HardHook::HardHook(voidFunc func, voidFunc replacement) {
    EnsureMinHookInitialized();

    m_func = func;
    m_replacement = replacement;

    setup(func, replacement);
}

/**
 * @brief Makes sure the given replacement function is run once func is called.
 *
 * Tries to construct a trampoline for the given function (@see HardHook::cloneCode)
 * and then injects replacement function calling code into the first 6 bytes of the
 * original function (@see HardHook::inject).
 *
 * @param func Pointer to function to redirect.
 * @param replacement Pointer to code to redirect to.
 */
void HardHook::setup(voidFunc func, voidFunc replacement) {
  MH_STATUS status = MH_CreateHook((LPVOID) func, (LPVOID)replacement, (LPVOID *)&call);
  if (status != MH_OK) {
    fods("HardHook: setup failed, MH_CreateHook returned %li", static_cast<long>(status));
  }

  status = MH_EnableHook((LPVOID)m_func);
  if (status != MH_OK) {
    fods("HardHook: setup failed, MH_EnableHook returned %ld", static_cast<long>(status));
  }
}

void HardHook::setupInterface(IUnknown *unkn, LONG funcoffset, voidFunc replacement) {
	fods("HardHook: setupInterface: Replacing %p function #%ld", unkn, funcoffset);
	void **ptr = reinterpret_cast<void **>(unkn);
	ptr = reinterpret_cast<void **>(ptr[0]);
	setup(reinterpret_cast<voidFunc>(ptr[funcoffset]), replacement);
}

void HardHook::reset() {
  m_func = NULL;
  m_replacement = NULL;
  call = NULL;
}

void HardHook::inject(bool force) {
  // XXX: MinHook seems to guarantee presence of a trampoline, so this can be a no-op.
  //      Check the source to make sure.
}

void HardHook::restore(bool force) {
  // XXX: MinHook seems to guarantee presence of a trampoline, so this can be a no-op.
  //      Check the source to make sure.
}