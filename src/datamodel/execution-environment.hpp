
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

#ifndef __EXECUTION_ENVIROMENT_HPP
#define __EXECUTION_ENVIROMENT_HPP

#include <memory>
#include <vector>

#include "../ext/json.hpp"
#include "deployment-unit.hpp"
#include "execution-unit.hpp"
#include "bridge/packager.hpp"
#include "bridge/container-runtime.hpp"

class DeploymentUnit;
class ExecutionUnit;

class ExecutionEnvironment {
   nlohmann::json config;
   std::shared_ptr<Packager> packager {nullptr};
   std::shared_ptr<ContainerRuntime> runtime {nullptr};
   std::vector<std::shared_ptr<DeploymentUnit> > du_list;

   // TR-181 Layer 1 metadata
   std::string type_;
   std::string vendor_;
   std::string version_;
   int         allocated_disk_space_{ -1 };
   int         available_disk_space_{ -1 };
   int         allocated_memory_{ -1 };
   int         available_memory_{ -1 };
   int         requested_run_level_{ -1 };
   int         run_level_at_boot_{ 5 };

  public:
   ExecutionEnvironment(nlohmann::json config, std::shared_ptr<Packager> packager, std::shared_ptr<ContainerRuntime> runtime);
   ~ExecutionEnvironment() = default;

   auto id() const -> unsigned int;
   auto name() const -> std::string;
   auto enable() const -> bool;
   auto set_enable(bool en) -> void;
   auto status() const -> std::string;
   auto initial_runlevel() const -> int;
   auto set_initial_runlevel(int rl) -> void;
   auto current_runlevel() const -> int;
   auto type()                  const -> std::string;
   auto vendor()                const -> std::string;
   auto version()               const -> std::string;
   auto allocated_disk_space()  const -> int;
   auto available_disk_space()  const -> int;
   auto allocated_memory()      const -> int;
   auto available_memory()      const -> int;
   auto requested_run_level()   const -> int;
   auto set_requested_run_level(int rl) -> void;
   auto run_level_at_boot()     const -> int;
   auto install(std::string uri) -> std::shared_ptr<DeploymentUnit>;
   auto add_existing(PackageData &installed_package) -> std::shared_ptr<DeploymentUnit>;
   auto has_du_in_config(std::string uid) -> bool;
   auto find_deployment_unit(std::string uri) -> std::shared_ptr<DeploymentUnit>;
   auto to_json() -> nlohmann::json;
   auto is_default() -> bool;
   auto get_eu_list() -> std::vector<ExecutionUnit *> ;
   auto get_runtime() -> ContainerRuntime*;
};

#endif
