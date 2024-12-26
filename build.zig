const std = @import("std");
const builtin = @import("builtin");
const zcc = @import("compile_commands");

const release_flags = &[_][]const u8{
    "-DNDEBUG",
    "-std=c++17",
};

const debug_flags = &[_][]const u8{
    "-g",
    "-std=c++17",
};

const testing_flags = &[_][]const u8{
    "-DOKAYLIB_HEADER_TESTING",

    // use ctti (default behavior)
    // "-DOKAYLIB_ALLOCATORS_USE_RTTI",
    // "-DOKAYLIB_ALLOCATORS_DISABLE_TYPECHECKING",
    "-fno-rtti",

    "-I./tests/",
    "-I./include/",
    "-DOKAYLIB_NOEXCEPT=", // allow exceptions in testing mode

    "-DOKAYLIB_TESTING",
    "-DOKAYLIB_USE_FMT",
    "-DFMT_HEADER_ONLY",
};

const okaylib_top_level_headers = &[_][]const u8{
    "anystatus.h",
    "context.h",
    "defer.h",
    "opt.h",
    "res.h",
    "short_arithmetic_types.h",
    "slice.h",
    "status.h",
    "stdmem.h",
};

const test_source_files = &[_][]const u8{
    "defer/defer.cpp",
    "enumerate/enumerate.cpp",
    "opt/opt.cpp",
    "res/res.cpp",
    "slice/slice.cpp",
    "status/status.cpp",
    "stdmem/stdmem.cpp",
};

const universal_tests_source_files = &[_][]const u8{};

pub fn build(b: *std.Build) !void {
    // options
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    var flags = std.ArrayList([]const u8).init(b.allocator);
    defer flags.deinit();
    try flags.appendSlice(if (optimize == .Debug) debug_flags else release_flags);
    try flags.appendSlice(testing_flags);

    var tests = std.ArrayList(*std.Build.Step.Compile).init(b.allocator);
    defer tests.deinit();

    // actual public installation step
    b.installDirectory(.{
        .source_dir = b.path("include/okay/"),
        .install_dir = .header,
        .install_subdir = "okay/",
    });

    for (okaylib_top_level_headers) |header_filename| {
        const install = b.addInstallHeaderFile(b.path(b.pathJoin(&.{ "include", header_filename })), header_filename);
        b.getInstallStep().dependOn(&install.step);
    }

    const fmt = b.dependency("fmt", .{});
    const fmt_include_path = b.pathJoin(&.{ fmt.builder.install_path, "include" });
    try flags.append(b.fmt("-I{s}", .{fmt_include_path}));

    const flags_owned = flags.toOwnedSlice() catch @panic("OOM");

    for (test_source_files) |source_file| {
        var test_exe = b.addExecutable(.{
            .name = std.fs.path.stem(source_file),
            .optimize = optimize,
            .target = target,
        });
        test_exe.addCSourceFile(.{
            .file = b.path(b.pathJoin(&.{ "tests", source_file })),
            .flags = flags_owned,
        });
        test_exe.addCSourceFiles(.{
            .files = universal_tests_source_files,
            .flags = flags_owned,
        });
        test_exe.linkLibCpp();
        test_exe.step.dependOn(fmt.builder.getInstallStep());
        try tests.append(test_exe);
    }

    const run_tests_step = b.step("run_tests", "Compile and run all the tests");
    const install_tests_step = b.step("install_tests", "Install all the tests but don't run them");
    for (tests.items) |test_exe| {
        const test_install = b.addInstallArtifact(test_exe, .{});
        install_tests_step.dependOn(&test_install.step);

        const test_run = b.addRunArtifact(test_exe);
        if (b.args) |args| {
            test_run.addArgs(args);
        }
        run_tests_step.dependOn(&test_run.step);
    }

    zcc.createStep(b, "cdb", try tests.toOwnedSlice());
}
