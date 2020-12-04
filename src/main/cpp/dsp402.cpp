//! robotkernel module for dsp402 serial devices
/*!
 * author: Robert Burger <robert.burger@dlr.de>
 */

/*
 * This dsp402 is part of robotkernel.
 *
 * robotkernel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * robotkernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with robotkernel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cctype>

#include "dsp402.h"

#include "robotkernel/helpers.h"
#include "robotkernel/kernel.h"
#include "robotkernel/exceptions.h"

MODULE_DEF(module_dsp402, module_dsp402::dsp402);

using namespace std;
using namespace robotkernel;
using namespace module_dsp402;
using namespace string_util;

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
    pd_consumer(parent->name), pd_provider(parent->name), parent(parent)
{
    name = get_as<string>(node, "name");
    user_inputs_name    = get_as<string>(node, "pdin");
    user_outputs_name   = get_as<string>(node, "pdout");

    user_inputs_trigger_name  = get_as<string>(node, "pdin_trigger", "");
    user_outputs_trigger_name = get_as<string>(node, "pdout_trigger", "");

    status_word_offset  = get_as<unsigned>(node, "status_word_offset", 0u);
    control_word_offset = get_as<unsigned>(node, "control_word_offset", 0u);

    status_word_name    = get_as<std::string>(node, "status_word_name", "statusword");
    control_word_name   = get_as<std::string>(node, "control_word_name", "controlword");
}

dsp402_device::~dsp402_device() {
}

std::string create_process_data_definition(const std::string& input_definition, 
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

                emitter << YAML::Key << __datatype_name << YAML::Value << __field_name;
                emitter << YAML::Key << "uint8_t" << YAML::Value << "dsp402_power";
                emitter << YAML::EndMap << YAML::BeginMap;
                emitter << YAML::Key << "uint8_t" << YAML::Value << "dsp402_brakes";
                emitter << YAML::EndMap << YAML::BeginMap;
                emitter << YAML::Key << "uint8_t" << YAML::Value << "dsp402_fault";
                
                local_offset += datatype_to_size[__datatype_name];
                local_offset += sizeof(dsp402_device::control_t);
            } else {
                emitter << YAML::Key << __datatype_name << YAML::Value << __field_name;
                local_offset += datatype_to_size[__datatype_name];
            }
        }

        emitter << YAML::EndMap;
    }

    return std::string(emitter.c_str());
}

void dsp402_device::open() {
    kernel& k = *kernel::get_instance();
    
    user_inputs.pd       = k.get_process_data(user_inputs_name);
    user_inputs.hash     = user_inputs.pd->set_consumer(shared_from_this());
    
    if (user_inputs_trigger_name == "") {
        user_inputs_trigger_name = user_inputs.pd->clk_device;
    }

    user_inputs.trigger  = k.get_trigger(user_inputs_trigger_name);

    user_outputs.pd      = k.get_process_data(user_outputs_name);
    user_outputs.hash    = user_outputs.pd->set_provider(shared_from_this());
    
    if (user_outputs_trigger_name == "") {
        user_outputs_trigger_name = user_outputs.pd->clk_device;
    }

    user_inputs.trigger  = k.get_trigger(user_inputs_trigger_name);
    if (user_outputs_trigger_name != "") {
        user_outputs.trigger = k.get_trigger(user_outputs_trigger_name);
    }

    off_t inputs_length = 0;
    std::string inputs_def = create_process_data_definition(
            user_inputs.pd->process_data_definition, inputs_length, 
            status_word_name, status_word_offset);
    inputs.trigger       = make_shared<trigger>(parent->name, name + ".inputs");
    inputs.pd            = make_shared<triple_buffer>(inputs_length,
            parent->name, name + ".inputs", inputs_def, inputs.trigger->id());
    inputs.hash          = inputs.pd->set_provider(shared_from_this());

    k.add_device(inputs.trigger);
    k.add_device(inputs.pd);

    off_t outputs_length = 0;
    std::string outputs_def = create_process_data_definition(
            user_outputs.pd->process_data_definition, outputs_length, 
            control_word_name, control_word_offset);
    outputs.trigger      = make_shared<trigger>(parent->name, name + ".outputs");
    outputs.pd           = make_shared<triple_buffer>(outputs_length,
            parent->name, name + ".outputs", outputs_def, outputs.trigger->id());
    outputs.hash         = outputs.pd->set_consumer(shared_from_this());

    user_inputs.trigger->add_trigger(shared_from_this());

    k.add_device(outputs.trigger);
    k.add_device(outputs.pd);
}

void dsp402_device::close() {
    if (user_inputs.trigger) {
        user_inputs.trigger->remove_trigger(shared_from_this());
        user_inputs.trigger = nullptr;
    }

    kernel& k = *kernel::get_instance();
    k.remove_device(outputs.pd);
    outputs.pd = nullptr;
    k.remove_device(outputs.trigger);
    outputs.trigger = nullptr;

    k.remove_device(inputs.pd);
    inputs.pd = nullptr;
    k.remove_device(inputs.trigger);
    inputs.trigger = nullptr;
    
    
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

void dsp402_device::tick() {
    auto inputs_buf = inputs.pd->next(inputs.hash);
    auto outputs_buf = outputs.pd->pop(outputs.hash);

    auto user_inputs_buf = user_inputs.pd->pop(user_inputs.hash);
    auto user_outputs_buf = user_outputs.pd->next(user_outputs.hash);

    control_t inputs_control, outputs_control;

    if (status_word_offset > 0) 
        memcpy(&inputs_buf[0], &user_inputs_buf[0], status_word_offset + 2); // copy with state word
    if (control_word_offset > 0)
        memcpy(&user_outputs_buf[0], &outputs_buf[0], control_word_offset);

    memcpy(&outputs_control, &outputs_buf[control_word_offset + 2], sizeof(control_t));

    uint16_t status_word  = *(uint16_t *)&user_inputs_buf[status_word_offset];
    uint16_t control_word = *(uint16_t *)&outputs_buf[control_word_offset];

    switch (status_word & STATUS_MASK) {
        default: 
            inputs_control.fault = 0;;
            inputs_control.power = 0;
            inputs_control.brakes = 1;
            break;
        case STATUS_READY_TO_SWITCH_ON: // 0x0001
            inputs_control.fault = 0;;
            inputs_control.power = 0;
            inputs_control.brakes = 1;
            control_word = (control_word & ~CONTROL_MASK) | CONTROL_SHUTDOWN;
            break;
        case STATUS_SWITCH_ON:          // 0x0003
            inputs_control.fault = 0;;
            inputs_control.power = 0;
            inputs_control.brakes = 1;
            control_word = (control_word & ~CONTROL_MASK) | CONTROL_SWITCH_ON;
            break;
        case STATUS_OPERATION_ENABLED:  // 0x0007
            inputs_control.fault = 0;;
            inputs_control.power = 1;
            inputs_control.brakes = 0;

            if (outputs_control.power == 1)
                control_word = (control_word & ~CONTROL_MASK) | CONTROL_ENABLE_OPERATION;
            else 
                control_word = (control_word & ~CONTROL_MASK) | CONTROL_SWITCH_ON;
            break;
        case STATUS_FAULT:
        case STATUS_FAULT_REACTION_ACTIVE:
            inputs_control.fault = status_word & STATUS_MASK;
            inputs_control.power = 0;
            inputs_control.brakes = 1;
            break;
    }

    // inputs
    memcpy(&inputs_buf[status_word_offset + 2], &inputs_control, sizeof(control_t));
    memcpy(&inputs_buf[status_word_offset + 2 + sizeof(control_t)],
            &user_inputs_buf[status_word_offset + 2], user_inputs.pd->length - status_word_offset - 2); 
    inputs.pd->push(inputs.hash);
    inputs.trigger->trigger_modules();

    // outputs
    memcpy(&user_outputs_buf[control_word_offset], &control_word, sizeof(control_word));
    memcpy(&user_outputs_buf[control_word_offset + sizeof(control_word)], 
            &outputs_buf[control_word_offset + sizeof(control_t)], 
            outputs.pd->length - control_word_offset - sizeof(control_t));
    user_outputs.pd->push(user_outputs.hash);
}

//! construction
/*
 * \param name fts name
 * \param node YAML configuration node
 */
dsp402::dsp402(const char *name, const YAML::Node& node) :
    module_base("module_dsp402", name, node) 
{
    set_state(module_state_init);

    for (const auto& dev : node["devices"])
        devices.push_back(make_shared<dsp402_device>(this, dev)); 
}

//! destruction
dsp402::~dsp402() {
    // set to init, this will close dsp402
    set_state(module_state_init);
}

//! set fts state
/*!
 * \param state new fts state
 */
int dsp402::set_state(module_state_t state) {
    // get transition
    uint32_t transition = GEN_STATE(this->state, state);

    switch (transition) {
        case op_2_safeop:
        case op_2_preop:
        case op_2_init:
            // ====> stop sending commands
            if (    (transition == op_2_safeop))
                break;
        case safeop_2_preop:
        case safeop_2_init:
            // ====> stop receiving measurements
            for (const auto& dev : devices)
                dev->close();

            if (    (transition == op_2_preop) ||
                    (transition == safeop_2_preop))
                break;
        case preop_2_init:
            // ====> deinit devices
            
            // remove stream device
            //k.remove_device(shared_from_this());
        case init_2_init:
            // ====> do nothing
            break;

        case init_2_op:
        case init_2_safeop:
        case init_2_preop:
            // ====> initial devices            
            // add stream device
            //k.add_device(shared_from_this());

            if (    (transition == init_2_preop))
                break;
        case preop_2_op:
        case preop_2_safeop:
            // ====> start receiving measurements
            for (const auto& dev : devices)
                dev->open();

            if (    (transition == init_2_safeop) ||
                    (transition == preop_2_safeop))
                break;
        case safeop_2_op:
            // ====> start sending commands
            break;
        case op_2_op:
        case safeop_2_safeop:
        case preop_2_preop:
            // ====> do nothing
            break;

        default:
            break;
    }

    return (this->state = state);
}

