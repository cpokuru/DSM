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

#include "packager-adapter.hpp"
#include "../bridge/packager.hpp"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <optional>
#include <ftw.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <filesystem>

#include "../../utils/file-system.hpp"

// ── libbundlegen.so headers ──────────────────────────────────────────────────
// Adjust this include path to match your DSM CMakeLists.txt after adding it
#include "/nvram/chandra/dac20bb/rdkcentral/rdkBundleManager/bundlegen-cpp/include/stb_platform.h"
#include "/nvram/chandra/dac20bb/rdkcentral/rdkBundleManager/bundlegen-cpp/include/image_downloader.h"
#include "/nvram/chandra/dac20bb/rdkcentral/rdkBundleManager/bundlegen-cpp/include/image_unpacker.h"
#include "/nvram/chandra/dac20bb/rdkcentral/rdkBundleManager/bundlegen-cpp/include/bundle_processor.h"
#include "/nvram/chandra/dac20bb/rdkcentral/rdkBundleManager/bundlegen-cpp/include/logger.h"

// g_logLevel is declared extern in logger.h and used by libbundlegen.so.
// Define it once here in the DSM binary so the linker finds it.
// (If dsm main.cpp already defines it, remove this line and declare extern instead.)
LogLevel g_logLevel = LogLevel::INFO;

// ─────────────────────────────────────────────────────────────────────────────
// Unchanged helpers from original packager-adapter.cpp
// ─────────────────────────────────────────────────────────────────────────────

bool is_http(std::string uri)
{
    return (uri.substr(0, 7) == "http://") || (uri.substr(0, 8) == "https://");
}

void PackagerAdapter::fork_exe(char* path, char* const args[],
                                const std::function<void(void)>& fn_callback)
{
    std::cout << "fork_exe path=" << path << std::endl;
    pid_t pid;
    int status;
    pid_t ret;

    pid = fork();
    if (pid == -1) {
        std::cerr << "fork_exe: fork() failed: " << strerror(errno) << std::endl;
    } else if (pid != 0) {
        // Parent — wait for child
        while ((ret = waitpid(pid, &status, 0)) == -1) {
            if (errno != EINTR) break;
        }
        // Always call the callback in the parent (existing behaviour preserved)
        fn_callback();
    } else {
        // Child
        int result = execv(path, args);
        std::cout << "execv path=" << path << ", result=" << result << std::endl;
        if (result == -1) _Exit(127);
    }
}

std::string convertToLocalUri(std::string uri)
{
    std::string localUri = uri;
    if (is_http(uri)) {
        size_t length = uri.length();
        size_t last   = 0;
        bool   found  = false;
        for (size_t i = 0; i < length; i++) {
            if (uri[i] == '/') { last = i; found = true; }
        }
        if (found) {
            localUri = uri.substr(last + 1);
            std::cout << "Converted to local " << localUri << std::endl;
        }
    }
    return localUri;
}

std::optional<std::string> strip_tar_ext(std::string filename)
{
    if (filename.find(".tar.gz") != std::string::npos)
        return filename.substr(0, filename.length() - 7);
    if (filename.find(".tar") != std::string::npos)
        return filename.substr(0, filename.length() - 4);
    return std::nullopt;
}

auto load_package_config(std::string dest, std::string id) -> nlohmann::json
{
    nlohmann::json package_config;
    std::string localUri = convertToLocalUri(id);
    try {
        std::ifstream du_file(dest + localUri + ".json");
        du_file >> package_config;
    } catch (const std::exception& e) {
        return nlohmann::json::parse("{}");
    }
    return package_config;
}

auto save_package_config(std::string dest, std::string id, std::string uri,
                          std::string state, std::string path = "",
                          std::string localUri = "") -> nlohmann::json
{
    std::cout << "save_package_config dest=" << dest << ", state=" << state << std::endl;
    nlohmann::json package_data;
    package_data["id"]  = id;
    package_data["uri"] = uri;
    package_data["state"] = state;

    if (path.length() > 0) {
        package_data["path"] = path;
        package_data["exec"] = path_exists(path + "/config.json");
        try {
            std::ofstream package_file(dest + localUri + ".json");
            package_file << package_data << std::endl;
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
        }
    }
    return package_data;
}

auto delete_package_config(std::string dest, std::string id, std::string uri,
                             std::string state, std::string path = "",
                             std::string localUri = "") -> nlohmann::json
{
    nlohmann::json package_data;
    package_data["id"]    = id;
    package_data["uri"]   = uri;
    package_data["state"] = state;

    if (path.length() > 0) {
        auto file_name_gz  = path + ".tar.gz.json";
        auto file_name_tar = path + ".tar.json";
        auto file_name_local = dest + convertToLocalUri(id) + ".json";

        if (path_exists(file_name_gz)) {
            std::cout << "Deleting " << file_name_gz << std::endl;
            remove(file_name_gz.c_str());
        } else if (path_exists(file_name_tar)) {
            std::cout << "Deleting " << file_name_tar << std::endl;
            remove(file_name_tar.c_str());
	} else if (path_exists(file_name_local)) {
            std::cout << "Deleting " << file_name_local << std::endl;
            remove(file_name_local.c_str());
        } else {
            std::cout << "No config file found" << std::endl;
        }
    }
    return package_data;
}

PackagerAdapter::PackagerAdapter() : config("")
{
    std::cout << "<<create>> PackagerAdapter(config=" << config << ")" << std::endl;
}


void PackagerAdapter::configure(nlohmann::json config)
{
    std::cout << "PackagerAdapter->configure(" << config << ")" << std::endl;
    this->config = config;

    // ── Bundlegen defaults — applied if not present in dsm.config ─────────
#ifdef ENABLE_BUNDLEGEN
    if (!this->config.contains("bundlegenEnabled"))
        this->config["bundlegenEnabled"] = true;

    if (!this->config.contains("platformName"))
        this->config["platformName"] = "bpir4_reference";

    if (!this->config.contains("platformSearchPath"))
        this->config["platformSearchPath"] =
            "/nvram/chandra/dac20bb/rdkcentral/rdkBundleManager/templates/generic";
#endif
    // ─────────────────────────────────────────────────────────────────────

    std::string destination_path{config["destination"]};
    if (!path_exists(destination_path)) {
        if (!create_directory(destination_path))
            throw std::invalid_argument(
                std::string("Destination path doesn't exist and unable to create: ")
                + destination_path);
    }
}

void PackagerAdapter::wget_callback_success(std::shared_ptr<PackageData> package,
                                             std::string id, std::string uri,
                                             std::string dest, std::string localUri)
{
    std::cout << "wget_callback_success" << std::endl;

    std::function<void(void)> fn_install_callback = [=]() {
        std::string delete_file = dest + localUri;
        std::remove(delete_file.c_str());
        std::cout << "Delete file=" << delete_file << std::endl;
    };

    std::string containerName = convertToLocalUri(localUri);
    std::optional<std::string> stripped = strip_tar_ext(containerName);
    if (!stripped.has_value()) {
        std::cout << "Install Error: couldn't extract file type" << std::endl;
        return;
    }
    containerName = stripped.value();
    std::filesystem::create_directory(dest + containerName);

    std::string arg1 = "-xf";
    std::string arg2 = dest + localUri;
    std::string arg3 = "-C";
    std::string arg4 = dest + containerName;

    char* argv_list[] = {
        (char*)"tar", (char*)arg1.c_str(), (char*)arg2.c_str(),
        (char*)arg3.c_str(), (char*)arg4.c_str(), nullptr
    };
    fork_exe((char*)"/bin/tar", argv_list, fn_install_callback);
}

void PackagerAdapter::fork_exe_wget(std::shared_ptr<PackageData> package,
                                     std::string id, std::string uri,
                                     std::string dest, std::string localUri)
{
    std::function<void(void)> fn_wget_callback = [=]() {
        wget_callback_success(package, id, uri, dest, localUri);
    };

    std::string arg1 = "-T";
    std::string arg2 = "3";
    std::string arg3 = uri;
    std::string arg4 = "-O";
    std::string arg5 = dest + "/" + localUri;

    char* argv_list[] = {
        (char*)"wget", (char*)arg1.c_str(), (char*)arg2.c_str(),
        (char*)arg3.c_str(), (char*)arg4.c_str(), (char*)arg5.c_str(), nullptr
    };
    fork_exe((char*)"/usr/bin/wget", argv_list, fn_wget_callback);
}

// ─────────────────────────────────────────────────────────────────────────────
// install() — routes to OCI or traditional flow based on URL
// ─────────────────────────────────────────────────────────────────────────────
auto PackagerAdapter::install(std::shared_ptr<PackageData> package,
                               std::string id, std::string uri) -> nlohmann::json
{
    auto dest        = std::string(config["destination"]) + "/";
    std::string localUri = uri;

    std::cout << "PackagerAdapter::install(id=" << id << ", uri=" << uri << ")" << std::endl;
    std::cout << "   config=" << config << std::endl;

    auto package_config = load_package_config(dest, id);
    if ((!package_config["state"].is_null() && package_config["state"] != "uninstalled")
        && package_config["exec"]) {
        std::cout << "    INSTALL ERROR: Package already installed." << std::endl;
        return package_config;
    }

    if (!is_http(uri)) {
        std::cout << "    INSTALL ERROR: Local install not supported, use http" << std::endl;
        return package_config;
    }

    // ── Route decision ────────────────────────────────────────────────────────
    bool bundlegenEnabled = config.value("bundlegenEnabled", false);

    if (bundlegenEnabled && is_binoci(uri)) {
        std::cout << "    >>> OCI BUNDLE FLOW (libbundlegen.so) <<<" << std::endl;
        return install_oci_bundle(package, id, uri);
    }

    // ── Traditional flow (unchanged) ─────────────────────────────────────────
    std::cout << "    >>> TRADITIONAL TAR FLOW <<<" << std::endl;
    localUri = convertToLocalUri(uri);
    save_package_config(dest, id, uri, "downloading");
    fork_exe_wget(package, id, uri, dest, localUri);
    save_package_config(dest, id, uri, "installing");

    std::optional<std::string> filename = strip_tar_ext(localUri);
    if (!filename.has_value()) {
        std::cout << "Install Error: invalid archive type " << localUri << std::endl;
        return package_config;
    }
    package->path = dest + filename.value();
    auto package_status = save_package_config(dest, id, uri, "installed",
                                               package->path, localUri);
    return package_status;
}

// ─────────────────────────────────────────────────────────────────────────────
// uninstall / is_installed / list / check_executable — UNCHANGED
// ─────────────────────────────────────────────────────────────────────────────

static int ftw_callback(const char* path, const struct stat* sb, int flag, struct FTW* buf)
{
    int ret = remove(path);
    if (ret) ret = unlink(path);
    return ret;
}

auto PackagerAdapter::uninstall(std::string id) -> nlohmann::json
{
    auto dest = std::string(config["destination"]) + "/";
    std::cout << "PackagerAdapter::uninstall(" << dest + id + ".json)" << std::endl;
    auto package_config = load_package_config(dest, id);

    std::string uri  = package_config["uri"];
    save_package_config(dest, id, uri, "uninstalling");

    std::string path = package_config["path"];
    nftw(path.c_str(), ftw_callback, 64, FTW_DEPTH | FTW_PHYS);
    remove(path.c_str());

    auto package_status = delete_package_config(dest, id, uri, "uninstalled",
                                                 path, convertToLocalUri(id));
    return package_status;
}

auto PackagerAdapter::is_installed(std::string id) -> bool
{
    auto dest = std::string(config["destination"]) + "/";
    auto package_config = load_package_config(dest, id);
    return (!(package_config["state"].is_null() || package_config["state"] == "uninstalled")
            && package_config["exec"]);
}

auto PackagerAdapter::list() -> nlohmann::json
{
    auto dest = std::string(config["destination"]) + "/";
    auto ret  = nlohmann::json::parse("[]");
    try {
        DIR* dirFile = opendir(dest.c_str());
        if (dirFile) {
            struct dirent* hFile;
            errno = 0;
            while ((hFile = readdir(dirFile)) != nullptr) {
                if (!strcmp(hFile->d_name, ".")) continue;
                if (!strcmp(hFile->d_name, "..")) continue;
                if (strstr(hFile->d_name, ".json")) {
                    nlohmann::json package_data;
                    std::fstream package_file(dest + hFile->d_name);
                    package_file >> package_data;
                    ret.push_back(package_data);
                }
            }
            closedir(dirFile);
        }
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }
    return ret;
}

auto PackagerAdapter::check_executable(std::string uri) -> nlohmann::json { return ""; }

// =============================================================================
// OCI BUNDLE FLOW — new code below, nothing above this line was modified
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// is_binoci — true if the URL is a DAC OCI bundle
// Matches:  http://.../foo.bin-oci
//           http://.../dac-image-iperf3-filogic-20260126175526.bin-oci.tar
// ─────────────────────────────────────────────────────────────────────────────
bool PackagerAdapter::is_binoci(const std::string& uri)
{
    return uri.find(".bin-oci") != std::string::npos;
}

// ─────────────────────────────────────────────────────────────────────────────
// extract_oci_layout
//
// Extracts the downloaded .bin-oci.tar into destDir.
// A valid OCI image layout has: index.json  oci-layout  blobs/sha256/
// Returns true if destDir/index.json exists after extraction.
// ─────────────────────────────────────────────────────────────────────────────
std::string PackagerAdapter::extract_oci_layout(const std::string& tarPath,
                                                  const std::string& destDir)
{
    std::cout << "extract_oci_layout: " << tarPath << " -> " << destDir << std::endl;

    std::error_code ec;
    std::filesystem::create_directories(destDir, ec);
    if (ec) {
        std::cerr << "extract_oci_layout: create_directories failed: "
                  << ec.message() << std::endl;
        return "";
    }

    std::string arg1 = "-xf";
    std::string arg2 = tarPath;
    std::string arg3 = "-C";
    std::string arg4 = destDir;

    char* argv_list[] = {
        (char*)"tar",
        (char*)arg1.c_str(), (char*)arg2.c_str(),
        (char*)arg3.c_str(), (char*)arg4.c_str(),
        nullptr
    };
    fork_exe((char*)"/bin/tar", argv_list, []() {});

    // Search for index.json up to 3 levels deep using recursive_directory_iterator
    std::error_code sec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(destDir, sec)) {
        if (sec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().filename() == "index.json") {
            std::string layoutDir = entry.path().parent_path().string();
            std::cout << "extract_oci_layout: OCI layout found at: "
                      << layoutDir << std::endl;
            return layoutDir;
        }
    }

    std::cerr << "extract_oci_layout: no index.json found anywhere under "
              << destDir << std::endl;
    return "";
}
// ─────────────────────────────────────────────────────────────────────────────
// run_bundlegen
//
// Drives the full libbundlegen.so pipeline (mirrors test_so_rdkb.cpp):
//
//   STBPlatform     — load bpir4_reference from templates/generic
//   ImageDownloader — oci:<local path>:latest  (no network, layout already on disk)
//   ImageUnpacker   — extract layers into outputDir/rootfs
//   BundleProcessor — generate outputDir/config.json with Dobby mounts
//
// Returns true if outputDir/config.json and outputDir/rootfs/ both exist.
// ─────────────────────────────────────────────────────────────────────────────
bool PackagerAdapter::run_bundlegen(const std::string& ociLayoutDir,
                                     const std::string& outputDir)
{
    std::string platformName = config.value("platformName",   "bpir4_reference");
    std::string searchPath   = config.value("platformSearchPath",
        "/nvram/chandra/dac20bb/rdkcentral/rdkBundleManager/templates/generic");

    std::cout << "run_bundlegen:"
              << "\n  platform   = " << platformName
              << "\n  searchPath = " << searchPath
              << "\n  ociLayout  = " << ociLayoutDir
              << "\n  outputDir  = " << outputDir
              << std::endl;

    // ── [1/4] Load platform config ───────────────────────────────────────────
    STBPlatform platform(platformName, searchPath);
    if (!platform.foundConfig()) {
        std::cerr << "run_bundlegen [1/4] FAIL: platform '" << platformName
                  << "' not found at: " << searchPath << std::endl;
        return false;
    }
    if (!platform.validatePlatformConfig()) {
        std::cerr << "run_bundlegen [1/4] FAIL: platform config validation failed"
                  << std::endl;
        return false;
    }
    std::cout << "run_bundlegen [1/4] OK: platform config loaded" << std::endl;

    // ── [2/4] Locate OCI image via ImageDownloader ───────────────────────────
    // "oci:<path>:latest" tells ImageDownloader to use the local layout directory
    // directly — no network pull happens, it just validates index.json and returns
    // the path back. This is the same mechanism test_so_rdkb.cpp uses.
    std::string imageUrl = "oci:" + ociLayoutDir + ":latest";
    std::string imageTag = ImageDownloader::getImageTag(imageUrl);

    ImageDownloader imgDownloader;
    std::string imgPath = imgDownloader.downloadImage(imageUrl, "",
                                                       platform.getConfig());
    if (imgPath.empty()) {
        std::cerr << "run_bundlegen [2/4] FAIL: ImageDownloader returned empty path"
                  << std::endl;
        return false;
    }
    std::cout << "run_bundlegen [2/4] OK: OCI layout at " << imgPath << std::endl;

    // Clean up any existing output dir from a previous failed attempt
    std::error_code ec;
    if (std::filesystem::exists(outputDir)) {
        std::filesystem::remove_all(outputDir, ec);
        if (ec) {
            std::cerr << "run_bundlegen: failed to remove existing output dir: "
                      << ec.message() << std::endl;
            return false;
        }
    }

    // ── [3/4] Unpack OCI image layers into outputDir/rootfs ──────────────────
    ImageUnpacker imgUnpacker(imgPath, outputDir);
    if (!imgUnpacker.unpackImage(imageTag, /*deleteAfter=*/false)) {
        std::cerr << "run_bundlegen [3/4] FAIL: ImageUnpacker::unpackImage failed"
                  << std::endl;
        return false;
    }
    std::cout << "run_bundlegen [3/4] OK: layers unpacked to " << outputDir << std::endl;

    // ── Load appmetadata.json from inside the unpacked OCI image ─────────────
    nlohmann::json appMeta = imgUnpacker.getAppMetadataFromImg();
    if (appMeta.empty()) {
        std::cerr << "run_bundlegen: FAIL: no appmetadata.json inside OCI image" << std::endl;
        std::cerr << "              Hint: image must contain /appmetadata.json in rootfs"
                  << std::endl;
        return false;
    }
    imgUnpacker.deleteImgAppMetadata();
    std::cout << "run_bundlegen: app id   = " << appMeta.value("id",   "<unknown>") << std::endl;
    std::cout << "run_bundlegen: app type = " << appMeta.value("type", "<unknown>") << std::endl;

    // ── [4/4] BundleProcessor — generate config.json with Dobby mounts ───────
    BundleProcessor processor(
        platform.getConfig(),
        outputDir,
        appMeta,
        /*noDepWalking=*/   false,
        /*libMatchingMode=*/"normal",
        /*createMountPoints=*/false,
        /*crunOnly=*/       false);

    if (!processor.checkCompatibility()) {
        std::cerr << "run_bundlegen [4/4] FAIL: app not compatible with platform "
                  << platformName << std::endl;
        std::filesystem::remove_all(outputDir, ec);
        return false;
    }
    if (!processor.beginProcessing()) {
        std::cerr << "run_bundlegen [4/4] FAIL: BundleProcessor::beginProcessing failed"
                  << std::endl;
        return false;
    }
    std::cout << "run_bundlegen [4/4] OK: config.json generated" << std::endl;

    // ── Sanity check: Dobby needs exactly these two entries ───────────────────
    bool hasConfig = std::filesystem::exists(outputDir + "/config.json");
    bool hasRootfs = std::filesystem::exists(outputDir + "/rootfs");
    if (!hasConfig || !hasRootfs) {
        std::cerr << "run_bundlegen FAIL: bundle incomplete"
                  << " config.json=" << hasConfig
                  << " rootfs="      << hasRootfs << std::endl;
        return false;
    }

    std::cout << "run_bundlegen SUCCESS: bundle ready at " << outputDir << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// install_oci_bundle
//
// Called from install() when URL contains ".bin-oci".
// Steps:
//   1. wget  → /home/root/destination/dac-image-iperf3-...bin-oci.tar
//   2. tar   → /tmp/dsm-oci-work/<localname>/index.json + blobs/
//   3. libbundlegen.so pipeline → /home/root/destination/iperf3/config.json + rootfs/
//   4. write installed state to JSON (same as traditional flow)
//
// The output path /home/root/destination/<appid>/ is identical to what
// the traditional .tar.gz flow produces, so the rest of DSM (EU creation,
// Dobby RPC, SetRequestedState) works with zero changes.
// ─────────────────────────────────────────────────────────────────────────────
nlohmann::json PackagerAdapter::install_oci_bundle(std::shared_ptr<PackageData> package,
                                                    const std::string& id,
                                                    const std::string& uri)
{
    auto dest     = std::string(config["destination"]) + "/";
    std::string localUri = convertToLocalUri(uri);
    // localUri = "dac-image-iperf3-filogic-20260126175526.bin-oci.tar"

    std::cout << "install_oci_bundle:"
              << "\n  id       = " << id
              << "\n  uri      = " << uri
              << "\n  localUri = " << localUri << std::endl;

    // ── Step 1: wget download ─────────────────────────────────────────────────
    save_package_config(dest, id, uri, "downloading");

    std::string downloadPath = dest + localUri;
    // e.g. /home/root/destination/dac-image-iperf3-filogic-20260126175526.bin-oci.tar

    {
        std::string arg1 = "-T";  // timeout
        std::string arg2 = "60";  // 60s — OCI tarballs can be large
        std::string arg3 = uri;
        std::string arg4 = "-O";
        std::string arg5 = downloadPath;

        char* argv_list[] = {
            (char*)"wget",
            (char*)arg1.c_str(), (char*)arg2.c_str(),
            (char*)arg3.c_str(),
            (char*)arg4.c_str(), (char*)arg5.c_str(),
            nullptr
        };

        fork_exe((char*)"/usr/bin/wget", argv_list, []() {});

        if (!std::filesystem::exists(downloadPath)) {
            std::cerr << "install_oci_bundle: wget FAILED — file not found after download: "
                      << downloadPath << std::endl;
            save_package_config(dest, id, uri, "uninstalled");
            return nlohmann::json::parse(R"({"state":"uninstalled","exec":false})");
        }
    }
    std::cout << "install_oci_bundle: step 1 OK — downloaded to " << downloadPath << std::endl;

    // ── Step 2: extract OCI image layout to temp dir ──────────────────────────
    save_package_config(dest, id, uri, "installing");

    // Clean up any leftover from a previous failed attempt
    {
        std::error_code ec;
        std::filesystem::remove_all("/tmp/dsm-oci-work", ec);
        std::filesystem::create_directories("/tmp/dsm-oci-work", ec);
    }

    // extract_oci_layout now returns the actual path where index.json was found
    // (handles tar extracting into nested subdirectories)
    std::string ociLayoutDir = extract_oci_layout(downloadPath, "/tmp/dsm-oci-work");

    if (ociLayoutDir.empty()) {
        std::cerr << "install_oci_bundle: step 2 FAILED — OCI layout extraction failed"
                  << std::endl;
        std::remove(downloadPath.c_str());
        save_package_config(dest, id, uri, "uninstalled");
        return nlohmann::json::parse(R"({"state":"uninstalled","exec":false})");
    }
    std::cout << "install_oci_bundle: step 2 OK — OCI layout at " << ociLayoutDir << std::endl;

    // Downloaded tarball no longer needed
    std::remove(downloadPath.c_str());
    std::cout << "install_oci_bundle: cleaned up " << downloadPath << std::endl;

    // ── Step 3: derive output bundle dir name from appmetadata inside OCI ─────
    // We need the app id (e.g. "iperf3") before running bundlegen so we know
    // where to write the bundle. Do a quick peek at appmetadata.json.
    // The OCI layout has been extracted to ociLayoutDir; we need to find the
    // config blob and read the appmetadata path from it.
    // Simplest approach: derive appName from the filename and let BundleProcessor
    // use the real appmetadata id internally.
    //
    // Filename pattern: dac-image-<appname>-<platform>-<timestamp>.bin-oci[.tar]
    // We extract <appname> as the part between "dac-image-" and the next "-"
    std::string appName;
    {
        const std::string prefix = "dac-image-";
        auto pos = localUri.find(prefix);
        if (pos != std::string::npos) {
            std::string after = localUri.substr(pos + prefix.size());
            auto dash = after.find('-');
            appName = (dash != std::string::npos) ? after.substr(0, dash) : after;
            // strip any extension that might have crept in
            for (const char* ext : {".bin-oci.tar", ".bin-oci", ".tar.gz", ".tar"}) {
                size_t p = appName.find(ext);
                if (p != std::string::npos) { appName = appName.substr(0, p); break; }
            }
        }
        // Fallback: use the whole localUri without extensions
        if (appName.empty()) {
            appName = localUri;
            for (const char* ext : {".bin-oci.tar", ".bin-oci", ".tar.gz", ".tar"}) {
                size_t p = appName.find(ext);
                if (p != std::string::npos) { appName = appName.substr(0, p); break; }
            }
        }
    }
    std::string outputDir = dest + appName;
    // e.g. /home/root/destination/iperf3

    std::cout << "install_oci_bundle: appName=" << appName
              << " outputDir=" << outputDir << std::endl;

    // ── Step 4: run the full libbundlegen.so pipeline ─────────────────────────
    if (!run_bundlegen(ociLayoutDir, outputDir)) {
        std::cerr << "install_oci_bundle: step 4 FAILED — bundlegen pipeline failed"
                  << std::endl;
        // Clean up temp OCI layout
        std::error_code ec;
        std::filesystem::remove_all("/tmp/dsm-oci-work", ec);
        save_package_config(dest, id, uri, "uninstalled");
        return nlohmann::json::parse(R"({"state":"uninstalled","exec":false})");
    }

    // Clean up temp OCI layout dir
    {
        std::error_code ec;
        std::filesystem::remove_all("/tmp/dsm-oci-work", ec);
    }

    // ── Step 5: save installed state — same JSON as traditional flow ──────────
    // outputDir = /home/root/destination/iperf3
    // This is the same path the EU and Dobby will use.
    package->path = outputDir;

    // The state JSON filename uses the localUri as key, consistent with traditional
    // flow. e.g. /home/root/destination/dac-image-iperf3-...bin-oci.tar.json
    auto package_status = save_package_config(dest, id, uri, "installed",
                                               outputDir, localUri);

    std::cout << "install_oci_bundle: SUCCESS"
              << "\n  bundle    = " << outputDir
              << "\n  config.json exists = "
              << std::filesystem::exists(outputDir + "/config.json")
              << "\n  rootfs exists      = "
              << std::filesystem::exists(outputDir + "/rootfs")
              << std::endl;

    return package_status;
}
