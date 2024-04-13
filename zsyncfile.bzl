"""zsyncfile rule for Bazel."""

def _is_power_of_two(n):
    return (n != 0) and (n & (n - 1) == 0)

def _zsyncfile_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    args = []

    # Set Filename header "short path" of the file. Without this option, zsyncmake would use the basename.
    args += ["-f", ctx.file.file.short_path]
    args += ["-o", out.path]

    # zsync3: Do not set MTime header to make sure our zsync files are reproducible.
    args.append("-M")

    if ctx.attr.blocksize:
        # By default, zsync will use a block size of 2048, or 4096 if the file is larger than 100MB.
        if ctx.attr.blocksize < 0 or not _is_power_of_two(ctx.attr.blocksize):
            fail("If set, blocksize must be >0 and a power of two")
        args += ["-b", str(ctx.attr.blocksize)]

    # Yes zsync supports multiple URLs but I think that's stupid.
    args += ["-u", ctx.attr.url or ctx.file.file.basename]
    args.append(ctx.file.file.path)
    ctx.actions.run(
        inputs = [ctx.file.file],
        outputs = [out],
        arguments = args,
        executable = ctx.executable.zsyncmake,
        tools = [ctx.executable.zsyncmake],
    )
    return DefaultInfo(files = depset([out]))

zsyncfile = rule(
    implementation = _zsyncfile_impl,
    attrs = {
        "url": attr.string(mandatory = False),
        "file": attr.label(mandatory = True, allow_single_file = True),
        "blocksize": attr.int(mandatory = False),
        "zsyncmake": attr.label(
            default = Label("@zsync3//:zsyncmake"),
            executable = True,
            cfg = "exec",
        ),
    },
)
