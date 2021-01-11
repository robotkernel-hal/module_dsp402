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
