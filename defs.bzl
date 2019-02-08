def cc_lib(**kwargs):
  native.cc_library(alwayslink = True, **kwargs)

def cc_target(name, intf_deps =[], impl_deps = [], **kwargs):
    native.cc_library(
        name = name,
        hdrs = [name + ".h"],
        deps = intf_deps,
        **kwargs)

    native.cc_library(
        name = name + "-impl",
        srcs = [name + ".cc"],
        deps = [name] + impl_deps,
        alwayslink = True,
        **kwargs)
