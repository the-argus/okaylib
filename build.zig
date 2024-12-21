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
    "-DALLO_HEADER_TESTING",
    "-DALLO_HEADER_ONLY",

    // use ctti (default behavior)
    // "-DALLO_USE_RTTI",
    // "-DALLO_DISABLE_TYPEINFO",
    "-fno-rtti",
    // exceptions needed in testing mode
    // "-fno-exceptions",

    "-I./tests/",
    "-I./include/",

    // ziglike options
    // "-DZIGLIKE_HEADER_TESTING",
    // "-DZIGLIKE_USE_FMT",
    "-DFMT_HEADER_ONLY",
};

const test_source_files = &[_][]const u8{
    "stack_allocator_t/stack_allocator_t.cpp",
    "scratch_allocator_t/scratch_allocator_t.cpp",
    "block_allocator_t/block_allocator_t.cpp",
    "nonvirtual_inheritance/nonvirtual_inheritance.cpp",
    "heap_allocator_t/heap_allocator_t.cpp",
    "memory_map/memory_map.cpp",
    "collection_t/collection_t.cpp",
    "stack_t/stack_t.cpp",
    "list_t/list_t.cpp",
    "segmented_stack_t/segmented_stack_t.cpp",
};

const universal_tests_source_files = &[_][]const u8{
    "tests/generic_allocator_tests.cpp",
    "tests/heap_tests.cpp",
};

var ziglike: ?*std.Build.Dependency = null;

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
        .source_dir = b.path("include/allo/"),
        .install_dir = .header,
        .install_subdir = "allo/",
    });

    const main_header_install = b.addInstallHeaderFile(b.path("include/allo.h"), "allo.h");
    b.getInstallStep().dependOn(&main_header_install.step);

    {
        ziglike = b.dependency("ziglike", .{ .target = target, .optimize = optimize });
        const ziglike_include_path = b.pathJoin(&.{ ziglike.?.builder.install_path, "include" });
        try flags.append(b.fmt("-I{s}", .{ziglike_include_path}));
    }

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
        test_exe.step.dependOn(ziglike.?.builder.getInstallStep());
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
