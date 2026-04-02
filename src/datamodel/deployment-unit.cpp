
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

#include "deployment-unit.hpp"

#include <iostream>

DeploymentUnit::DeploymentUnit(ExecutionEnvironment *parent_ee, std::string uri, std::shared_ptr<Packager> packager)
    : ee(parent_ee), 
      packager(packager),
      uid(uri),
      state(Packager::Undefined),      
      eu(nullptr),
      eu_path("") {
   std::cout << "<<create>> DeploymentUnit (" << uri << ")" << std::endl;
   // Derive a human-readable name from the filename portion of the URI
   std::string filename = uri;
   auto slash = uri.rfind('/');
   if (slash != std::string::npos) filename = uri.substr(slash + 1);
   auto dot = filename.rfind('.');
   if (dot != std::string::npos) filename = filename.substr(0, dot);
   du_name_        = filename;
   du_version_     = "";
   du_vendor_      = "";
   du_description_ = "";
   resolved_       = false;
   config["URI"]         = uri;
   config["UUID"]        = uid;
   config["Name"]        = du_name_;
   config["Version"]     = du_version_;
   config["Vendor"]      = du_vendor_;
   config["Description"] = du_description_;
   config["Resolved"]    = resolved_;
   packager->append_package(uid, uri);
   packager->on_update(uri,
                       [=](std::string uid, Packager::PackageState new_state) 
                       { 
                        std::cout << "on_package_update1 (" << uri << ")" << std::endl;
                        on_package_update(uid, new_state); 
                       });
}
DeploymentUnit::DeploymentUnit(ExecutionEnvironment *parent_ee, const PackageData &installed_package,
                               std::shared_ptr<Packager> packager)
    : ee(parent_ee),
      packager(packager), 
      uid(installed_package.ext_id), 
      state(Packager::Installed),
      eu(nullptr),
      eu_path("") {
   // Derive a human-readable name from the URI filename
   std::string filename = installed_package.uri;
   auto slash = filename.rfind('/');
   if (slash != std::string::npos) filename = filename.substr(slash + 1);
   auto dot = filename.rfind('.');
   if (dot != std::string::npos) filename = filename.substr(0, dot);
   du_name_        = filename;
   du_version_     = "";
   du_vendor_      = "";
   du_description_ = "";
   resolved_       = false;
   config["URI"]         = installed_package.uri;
   config["UUID"]        = installed_package.ext_id;
   config["Name"]        = du_name_;
   config["Version"]     = du_version_;
   config["Vendor"]      = du_vendor_;
   config["Description"] = du_description_;
   config["Resolved"]    = resolved_;
   packager->on_update(installed_package.uri,
                       [=](std::string uid, Packager::PackageState new_state) 
                       { 
                        std::cout << "on_package_update2 (" << uid << ")" << std::endl;
                        on_package_update(uid, new_state); 
                       });
}

DeploymentUnit::DeploymentUnit(ExecutionEnvironment *parent_ee, const DeploymentUnit &other)
    : ee(parent_ee), 
      packager(other.packager), 
      uid(other.uid), 
      state(other.state),
      eu(other.eu),
      eu_path(other.eu_path),
      du_name_(other.du_name_),
      du_version_(other.du_version_),
      du_vendor_(other.du_vendor_),
      du_description_(other.du_description_),
      resolved_(other.resolved_) {      
   std::cout << "<<copy>> DeploymentUnit (" << uid << ")" << std::endl;
   config = other.config;
   packager->on_update(config["URI"],
                       [=](std::string uid, Packager::PackageState new_state) 
                       { 
                        std::cout << "on_package_update3 (" << uid << ")" << std::endl;
                        on_package_update(uid, new_state); 
                       });
}

void DeploymentUnit::on_package_update(std::string updated_uid, Packager::PackageState new_state) {
   std::cout << "DeploymentUnit.on_package_update[" << get_uri() << "](status change: " 
             << Packager::state_to_string(state) << " -> " << Packager::state_to_string(new_state)
             << ")" << std::endl;
   state = new_state;
   
   auto info = packager->get_info(updated_uid);

   // Once installed, and no eu yet, and is executable, set up the EU
   if (info["exec"] && eu == nullptr && new_state == Packager::PackageState::Installed)
   {
      eu_path = info["path"];
      eu = std::make_shared<ExecutionUnit>(ee, this);
   }
}

auto DeploymentUnit::parent_ee() -> ExecutionEnvironment * { return ee; }

auto DeploymentUnit::get_uri() const -> std::string { return config["URI"]; };

auto DeploymentUnit::get_state() -> Packager::PackageState 
{ 
   return state; 
}

auto DeploymentUnit::get_duid() -> std::string { return uid; }

void DeploymentUnit::install() {
   std::cout << "DeploymentUnit.install(" << get_uri() << ")" << std::endl;
   packager->install(uid);
}

bool DeploymentUnit::uninstall() {
   std::cout << "DeploymentUnit.uninstall(" << get_uri() << ")" << std::endl;
   std::cout <<"      has_eu:"<<bool(eu)<<std::endl;

   if (eu){
      auto eu_state = eu->get_state();
      if (eu_state == ContainerRuntime::Undefined || eu_state == ContainerRuntime::Idle ){
         eu.reset();
         return packager->uninstall(uid);
      }else{
         std::cout << "      error: cannot uninstall because EU is not Undefined or Idle"<<std::endl;
         return false;
      }
   }
   
   return false;
}

auto DeploymentUnit::to_json() -> nlohmann::json {
   config["Name"]        = du_name_;
   config["Version"]     = du_version_;
   config["Vendor"]      = du_vendor_;
   config["Description"] = du_description_;
   config["Resolved"]    = resolved_;
   return config;
}

auto DeploymentUnit::get_detail() -> nlohmann::json {
   auto detail = config;
   detail["parent-ee"] = ee->name();
   detail["state"] = Packager::state_to_string(state);
   detail["eu"] = false;

   if (has_eu()){
      detail["eu"] = eu->get_uid();
      detail["eu.path"] = eu_path;
   }
   detail["Name"]             = du_name_;
   detail["Version"]          = du_version_;
   detail["Vendor"]           = du_vendor_;
   detail["Description"]      = du_description_;
   detail["Resolved"]         = resolved_;
   detail["ExecutionEnvRef"]  = (ee != nullptr) ? ee->name() : "";
   detail["ExecutionUnitList"] = get_eu_list_str();
   return detail; 
}
auto DeploymentUnit::has_eu() -> bool { return eu != nullptr; }

auto DeploymentUnit::get_eu_path() -> std::string { return eu_path;}

auto DeploymentUnit::get_eu() -> ExecutionUnit*{
   return eu.get();
}

auto DeploymentUnit::get_name()        const -> std::string { return du_name_; }
auto DeploymentUnit::get_version()     const -> std::string { return du_version_; }
auto DeploymentUnit::get_vendor()      const -> std::string { return du_vendor_; }
auto DeploymentUnit::get_description() const -> std::string { return du_description_; }
auto DeploymentUnit::is_resolved()     const -> bool { return resolved_; }

auto DeploymentUnit::get_exec_env_ref() const -> std::string {
   return (ee != nullptr) ? ee->name() : "";
}

auto DeploymentUnit::get_eu_list_str() const -> std::string {
   if (eu == nullptr) return "";
   return eu->get_uid();
}
