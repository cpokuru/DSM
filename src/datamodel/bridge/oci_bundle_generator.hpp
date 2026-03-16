
// If not stated otherwise in this file or this component's license file the
// following copyright and licenses apply:
//
// Copyright 2022 Consult Red
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifdef USE_RDK_BUNDLE_MANAGER

#include <string>
#include "../../ext/json.hpp"

/**
 * OciBundleGenerator
 *
 * Wraps the rdkBundleManager (libbundlegen.so) pipeline to produce a
 * ready-to-use OCI bundle directory from an OCI image URL.
 *
 * Pipeline (mirrors test_so_rdkb.cpp exactly):
 *   1. STBPlatform    – load & validate platform config
 *   2. ImageDownloader – download/locate OCI image
 *   3. ImageUnpacker   – unpack OCI image layers → rootfs/ + config.json
 *   4. BundleProcessor – apply RDK platform transforms → final OCI bundle
 *
 * The caller (PackagerAdapter::install) receives the bundle directory path
 * and hands it directly to Dobby.
 */
class OciBundleGenerator {
public:
    /**
     * @param platform       RDK platform name   (e.g. "bpir4_reference")
     *                       Reads env RDK_PLATFORM if empty.
     * @param searchPath     Template search path (e.g. "templates/generic")
     *                       Reads env RDK_PLATFORM_SEARCHPATH if empty.
     * @param outputBaseDir  Base dir for bundle output (e.g. "/tmp/dsm-bundles").
     */
    OciBundleGenerator(const std::string& platform,
                       const std::string& searchPath,
                       const std::string& outputBaseDir);

    /**
     * Run full pipeline: locate/download OCI image → unpack → generate bundle.
     *
     * @param imageUrl   OCI image URL.
     *                   Supported formats (from ImageDownloader):
     *                     "oci:/path/to/oci-layout:tag"    (local OCI layout)
     *                     "docker://registry/repo:tag"     (remote registry)
     *                     "http://host/image.tar"          (HTTP OCI tar)
     * @param creds      Optional "user:pass". Empty = anonymous.
     * @param bundleId   Unique name for the bundle subdirectory.
     *
     * @return Path to the finished OCI bundle directory on success,
     *         empty string on any failure.
     *
     * @note The downloaded OCI image is deleted after unpacking (deleteAfter=true).
     *       The output directory (outputBaseDir/bundleId) contains config.json + rootfs/
     *       ready for Dobby.
     */
    std::string generateBundle(const std::string& imageUrl,
                               const std::string& creds,
                               const std::string& bundleId);

private:
    std::string platform_;
    std::string searchPath_;
    std::string outputBaseDir_;
};

#endif // USE_RDK_BUNDLE_MANAGER
