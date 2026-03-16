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

#ifndef __PACKAGER_ADAPTER_HPP
#define __PACKAGER_ADAPTER_HPP

#include <string>

#include "../../ext/json.hpp"

struct PackageData;

class PackagerAdapter {
   nlohmann::json config;

  public:
   const std::string name{"local-tarball"};

   PackagerAdapter();
   void configure(nlohmann::json config);
   auto install(std::shared_ptr<PackageData> package, std::string id, std::string uri) -> nlohmann::json;
   auto uninstall(std::string id) -> nlohmann::json;
   auto is_installed(std::string id) -> bool;
   auto list() -> nlohmann::json;
   auto check_executable(std::string id) -> nlohmann::json;

  private:
   // ── Existing helpers (unchanged) ────────────────────────────────────────
   void fork_exe(char* path, char* const args[], const std::function<void()>& fn_callback);
   void fork_exe_wget(std::shared_ptr<PackageData> package, std::string id,
                      std::string uri, std::string dest, std::string localUri);
   void wget_callback_success(std::shared_ptr<PackageData> package, std::string id,
                               std::string uri, std::string dest, std::string localUri);

   // ── OCI bundle flow (new) ────────────────────────────────────────────────

   // Returns true if the URL points to a DAC OCI bundle (contains ".bin-oci")
   bool is_binoci(const std::string& uri);

   // Top-level OCI install handler — called from install() when is_binoci()==true
   nlohmann::json install_oci_bundle(std::shared_ptr<PackageData> package,
                                      const std::string& id,
                                      const std::string& uri);

   // Step 1: extract the downloaded .bin-oci.tar into destDir (OCI image layout)
   // Returns true if destDir/index.json exists after extraction

   std::string extract_oci_layout(const std::string& tarPath, const std::string& destDir);

   // Step 2: run the full libbundlegen.so pipeline on an already-extracted OCI layout.
   // ociLayoutDir  = e.g. /tmp/dsm-oci-work/iperf3-layout  (has index.json + blobs/)
   // outputDir     = e.g. /home/root/destination/iperf3     (bundle written here)
   // Returns true on success; outputDir will contain config.json + rootfs/
   bool run_bundlegen(const std::string& ociLayoutDir, const std::string& outputDir);
};

#endif // __PACKAGER_ADAPTER_HPP
