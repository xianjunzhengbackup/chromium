// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"

class GetRegistrationCodeFunction : public SyncExtensionFunction {
 public:
  GetRegistrationCodeFunction();

 protected:
  virtual ~GetRegistrationCodeFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getRegistrationCode",
                             ECHOPRIVATE_GETREGISTRATIONCODE)
};

class GetOobeTimestampFunction : public AsyncExtensionFunction {
 public:
  GetOobeTimestampFunction();

 protected:
  virtual ~GetOobeTimestampFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  bool GetOobeTimestampOnFileThread();
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getOobeTimestamp",
                             ECHOPRIVATE_GETOOBETIMESTAMP)
};
#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
