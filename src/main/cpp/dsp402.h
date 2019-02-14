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

#ifndef __DSP402_H__
#define __DSP402_H__

#include "robotkernel/kernel.h"
#include "robotkernel/module_base.h"
#include "robotkernel/stream.h"
#include <string>
#include "yaml-cpp/yaml.h"

namespace module_dsp402 {
#ifdef EMACS
}
#endif

class dsp402;

class dsp402_device : 
    public std::enable_shared_from_this<dsp402_device>,
    public robotkernel::pd_consumer,
    public robotkernel::pd_provider, 
    public robotkernel::trigger_base
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

        //! de-/construction
        dsp402_device(dsp402 *parent, const YAML::Node& node);
        ~dsp402_device();

        typedef struct {
            robotkernel::sp_trigger_t trigger;
            robotkernel::sp_process_data_t pd;
            size_t hash;
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
        void tick();
};

class dsp402 : 
    public std::enable_shared_from_this<dsp402>,
    public robotkernel::module_base
{
    public:
        //! de-/construction
        /*
         * \param name fts name
         * \param node YAML configuration node
         */
        dsp402(const char *name, const YAML::Node& node);
        ~dsp402();

        //! set fts state
        /*!
         * \param state new fts state
         */
        int set_state(module_state_t state);

        std::list<std::shared_ptr<dsp402_device> > devices;
};

#ifdef EMACS
{
#endif
}; // namespace module_dsp402

#endif /* __DSP402_H__ */

