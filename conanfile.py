from conan import ConanFile

class MainProject(ConanFile):
    python_requires = "conan_template/[~6]@robotkernel/unstable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "module_dsp402"
    description = "Handling of CiA DSP 402 drive profile."
    exports_sources = ["*", "!.gitignore"]
    requires = ["robotkernel/[~6]@robotkernel/unstable", ]

