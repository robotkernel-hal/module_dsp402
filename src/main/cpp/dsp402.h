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

class dsp402 : 
    public std::enable_shared_from_this<dsp402>,
    public robotkernel::module_base,
    public robotkernel::stream
{
    public:
        std::string     dsp402_name;     //!< dsp402 name
        int             fd;            //!< dsp402 descriptor

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

        //! cyclic process data read
        /*!
         * \param buf process data buffer
         * \param bufsize size of process data buffer
         * \return size of read bytes
         */
        size_t read(void* buf, size_t bufsize);

        //! cyclic process data write
        /*!
         * \param buf process data buffer
         * \param bufsize size of process data buffer
         * \return size of written bytes
         */
        size_t write(void* buf, size_t bufsize);
};

#ifdef EMACS
{
#endif
}; // namespace module_dsp402

#endif /* __DSP402_H__ */

