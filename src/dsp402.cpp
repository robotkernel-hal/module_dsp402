//! robotkernel module for dsp402 serial devices
/*!
 * author: Robert Burger <robert.burger@dlr.de>
 */

/*
 * This file is part of module_dsp402.
 *
 * module_dsp402 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * module_dsp402 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with module_dsp402; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <algorithm>
#include <cctype>

#include "dsp402.h"

#include "robotkernel/helpers.h"
#include "robotkernel/exceptions.h"

MODULE_DEF(module_dsp402, module_dsp402::dsp402);

using namespace std;
using namespace robotkernel;
using namespace robotkernel::helpers;
using namespace module_dsp402;

const static uint16_t STATUS_QUICK_STOP_MASK        = 0x0020;

const static uint16_t STATUS_MASK                   = 0x004F;
const static uint16_t STATUS_NOT_READY_TO_SWITCH_ON = 0x0000;
const static uint16_t STATUS_SWITCH_ON_DISABLED     = 0x0040;
const static uint16_t STATUS_READY_TO_SWITCH_ON     = 0x0001;
const static uint16_t STATUS_SWITCH_ON              = 0x0003;
const static uint16_t STATUS_OPERATION_ENABLED      = 0x0007;

const static uint16_t STATUS_FAULT                  = 0x0008;
const static uint16_t STATUS_FAULT_REACTION_ACTIVE  = 0x000F;

const static uint16_t CONTROL_MASK                  = 0x000F;
const static uint16_t CONTROL_SHUTDOWN              = 0x0006;
const static uint16_t CONTROL_SWITCH_ON             = 0x0007;
const static uint16_t CONTROL_ENABLE_OPERATION      = 0x000F;

const static uint16_t CONTROL_FAULT_RESET           = 0x0080;

static std::map<std::string, size_t> datatype_to_size = {
    { "uint8_t",  1 },
    { "uint16_t", 2 },
    { "uint24_t", 3 },
    { "uint32_t", 4 },
    { "uint40_t", 5 },
    { "uint48_t", 6 },
    { "uint56_t", 7 },
    { "uint64_t", 8 },
    { "int8_t",   1 },
    { "int16_t",  2 },
    { "int24_t",  3 },
    { "int32_t",  4 },
    { "int40_t",  5 },
    { "int48_t",  6 },
    { "int56_t",  7 },
    { "int64_t",  8 },
    { "float",    4 },
    { "double",   8 },
};

dsp402_device::dsp402_device(dsp402 *parent, const YAML::Node& node) :
    parent(parent)
{
    config = YAML::Clone(node);

    name = get_as<string>(node, "name");
    user_inputs_name    = get_as<string>(node, "pdin");
    user_outputs_name   = get_as<string>(node, "pdout");

    status_word_offset  = get_as<unsigned>(node, "status_word_offset", 0u);
    control_word_offset = get_as<unsigned>(node, "control_word_offset", 0u);

    status_word_name    = get_as<std::string>(node, "status_word_name", "statusword");
    control_word_name   = get_as<std::string>(node, "control_word_name", "controlword");

    prefix_entries      = get_as<bool>(node, "prefix_entries", true);
}

dsp402_device::~dsp402_device() {
}

std::string create_process_data_definition(const std::string& type_prefix, const std::string& input_definition, 
        off_t& local_offset, std::string field_name, off_t& field_offset) {
    std::transform(field_name.begin(), field_name.end(), field_name.begin(),                     
            [](unsigned char c){ return std::tolower(c); });

    auto node = YAML::Load(input_definition);

    YAML::Emitter emitter;
    emitter << YAML::BeginSeq;
    for (const auto& entry : node) {
        emitter << YAML::BeginMap;

        for (const auto& kv: entry) {
            
            std::string __datatype_name = kv.first.as<std::string>();
            std::string __field_name    = kv.second.as<std::string>();
            std::string __ifield_name   = __field_name;
            std::transform(__ifield_name.begin(), __ifield_name.end(), __ifield_name.begin(), 
                    [](unsigned char c){ return std::tolower(c); });


            if (
                    (__ifield_name == field_name) ||
                    ((field_name == "") && (field_offset == local_offset))) {
                field_offset = local_offset;

                emitter << YAML::Key << __datatype_name << YAML::Value << 
                    string_printf("%s.%s", type_prefix.c_str(), __field_name.c_str());
                emitter << YAML::EndMap << YAML::BeginMap;
                emitter << YAML::Key << "uint8_t" << YAML::Value << "dsp402_power";
                emitter << YAML::EndMap << YAML::BeginMap;
                emitter << YAML::Key << "uint8_t" << YAML::Value << "dsp402_brakes";
                emitter << YAML::EndMap << YAML::BeginMap;
                emitter << YAML::Key << "uint8_t" << YAML::Value << "dsp402_fault";
                
                local_offset += datatype_to_size[__datatype_name];
                local_offset += sizeof(dsp402_device::control_t);
            } else {
                if (type_prefix != "") {
                    emitter << YAML::Key << __datatype_name << YAML::Value << 
                        string_printf("%s.%s", type_prefix.c_str(), __field_name.c_str());
                } else {
                    emitter << YAML::Key << __datatype_name << YAML::Value <<  __field_name.c_str();
                }
                local_offset += datatype_to_size[__datatype_name];
            }
        }

        emitter << YAML::EndMap;
    }

    return std::string(emitter.c_str());
}

void dsp402_device::open() {
    user_inputs.pd = robotkernel::get_device<process_data>(user_inputs_name);
    user_inputs.consumer = make_shared<pd_consumer>(parent->name + "." + name + ".user_inputs");
    user_inputs.pd->set_consumer(user_inputs.consumer);
    
    user_outputs.pd = robotkernel::get_device<process_data>(user_outputs_name);
    user_outputs.provider = make_shared<pd_provider>(parent->name + "." + name + ".user_outputs");
    user_outputs.pd->set_provider(user_outputs.provider);

    off_t inputs_length = 0;
    std::string inputs_def = create_process_data_definition(prefix_entries ? user_inputs.pd->id() : "",
            user_inputs.pd->process_data_definition, inputs_length, 
            status_word_name, status_word_offset);
    inputs.trigger = make_shared<trigger_cb>(std::bind(&dsp402_device::tick_inputs, shared_from_this()));
    inputs.pd = make_shared<triple_buffer>(inputs_length, parent->name, name + ".inputs", inputs_def);
    inputs.provider = make_shared<pd_provider>(parent->name + "." + name + ".inputs");     
    inputs.pd->set_provider(inputs.provider);
    robotkernel::add_device(inputs.pd);

    off_t outputs_length = 0;
    std::string outputs_def = create_process_data_definition(prefix_entries ? user_outputs.pd->id() : "",
            user_outputs.pd->process_data_definition, outputs_length, 
            control_word_name, control_word_offset);
    outputs.trigger = make_shared<trigger_cb>(std::bind(&dsp402_device::tick_outputs_update, shared_from_this()));
    outputs.pd = make_shared<triple_buffer>(outputs_length, parent->name, name + ".outputs", outputs_def);
    outputs.consumer = make_shared<pd_consumer>(parent->name + "." + name + ".outputs");
    outputs.pd->set_consumer(outputs.consumer);

    parent->log(info, "%s: adding trigger to %s\n", name.c_str(), user_inputs.pd->trigger_dev->id().c_str());
    user_inputs.pd->trigger_dev->add_trigger(inputs.trigger);
    outputs.pd->trigger_dev->add_trigger(outputs.trigger);

    robotkernel::add_device(outputs.pd);
}

void dsp402_device::close() {
    parent->log(info, "%s: removing trigger to %s\n", name.c_str(), user_inputs.pd->trigger_dev->id().c_str());
    user_inputs.pd->trigger_dev->remove_trigger(inputs.trigger);
    inputs.trigger = nullptr;
    
    parent->log(info, "%s: removing trigger to %s\n", name.c_str(), outputs.pd->trigger_dev->id().c_str());
    outputs.pd->trigger_dev->remove_trigger(outputs.trigger);
    outputs.trigger = nullptr;

    outputs.pd->reset_consumer(outputs.consumer);
    robotkernel::remove_device(outputs.pd);
    outputs.consumer = nullptr;
    outputs.pd = nullptr;

    inputs.pd->reset_provider(inputs.provider);
    robotkernel::remove_device(inputs.pd);
    inputs.provider = nullptr;
    inputs.pd = nullptr;

    user_inputs.pd->reset_consumer(user_inputs.consumer);
    user_inputs.pd = nullptr;
    user_inputs.consumer = nullptr;

    user_outputs.pd->reset_provider(user_outputs.provider);
    user_outputs.pd = nullptr;
    user_outputs.provider = nullptr;
}


/*
USER_INPUTS (from device)       ->  INPUTS (our own device)
u16 : Statusword                    u16 : Statusword
                                    u8  : Power
                                    u8  : Brake
                                    u8  : Fault
u8  : ModeOfOperationDisplay        u8  : ModeOfOperationDisplay
u8  : Padding                       u8  : Padding
....

status_word_offset = 0


OUTPUTS (our own device)        ->  USER_OUTPUTS (to device)
u16 : Controlword                   u16 : Controlword
u8  : Power
u8  : Brake
u8  : Fault
u8  : ModeOfOperation               u8  : ModeOfOperation
u8  : Padding                       u8  : Padding
....

control_word_offset = 0

*/

void dsp402_device::tick_inputs() {
    auto inputs_buf = inputs.pd->next(inputs.provider);
    auto user_inputs_buf = user_inputs.pd->pop(user_inputs.consumer);

    control_t inputs_control;

    if (status_word_offset > 0) 
        memcpy(&inputs_buf[0], &user_inputs_buf[0], status_word_offset + 2); // copy with state word

    uint16_t status_word  = *(uint16_t *)&user_inputs_buf[status_word_offset];
    inputs_control.fault = (status_word & STATUS_FAULT) == STATUS_FAULT ? 1 : 0;

    switch (status_word & STATUS_MASK) {
        default: 
        case STATUS_SWITCH_ON_DISABLED:
        case STATUS_READY_TO_SWITCH_ON: // 0x0001
        case STATUS_SWITCH_ON:          // 0x0003
            inputs_control.power = 0;
            inputs_control.brakes = 1;
            break;
        case STATUS_OPERATION_ENABLED:  // 0x0007
            inputs_control.power = 1;
            inputs_control.brakes = 0;
            break;
    }

    // inputs
    memcpy(&inputs_buf[status_word_offset + 2], &inputs_control, sizeof(control_t));
    memcpy(&inputs_buf[status_word_offset + 2 + sizeof(control_t)],
            &user_inputs_buf[status_word_offset + 2], user_inputs.pd->length - status_word_offset - 2); 
    inputs.pd->push(inputs.provider);
}

void dsp402_device::tick_outputs_update() {
    auto outputs_buf = outputs.pd->pop(outputs.consumer);

    auto user_inputs_buf = user_inputs.pd->peek();
    auto user_outputs_buf = user_outputs.pd->next(user_outputs.provider);

    control_t &outputs_control = *(control_t *)&outputs_buf[control_word_offset + 2];

    if (control_word_offset > 0)
        memcpy(&user_outputs_buf[0], &outputs_buf[0], control_word_offset);

    uint16_t status_word  = *(uint16_t *)&user_inputs_buf[status_word_offset];
    uint16_t control_word = *(uint16_t *)&outputs_buf[control_word_offset];
            
    switch (status_word & STATUS_MASK) {
        default: 
            control_word = (control_word & ~CONTROL_MASK);
        case STATUS_SWITCH_ON_DISABLED:
            control_word = (control_word & ~CONTROL_MASK) | CONTROL_SHUTDOWN;
            break;
        case STATUS_READY_TO_SWITCH_ON: // 0x0001
            control_word = (control_word & ~CONTROL_MASK) | CONTROL_SWITCH_ON;
            break;
        case STATUS_SWITCH_ON:          // 0x0003
            if (outputs_control.power == 1) {
                control_word = (control_word & ~CONTROL_MASK) | CONTROL_ENABLE_OPERATION;
            } else {
                control_word = (control_word & ~CONTROL_MASK) | CONTROL_SWITCH_ON;
            }
            break;
        case STATUS_OPERATION_ENABLED:  // 0x0007
            if (outputs_control.power == 1) {
                control_word = (control_word & ~CONTROL_MASK) | CONTROL_ENABLE_OPERATION;
            } else {
                control_word = (control_word & ~CONTROL_MASK) | CONTROL_SWITCH_ON;
            }
            break;
    }

    if (status_word & STATUS_FAULT) {
        if (outputs_control.fault == 1) {
            control_word |= CONTROL_FAULT_RESET;
        }
    }

    // outputs
    memcpy(&user_outputs_buf[control_word_offset], &control_word, sizeof(control_word));
    memcpy(&user_outputs_buf[control_word_offset + sizeof(control_word)], 
            &outputs_buf[control_word_offset + sizeof(control_word) + sizeof(control_t)], 
            user_outputs.pd->length - control_word_offset - sizeof(control_word));
    user_outputs.pd->push(user_outputs.provider);
}

//! construction
/*
 * \param name fts name
 * \param node YAML configuration node
 */
dsp402::dsp402(const char *name, const YAML::Node& node) :
    module_base("module_dsp402", name, node) 
{
    this->config = YAML::Clone(node);
    set_state(module_state_init);
}

//! init func
void dsp402::init() {
    if (config["devices"]) {
        // backwards compability
        for (const auto& dev : config["devices"])
            devices.push_back(make_shared<dsp402_device>(this, dev)); 
    }

    std::list<YAML::Node> device_instances_list;
    parse_templates(config, device_instances_list);

    for (const auto& inst : device_instances_list) {
        devices.push_back(make_shared<dsp402_device>(this, inst)); 
    }

    for (const auto& dev : devices) {
        YAML::Emitter emitter;
        emitter << *dev;
        log(verbose, "got device: \n%s\n", emitter.c_str());
    }
}

//! destruction
dsp402::~dsp402() {
    // set to init, this will close dsp402
    set_state(module_state_init);
}

//! State transition from SAFEOP to PREOP
void dsp402::set_state_safeop_2_preop() {
    for (const auto& dev : devices)
        dev->close();
}

//! State transition from PREOP to SAFEOP
void dsp402::set_state_preop_2_safeop() {
    for (const auto& dev : devices) {
        dev->open();

        YAML::Emitter emitter;
        emitter << *dev;
        log(verbose, "opened device: \n%s\n", emitter.c_str());
    }
}

YAML::Emitter& operator<<(YAML::Emitter& out, const module_dsp402::dsp402_device& dev) {
    out << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << dev.name;
    out << YAML::Key << "pdin" << YAML::Value << dev.user_inputs_name;
    out << YAML::Key << "pdout" << YAML::Value << dev.user_outputs_name;
    out << YAML::Key << "status_word_offset" << YAML::Value << dev.status_word_offset;
    out << YAML::Key << "control_word_offset" << YAML::Value << dev.control_word_offset;
    out << YAML::Key << "status_word_name" << YAML::Value << dev.status_word_name;
    out << YAML::Key << "control_word_name" << YAML::Value << dev.control_word_name;
    out << YAML::EndMap;

    return out;
};

