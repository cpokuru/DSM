
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
 * Wraps the rdkBundleManager (bundlegen-cpp) pipeline:
 *   1. Download OCI image  → ImageDownloader
 *   2. Unpack OCI image    → ImageUnpacker
 *   3. Process bundle      → BundleProcessor
 *
 * The resulting OCI bundle directory can then be handed to Dobby.
 */
class OciBundleGenerator {
public:
    /**
     * @param platformCfgPath  Path to the RDK platform JSON config file.
     *                         Falls back to env RDK_PLATFORM / RDK_PLATFORM_SEARCHPATH.
     * @param outputBaseDir    Base directory under which bundles are created,
     *                         e.g. "/tmp/dsm-bundles".
     */
    OciBundleGenerator(const std::string& platformCfgPath,
                       const std::string& outputBaseDir);

    /**
     * Full pipeline: download OCI image → unpack → generate bundle.
     *
     * @param imageUrl   OCI image URL, e.g. "docker://192.168.128.2:5000/iperf3:latest"
     *                   or a plain HTTP URL to an OCI image tar.
     * @param creds      Optional registry credentials "user:pass". Empty = anonymous.
     * @param bundleId   Unique identifier used to name the output bundle directory.
     *
     * @return  Path to the generated OCI bundle directory on success, empty string on failure.
     */
    std::string generateBundle(const std::string& imageUrl,
                               const std::string& creds,
                               const std::string& bundleId);

private:
    std::string platformCfgPath_;
    std::string outputBaseDir_;

    nlohmann::json loadPlatformConfig() const;
};

#endif // USE_RDK_BUNDLE_MANAGER
