
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

#include "execution-unit.hpp"
#include "../utils/uuid_generator.hpp"
#include <iostream>
#include <cstring>
ExecutionUnit::ExecutionUnit(ExecutionEnvironment *parent_ee, DeploymentUnit *parent_du)
        :ee(parent_ee),
         du(parent_du),
         uid(generate_UUID(8)),
         state(ContainerRuntime::Idle)
        {
    std::cout<< "<<create>> ExecutionUnit ["<< uid <<"] EE:"<< parent_ee->name() <<"  DU:"<< parent_du->get_duid() <<std::endl;
    std::cout<< "           Path: ["<< du->get_eu_path()<<"]" <<std::endl;

    // Derive EU name from parent DU name for better identification
    std::string duid = du->get_duid();
    auto slash = duid.rfind('/');
    std::string base = (slash != std::string::npos) ? duid.substr(slash + 1) : duid;
    // Strip common archive extensions
    bool stripped = true;
    while (stripped) {
        stripped = false;
        for (const char* ext : {".bin-oci.tar", ".tar.gz", ".bin-oci", ".tar", ".gz"}) {
            if (base.size() > strlen(ext) &&
                base.compare(base.size() - strlen(ext), strlen(ext), ext) == 0) {
                base = base.substr(0, base.size() - strlen(ext));
                stripped = true;
                break;
            }
        }
    }
    eu_name_        = base.empty() ? uid : base;
    eu_vendor_      = "";
    eu_version_     = "";
    eu_description_ = "";
    exec_env_label_ = eu_name_;
    auto_start_     = false;
    run_level_      = -1; 
}

ExecutionUnit::~ExecutionUnit()
{
    std::cout<< "<<destroy>> ExecutionUnit ["<< uid <<"]" <<std::endl;
}

void ExecutionUnit::start(){
    std::cout<<"["<<uid<<"].ExecutionUnit::start(path:"<<du->get_eu_path()<<")" << std::endl;
    ee->get_runtime()->start(uid, du->get_eu_path());
    state = ContainerRuntime::Starting;
}

void ExecutionUnit::stop(){
    std::cout<<"["<<uid<<"].ExecutionUnit::stop(path:"<<du->get_eu_path()<<")" << std::endl;
    ee->get_runtime()->stop(uid);
    state = ContainerRuntime::Stopping;
}

void ExecutionUnit::pause(){
    std::cout<<"["<<uid<<"].ExecutionUnit::pause()" << std::endl;
    ee->get_runtime()->pause(uid);
}

void ExecutionUnit::resume(){
    std::cout<<"["<<uid<<"].ExecutionUnit::resume()" << std::endl;
    ee->get_runtime()->resume(uid);
}

auto ExecutionUnit::get_state() -> ContainerRuntime::ContainerState {
    ContainerRuntime *runtime = ee->get_runtime();
    auto info =  runtime->getInfo(uid);
    std::string state =  info["status"];
    std::cout<< "ExecutionUnit::get_state state=" << state.c_str() << std::endl;
    if (state == "Idle") 
    {
        return ContainerRuntime::Idle;
    }
    if (state == "Active")
    {
      return ContainerRuntime::Active;  
    } 
    if (state == "Starting") 
    {
        return ContainerRuntime::Starting;
    }
    if (state == "Stopping") 
    {
        return ContainerRuntime::Stopping;
    }
    return ContainerRuntime::Undefined;
}


auto ExecutionUnit::get_detail() -> nlohmann::json {
    ContainerRuntime *runtime = ee->get_runtime();
    nlohmann::json ret_detail = runtime->getInfo(uid);

        ret_detail["uid"]          = uid;
    ret_detail["ee"]           = ee->name();
    ret_detail["du"]           = du->get_duid();    
    ret_detail["path"]         = du->get_eu_path();
    ret_detail["Name"]         = eu_name_;
    ret_detail["Vendor"]       = eu_vendor_;
    ret_detail["Version"]      = eu_version_;
    ret_detail["Description"]  = eu_description_;
    ret_detail["ExecEnvLabel"] = exec_env_label_;
    ret_detail["AutoStart"]    = auto_start_;
    ret_detail["RunLevel"]     = run_level_;
    ret_detail["References"]   = du->get_duid();
    return ret_detail;
}
auto ExecutionUnit::get_uid() -> std::string{
    return uid;
}
auto ExecutionUnit::get_name()           const -> std::string { return eu_name_; }
auto ExecutionUnit::get_vendor()         const -> std::string { return eu_vendor_; }
auto ExecutionUnit::get_version()        const -> std::string { return eu_version_; }
auto ExecutionUnit::get_description()    const -> std::string { return eu_description_; }
auto ExecutionUnit::get_exec_env_label() const -> std::string { return exec_env_label_; }
auto ExecutionUnit::get_auto_start()     const -> bool { return auto_start_; }
auto ExecutionUnit::get_run_level()      const -> int { return run_level_; }
auto ExecutionUnit::set_auto_start(bool val) -> void { auto_start_ = val; }
auto ExecutionUnit::set_run_level(int rl)    -> void { run_level_ = rl; }
