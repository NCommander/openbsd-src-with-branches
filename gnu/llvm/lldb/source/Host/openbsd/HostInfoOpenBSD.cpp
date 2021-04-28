//===-- HostInfoOpenBSD.cpp -----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Host/openbsd/HostInfoOpenBSD.h"
#include "lldb/Host/FileSystem.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>

using namespace lldb_private;

llvm::VersionTuple HostInfoOpenBSD::GetOSVersion() {
  struct utsname un;

  ::memset(&un, 0, sizeof(un));
  if (::uname(&un) < 0)
    return llvm::VersionTuple();

  uint32_t major, minor;
  int status = ::sscanf(un.release, "%" PRIu32 ".%" PRIu32, &major, &minor);
  switch (status) {
  case 1:
    return llvm::VersionTuple(major);
  case 2:
    return llvm::VersionTuple(major, minor);
  }
  return llvm::VersionTuple();
}

bool HostInfoOpenBSD::GetOSBuildString(std::string &s) {
  int mib[2] = {CTL_KERN, KERN_OSREV};
  char osrev_str[12];
  uint32_t osrev = 0;
  size_t osrev_len = sizeof(osrev);

  if (::sysctl(mib, 2, &osrev, &osrev_len, NULL, 0) == 0) {
    ::snprintf(osrev_str, sizeof(osrev_str), "%-8.8u", osrev);
    s.assign(osrev_str);
    return true;
  }

  s.clear();
  return false;
}

bool HostInfoOpenBSD::GetOSKernelDescription(std::string &s) {
  struct utsname un;

  ::memset(&un, 0, sizeof(utsname));
  s.clear();

  if (uname(&un) < 0)
    return false;

  s.assign(un.version);

  return true;
}

FileSpec HostInfoOpenBSD::GetProgramFileSpec() {
  static FileSpec g_program_filespec;
  return g_program_filespec;
}

bool HostInfoOpenBSD::ComputeSupportExeDirectory(FileSpec &file_spec) {
  if (HostInfoPosix::ComputeSupportExeDirectory(file_spec) &&
      file_spec.IsAbsolute() && FileSystem::Instance().Exists(file_spec))
    return true;

  file_spec.GetDirectory().SetCString("/usr/bin");
  return true;
}
