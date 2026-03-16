
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

#include "oci_bundle_generator.hpp"

#ifdef USE_RDK_BUNDLE_MANAGER

#include <cstdlib>
#include <filesystem>
#include <iostream>

// rdkBundleManager headers (from libbundlegen.so)
#include "stb_platform.h"
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

OciBundleGenerator::OciBundleGenerator(const std::string& platform,
                                        const std::string& searchPath,
                                        const std::string& outputBaseDir)
    : platform_(platform), searchPath_(searchPath), outputBaseDir_(outputBaseDir)
{
    // Fall back to environment variables when constructor args are empty
    if (platform_.empty()) {
        const char* env = std::getenv("RDK_PLATFORM");
        if (env) platform_ = env;
    }
    if (searchPath_.empty()) {
        const char* env = std::getenv("RDK_PLATFORM_SEARCHPATH");
        if (env) searchPath_ = env;
    }

    std::cout << "OciBundleGenerator: platform=" << platform_
              << " searchPath=" << searchPath_
              << " outputBaseDir=" << outputBaseDir_ << std::endl;

    // Ensure output base directory exists
    std::error_code ec;
    std::filesystem::create_directories(outputBaseDir_, ec);
    if (ec) {
        std::cerr << "OciBundleGenerator: failed to create outputBaseDir '"
                  << outputBaseDir_ << "': " << ec.message() << std::endl;
    }
}

std::string OciBundleGenerator::generateBundle(const std::string& imageUrl,
                                                const std::string& creds,
                                                const std::string& bundleId)
{
    std::cout << "OciBundleGenerator::generateBundle("
              << "imageUrl=" << imageUrl
              << ", bundleId=" << bundleId << ")" << std::endl;

    // Sanitize bundleId before using it as a path component
    std::string safeBundleId = sanitizeBundleId(bundleId);

    // Output directory for the final OCI bundle (config.json + rootfs/)
    std::string outputDir = outputBaseDir_ + "/" + safeBundleId;
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
        std::cerr << "OciBundleGenerator: failed to create outputDir '"
                  << outputDir << "': " << ec.message() << std::endl;
        return "";
    }

    try {
        // Step 1: Load platform config
        STBPlatform platform(platform_, searchPath_);
        platform.foundConfig();
        platform.validatePlatformConfig();
        nlohmann::json platformCfg = platform.getConfig();
        std::cout << "OciBundleGenerator: step 1 – platform config loaded" << std::endl;

        // Step 2: Download OCI image
        ImageDownloader imgDownloader;
        std::string imgPath = imgDownloader.downloadImage(imageUrl, creds, platformCfg);
        std::string imageTag = ImageDownloader::getImageTag(imageUrl);
        std::cout << "OciBundleGenerator: step 2 – image downloaded to "
                  << imgPath << " (tag=" << imageTag << ")" << std::endl;

        // Step 3: Unpack OCI image
        ImageUnpacker imgUnpacker(imgPath, outputDir);
        imgUnpacker.unpackImage(imageTag, /*deleteAfter=*/true);
        nlohmann::json appMeta = imgUnpacker.getAppMetadataFromImg();
        imgUnpacker.deleteImgAppMetadata();
        std::cout << "OciBundleGenerator: step 3 – image unpacked" << std::endl;

        // Step 4: Process bundle
        // Default flags matching test_so_rdkb.cpp reference implementation.
        // noDepWalking=false (perform dependency walking),
        // libMatchingMode=false, createMountPoints=true, crunOnly=false.
        BundleProcessor processor(platformCfg, outputDir, appMeta,
                                  /*noDepWalking=*/false,
                                  /*libMatchingMode=*/false,
                                  /*createMountPoints=*/true,
                                  /*crunOnly=*/false);
        processor.checkCompatibility();
        processor.validateAppMetadataConfig();
        processor.beginProcessing();
        std::cout << "OciBundleGenerator: step 4 – bundle processed at "
                  << outputDir << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "OciBundleGenerator: pipeline failed: " << e.what() << std::endl;
        return "";
    }

    return outputDir;
}

#endif // USE_RDK_BUNDLE_MANAGER
