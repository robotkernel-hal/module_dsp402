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

#include "moddsp402.h"
#include "robotkernel/helpers.h"
#include "robotkernel/kernel.h"
#include "robotkernel/exceptions.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/stat.h>

MODULE_DEF(module_dsp402, module_dsp402::dsp402);

using namespace std;
using namespace robotkernel;
using namespace module_dsp402;
using namespace string_util;

//! construction
/*
 * \param name fts name
 * \param node YAML configuration node
 */
dsp402::dsp402(const char *name, const YAML::Node& node) :
    module_base("module_dsp402", name, node),
    stream(name, "dsp402")
{
    fd                    = -1;
    dsp402_name             = get_as<std::string>(node, "dsp402_name");

    set_state(module_state_init);
}

//! destruction
dsp402::~dsp402() {
    // set to init, this will close dsp402
    set_state(module_state_init);
}

size_t dsp402::read(void* buf, size_t bufsize) {
    if (state < module_state_safeop) {
        log(warning, "invalid state for reading data\n");
        // invalid state
        return 0;
    }

    return ::read(fd, buf, bufsize);
}

size_t dsp402::write(void* buf, size_t bufsize) {
    if (state < module_state_op)
        // invalid state
        return 0;

    return ::write(fd, buf, bufsize);
}

//! set fts state
/*!
 * \param state new fts state
 */
int dsp402::set_state(module_state_t state) {
    kernel& k = *kernel::get_instance();

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
            if (    (transition == op_2_preop) ||
                    (transition == safeop_2_preop))
                break;
        case preop_2_init:
            // ====> deinit devices
            
            // remove stream device
            k.remove_device(shared_from_this());

            close(fd);
            fd = -1;
        case init_2_init:
            // ====> do nothing
            break;

        case init_2_op:
        case init_2_safeop:
        case init_2_preop:
            // ====> initial devices            
            log(info, "opening dsp402 %s ...\n", dsp402_name.c_str());

            fd = open(dsp402_name.c_str(), O_RDWR | O_CREAT);
            if (fd == -1)
                throw str_exception("open %s: %s", dsp402_name.c_str(), strerror(errno));
            
            // add stream device
            k.add_device(shared_from_this());

            if (    (transition == init_2_preop))
                break;
        case preop_2_op:
        case preop_2_safeop:
            // ====> start receiving measurements
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

