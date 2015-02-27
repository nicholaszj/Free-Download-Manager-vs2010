/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__CryptoTask_h
#define mozilla__CryptoTask_h

#include "mozilla/Attributes.h"
#include "nsThreadUtils.h"
#include "nsNSSShutDown.h"

namespace mozilla {

/**
 * Frequently we need to run a task on a background thread without blocking
 * the main thread, and then call a callback on the main thread with the
 * result. This class provides the framework for that. Subclasses must:
 *
 *   (1) Override CalculateResult for the off-the-main-thread computation.
 *       NSS functionality may only be accessed within CalculateResult.
 *   (2) Override ReleaseNSSResources to release references to all NSS
 *       resources (that do implement nsNSSShutDownObject themselves).
 *   (3) Override CallCallback() for the on-the-main-thread call of the
 *       callback.
 *
 * CalculateResult, ReleaseNSSResources, and CallCallback are called in order,
 * except CalculateResult might be skipped if NSS is shut down before it can
 * be called; in that case ReleaseNSSResources will be called and then
 * CallCallback will be called with an error code.
 */
class CryptoTask : public nsRunnable,
                   public nsNSSShutDownObject
{
public:
  template <size_t LEN>
  nsresult Dispatch(const char (&taskThreadName)[LEN])
  {
    static_assert(LEN <= 15,
                  "Thread name must be no more than 15 characters");
    return Dispatch(nsDependentCString(taskThreadName));
  }

protected:
  CryptoTask()
    : mRv(NS_ERROR_NOT_INITIALIZED),
      mReleasedNSSResources(false)
  {
  }

  virtual ~CryptoTask();

  /**
   * Called on a background thread (never the main thread). If CalculateResult
   * is called, then its result will be passed to CallCallback on the main
   * thread.
   */
  virtual nsresult CalculateResult() = 0;

  /**
   * Called on the main thread during NSS shutdown or just before CallCallback
   * has been called. All NSS resources must be released. Usually, this just
   * means assigning nullptr to the ScopedNSSType-based memory variables.
   */
  virtual void ReleaseNSSResources() = 0;

  /**
   * Called on the main thread with the result from CalculateResult() or
   * with an error code if NSS was shut down before CalculateResult could
   * be called.
   */
  virtual void CallCallback(nsresult rv) = 0;

private:
  NS_IMETHOD Run() MOZ_OVERRIDE MOZ_FINAL;
  virtual void virtualDestroyNSSReference() MOZ_OVERRIDE MOZ_FINAL;

  nsresult Dispatch(const nsACString & taskThreadName);

  nsresult mRv;
  bool mReleasedNSSResources;
};

} // namespace mozilla

#endif // mozilla__CryptoTask_h