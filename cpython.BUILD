cc_library(
    name = "python_h",
    srcs = [":pyconfig.h"] + glob(["Include/*.h"]),
    hdrs = ["Include/Python.h"],
    visibility = ["//visibility:public"],
)

genrule(
    name = "pyconfig",
    srcs = glob([
        "**/*.in",
        "Include/*.h",
        "Modules/**",
        "config.guess",
        "config.sub",
        "configure",
        "install-sh",
    ]),
    outs = ["pyconfig.h"],
    cmd = """
      ucs=$$(python -c "import sysconfig; print sysconfig.get_config_var('Py_UNICODE_SIZE')")
      cfgscr=$$PWD/$(location :configure)
      cd $(@D)
      $$cfgscr --enable-unicode=ucs$$ucs
    """,
)
