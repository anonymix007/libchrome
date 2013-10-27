// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APP_CONTAINER_H_
#define MOJO_SHELL_APP_CONTAINER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/loader/job.h"
#include "mojo/public/system/core.h"

namespace base {
class FilePath;
class Thread;
}

namespace mojo {
namespace shell {

class Context;

// A container class that runs an app on its own thread.
class AppContainer : public loader::Job::Delegate {
 public:
  explicit AppContainer(Context* context);
  virtual ~AppContainer();

  void Load(const GURL& app_url);

 private:
  // From loader::Job::Delegate
  virtual void DidCompleteLoad(const GURL& app_url,
                               const base::FilePath& app_path) OVERRIDE;

  void AppCompleted();

  Context* context_;
  scoped_ptr<loader::Job> request_;
  scoped_ptr<base::Thread> thread_;

  // Following members are valid only on app thread.
  Handle shell_handle_;

  base::WeakPtrFactory<AppContainer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppContainer);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APP_CONTAINER_H_
