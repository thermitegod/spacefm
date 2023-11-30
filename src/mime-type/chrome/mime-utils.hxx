// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://source.chromium.org/chromium/chromium/src/+/main:base/nix/mime_util_xdg.h

#pragma once

#include <string>

#include <filesystem>

// Gets the mime type for a file at |filepath|.
//
// The mime type is calculated based only on the file name of |filepath|.  In
// particular |filepath| will not be touched on disk and |filepath| doesn't even
// have to exist.  This means that the function does not work for directories
// (i.e. |filepath| is assumed to be a path to a file).
//
// Note that this function might need to read from disk the mime-types data
// provided by the OS.  Therefore this function should not be called from
// threads that disallow blocking.
//
// If the mime type is unknown, this will return application/octet-stream.
const std::string GetFileMimeType(const std::filesystem::path& filepath);
