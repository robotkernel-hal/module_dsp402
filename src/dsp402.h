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

#ifndef MODULE_DSP402__DSP402_H
#define MODULE_DSP402__DSP402_H

#include "robotkernel/module_base.h"
#include "robotkernel/stream.h"
#include "robotkernel/process_data.h"

#include <string>

#include "yaml-cpp/yaml.h"

namespace module_dsp402 { class dsp402_device; };
YAML::Emitter& operator<<(YAML::Emitter& out, const module_dsp402::dsp402_device& dev);

namespace module_dsp402 {

class dsp402;

class trigger_cb : public robotkernel::trigger_base {
    public:
        std::function<void(void)> cb;

        trigger_cb(std::function<void(void)> cb) : cb(cb) {}

        //! trigger function
        void tick() { cb(); }
};

class dsp402_device : 
    public std::enable_shared_from_this<dsp402_device>
{
    public:
        dsp402 *parent;
        
        std::string name;
        std::string user_inputs_name;
        std::string user_outputs_name;
        off_t status_word_offset;
        off_t control_word_offset;
        std::string status_word_name;
        std::string control_word_name;
        bool prefix_entries = true;

        YAML::Node config;

        //! de-/construction
        dsp402_device(dsp402 *parent, const YAML::Node& node);
        ~dsp402_device();

        typedef struct {
            std::shared_ptr<trigger_cb> trigger;
            robotkernel::sp_process_data_t pd;
            robotkernel::sp_pd_provider_t provider;
            robotkernel::sp_pd_consumer_t consumer;
        } pd_t;

        pd_t user_inputs;     //!< inputs from other module
        pd_t user_outputs;    //!< outputs from other module

        typedef struct __attribute__((__packed__)) control {
            uint8_t power;
            uint8_t brakes;
            uint8_t fault;
        } __attribute__((__packed__)) control_t;

        pd_t inputs;          //!< own provided inputs
        pd_t outputs;         //!< own provided outputs

        void open();
        void close();
        
        //! trigger function
        void tick_inputs();
        void tick_outputs_update();
};

class dsp402 : 
    public std::enable_shared_from_this<dsp402>,
    public robotkernel::module_base
{
    private:
        YAML::Node config;

    public:
        //! de-/construction
        /*
         * \param name fts name
         * \param node YAML configuration node
         */
        dsp402(const char *name, const YAML::Node& node);
        ~dsp402();

        //! init func
        virtual void init() override;

        //! State transition from SAFEOP to PREOP
        virtual void set_state_safeop_2_preop() override;

        //! State transition from PREOP to SAFEOP
        virtual void set_state_preop_2_safeop() override;

        std::list<std::shared_ptr<dsp402_device> > devices;
};

}; // namespace module_dsp402

#endif /* MODULE_DSP402__DSP402_H */

