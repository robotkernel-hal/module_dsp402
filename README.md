# module_dsp402

This module implements the CiA DSP 402 state machine. Therefore it takes an input and output process data with the statusword and the controlword and exports new process data to control the state machine in an easier way.

# DSP402 control state machine module

```yaml
- name: dsp402
  so_file: libmodule_dsp402.so
  config:
    devices:
    - name: axis_0
      pdin: ecat.slave_1.inputs.pd
      pdin_trigger: ecat.slave_1.inputs.trigger
      pdout: ecat.slave_1.outputs.pd
      pdout_trigger: ecat.slave_1.outputs.trigger
      status_word_name: Statusword
      control_word_name: Controlword
  power_up: op
  depends: [ ecat ]
```

```yaml
- name: dsp402
  so_file: libmodule_dsp402.so
  config:
    classes:
      my_axis:
        name: ($axis_name)
        pdin: ($inputs_name).pd
        pdin_trigger: ($inputs_name).trigger
        pdout: ($outputs_name).pd
        pdout_trigger: ($outputs_name).trigger
        status_word_name: Statusword
        control_word_name: Controlword
    instances:
    - { use_class: my_axis, axis_name: axis_1, inputs_name: ecat.slave_1.inputs, outputs_name: ecat.slave_1.outputs }
    - { use_class: my_axis, axis_name: axis_2, inputs_name: ecat.slave_2.inputs, outputs_name: ecat.slave_2.outputs }
    - { use_class: my_axis, axis_name: axis_3, inputs_name: ecat.slave_3.inputs, outputs_name: ecat.slave_3.outputs }
  power_up: op
  depends: [ ecat ]
```

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
