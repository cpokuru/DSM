
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

#ifdef USE_RDK_BUNDLE_MANAGER

#include "oci_bundle_generator.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

// rdkBundleManager (bundlegen-cpp) headers
#include "image_downloader.h"
#include "image_unpacker.h"
#include "bundle_processor.h"

// Returns a sanitized copy of bundleId safe for use as a path component.
// Allows alphanumerics, hyphens, underscores and dots; replaces anything
// else with an underscore to prevent directory-traversal attacks.
static std::string sanitizeBundleId(const std::string& bundleId)
{
    std::string safe;
    safe.reserve(bundleId.size());
    for (char c : bundleId) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.') {
            safe += c;
        } else {
            safe += '_';
        }
    }
    // Guard against empty result or a bare "." / ".."
    if (safe.empty() || safe == "." || safe == "..") {
        safe = "bundle";
    }
    return safe;
}

OciBundleGenerator::OciBundleGenerator(const std::string& platformCfgPath,
                                        const std::string& outputBaseDir)
    : platformCfgPath_(platformCfgPath), outputBaseDir_(outputBaseDir)
{
    std::cout << "OciBundleGenerator: platformCfgPath=" << platformCfgPath_
              << " outputBaseDir=" << outputBaseDir_ << std::endl;

    // Ensure output base directory exists
    std::error_code ec;
    std::filesystem::create_directories(outputBaseDir_, ec);
    if (ec) {
        std::cerr << "OciBundleGenerator: failed to create outputBaseDir '"
                  << outputBaseDir_ << "': " << ec.message() << std::endl;
    }
}

nlohmann::json OciBundleGenerator::loadPlatformConfig() const
{
    nlohmann::json cfg;
    if (!platformCfgPath_.empty()) {
        try {
            std::ifstream f(platformCfgPath_);
            if (f.is_open()) {
                f >> cfg;
                std::cout << "OciBundleGenerator: loaded platform config from "
                          << platformCfgPath_ << std::endl;
                return cfg;
            }
        } catch (const std::exception& e) {
            std::cerr << "OciBundleGenerator: failed to load platform config '"
                      << platformCfgPath_ << "': " << e.what() << std::endl;
        }
    }

    // Fall back to env vars
    const char* envPlatform = std::getenv("RDK_PLATFORM");
    const char* envSearchPath = std::getenv("RDK_PLATFORM_SEARCHPATH");
    if (envPlatform) {
        cfg["platform"] = envPlatform;
        std::cout << "OciBundleGenerator: using RDK_PLATFORM=" << envPlatform << std::endl;
    }
    if (envSearchPath) {
        cfg["searchPath"] = envSearchPath;
        std::cout << "OciBundleGenerator: using RDK_PLATFORM_SEARCHPATH="
                  << envSearchPath << std::endl;
    }
    return cfg;
}

std::string OciBundleGenerator::generateBundle(const std::string& imageUrl,
                                                const std::string& creds,
                                                const std::string& bundleId)
{
    std::cout << "OciBundleGenerator::generateBundle("
              << "imageUrl=" << imageUrl
              << ", bundleId=" << bundleId << ")" << std::endl;

    nlohmann::json platformCfg = loadPlatformConfig();

    // Sanitize bundleId before using it as a path component
    std::string safeBundleId = sanitizeBundleId(bundleId);

    // Staging directory for download and unpack intermediates
    std::string stagingDir = outputBaseDir_ + "/.staging/" + safeBundleId;
    std::error_code ec;
    std::filesystem::create_directories(stagingDir, ec);
    if (ec) {
        std::cerr << "OciBundleGenerator: failed to create staging dir '"
                  << stagingDir << "': " << ec.message() << std::endl;
        return "";
    }

    // Step 1: Download OCI image
    std::cout << "OciBundleGenerator: step 1 – downloading OCI image" << std::endl;
    ImageDownloader downloader(platformCfg);
    if (!downloader.downloadImage(imageUrl, creds, stagingDir)) {
        std::cerr << "OciBundleGenerator: ImageDownloader failed for " << imageUrl << std::endl;
        return "";
    }
    std::string downloadedImagePath = downloader.getImagePath();
    std::cout << "OciBundleGenerator: downloaded image to " << downloadedImagePath << std::endl;

    // Step 2: Unpack OCI image
    std::cout << "OciBundleGenerator: step 2 – unpacking OCI image" << std::endl;
    std::string unpackDir = stagingDir + "/unpacked";
    std::filesystem::create_directories(unpackDir, ec);
    if (ec) {
        std::cerr << "OciBundleGenerator: failed to create unpack dir '"
                  << unpackDir << "': " << ec.message() << std::endl;
        return "";
    }
    ImageUnpacker unpacker(platformCfg);
    if (!unpacker.unpackImage(downloadedImagePath, unpackDir)) {
        std::cerr << "OciBundleGenerator: ImageUnpacker failed for " << downloadedImagePath << std::endl;
        return "";
    }
    std::string unpackedDir = unpacker.getUnpackedDir();
    std::cout << "OciBundleGenerator: unpacked image to " << unpackedDir << std::endl;

    // Step 3: Generate OCI bundle
    std::cout << "OciBundleGenerator: step 3 – generating OCI bundle" << std::endl;
    std::string bundleOutputDir = outputBaseDir_ + "/" + safeBundleId;
    std::filesystem::create_directories(bundleOutputDir, ec);
    if (ec) {
        std::cerr << "OciBundleGenerator: failed to create bundle output dir '"
                  << bundleOutputDir << "': " << ec.message() << std::endl;
        return "";
    }
    BundleProcessor processor(platformCfg);
    if (!processor.processBundle(unpackedDir, safeBundleId, bundleOutputDir)) {
        std::cerr << "OciBundleGenerator: BundleProcessor failed for bundleId=" << bundleId << std::endl;
        return "";
    }
    std::string bundlePath = processor.getBundlePath();
    std::cout << "OciBundleGenerator: OCI bundle generated at " << bundlePath << std::endl;

    return bundlePath;
}

#endif // USE_RDK_BUNDLE_MANAGER
