# module_dsp402

```yaml
# Configuration file for PULSAR DSP402 module.
#
# vi: set ft=yaml nowrap:
# -*- mode: yaml -*-

#########################################################
# logging settings

# Standard robotkernel loglevel.
loglevel: info

#########################################################
# devices
devices:
- 
  # Local instance Name
  name: axis_0
  
  # Name of the input process data
  pdin: ecat.slave_1.inputs.pd

  # Trigger of the input process data
  pdin_trigger: ecat.slave_1.inputs.trigger

  # Name of the output process data
  pdout: ecat.slave_1.outputs.pd

  # Trigger of the output process data
  pdout_trigger: ecat.slave_1.outputs.trigger

  # Name of the Status Word in <pdin>
  status_word_name: Statusword

  # Name of the Control Word in <pdout>
  control_word_name: Controlword
```
