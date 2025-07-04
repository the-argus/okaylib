const std = @import("std");
const builtin = @import("builtin");
const zcc = @import("compile_commands");

const release_flags = &[_][]const u8{
    "-DNDEBUG",
    "-std=c++20",
};

const debug_flags = &[_][]const u8{
    "-g",
    "-std=c++20",
    // TODO: fix tests to only use non throwing types, so tests still run and
    // compile with disallow exceptions on
    // "-DOKAYLIB_DISALLOW_EXCEPTIONS",
};

const testing_flags = &[_][]const u8{
    "-DOKAYLIB_HEADER_TESTING",

    // use ctti (default behavior)
    // "-DOKAYLIB_ALLOCATORS_USE_RTTI",
    // "-DOKAYLIB_ALLOCATORS_DISABLE_TYPECHECKING",
    "-fno-rtti",
    "-Werror",
    "-Wno-deprecated",
    "-DOKAYLIB_NOEXCEPT=", // allow exceptions in testing mode

    "-DOKAYLIB_TESTING",
    // "-DOKAYLIB_USE_FMT",
    "-DFMT_HEADER_ONLY",
};

const okaylib_headers = &[_][]const u8{
    "anystatus.h",
    "construct.h",
    "context.h",
    "defer.h",
    "opt.h",
    "res.h",
    "short_arithmetic_types.h",
    "slice.h",
    "status.h",
    "stdmem.h",
    "version.h",

    "allocators/allocator.h",
    "allocators/arena.h",
    "allocators/block_allocator.h",
    "allocators/c_allocator.h",
    "allocators/destruction_callbacks.h",
    "allocators/linked_blockpool_allocator.h",
    "allocators/linked_slab_allocator.h",
    "allocators/page_allocator.h",
    "allocators/reserving_page_allocator.h",
    "allocators/slab_allocator.h",
    "allocators/std_memory_resource_allocator.h",
    "allocators/wrappers.h",

    "containers/array.h",
    "containers/arraylist.h",
    "containers/bit_arraylist.h",
    "containers/bit_array.h",
    "containers/fixed_arraylist.h",
    "containers/segmented_list.h",

    "detail/template_util/c_array_length.h",
    "detail/template_util/c_array_value_type.h",
    "detail/template_util/empty.h",
    "detail/template_util/first_type_in_pack.h",
    "detail/template_util/is_all_same.h",
    "detail/template_util/remove_cvref.h",
    "detail/template_util/uninitialized_storage.h",

    "detail/traits/is_std_container.h",
    "detail/traits/is_derived_from.h",
    "detail/traits/is_instance.h",
    "detail/traits/is_status_enum.h",
    "detail/traits/mathop_traits.h",
    "detail/traits/special_member_traits.h",

    "detail/abort.h",
    "detail/addressof.h",
    "detail/get_best.h",
    "detail/invoke.h",
    "detail/is_lvalue.h",
    "detail/no_unique_addr.h",
    "detail/noexcept.h",
    "detail/ok_assert.h",
    "detail/ok_unreachable.h",
    "detail/view_common.h",
    "detail/memory.h",

    "macros/foreach.h",
    "macros/try.h",

    "math/math.h",
    "math/ordering.h",
    "math/rounding.h",

    "platform/memory_map.h",

    "ranges/adaptors.h",
    "ranges/algorithm.h",
    "ranges/for_each.h",
    "ranges/ranges.h",
    "ranges/indices.h",

    "ranges/views/all.h",
    "ranges/views/any.h",
    "ranges/views/drop.h",
    "ranges/views/enumerate.h",
    "ranges/views/join.h",
    "ranges/views/keep_if.h",
    "ranges/views/reverse.h",
    // "ranges/views/sliding_window.h",
    "ranges/views/std_for.h",
    "ranges/views/take_at_most.h",
    "ranges/views/transform.h",
    "ranges/views/zip.h",

    "smart_pointers/arc.h",
};

const test_source_files = &[_][]const u8{
    "defer/defer.cpp",
    "enumerate/enumerate.cpp",
    "transform/transform.cpp",
    "keep_if/keep_if.cpp",
    "take_at_most/take_at_most.cpp",
    "drop/drop.cpp",
    "reverse/reverse.cpp",
    "opt/opt.cpp",
    "res/res.cpp",
    "slice/slice.cpp",
    "status/status.cpp",
    "stdmem/stdmem.cpp",
    "ranges/ranges.cpp",
    "ordering/ordering.cpp",
    "join/join.cpp",
    "zip/zip.cpp",
    "c_allocator/c_allocator.cpp",
    "arena_allocator/arena_allocator.cpp",
    "linked_blockpool_allocator/linked_blockpool_allocator.cpp",
    "slab_allocator/slab_allocator.cpp",
    "block_allocator/block_allocator.cpp",
    "arc/arc.cpp",
    "all/all.cpp",
    "any/any.cpp",
    "arraylist/arraylist.cpp",
    "algorithm/ranges_equal.cpp",
    "algorithm/ranges_copy.cpp",
    "bit_array/bit_array.cpp",
    "bit_arraylist/bit_arraylist.cpp",
    "segmented_list/segmented_list.cpp",
};

const universal_tests_source_files = &[_][]const u8{
    // backtraces for tests
    "tests/backward.cpp",
};

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
        .source_dir = b.path("include/"),
        .install_dir = .header,
        .install_subdir = "",
    });

    const fmt = b.dependency("fmt", .{});
    const fmt_include_path = b.pathJoin(&.{ fmt.builder.install_path, "include" });
    try flags.append(b.fmt("-I{s}", .{fmt_include_path}));

    const flags_owned = flags.toOwnedSlice() catch @panic("OOM");

    const universal_tests_lib = b.addStaticLibrary(.{
        .target = target,
        .optimize = optimize,
        .name = "universal_tests_source_files",
    });

    universal_tests_lib.addCSourceFiles(.{
        .files = universal_tests_source_files,
        .flags = flags_owned,
    });
    // backtraces
    universal_tests_lib.linkLibCpp();
    universal_tests_lib.linkSystemLibrary("unwind");
    universal_tests_lib.linkSystemLibrary("bfd");

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
        test_exe.linkLibCpp();

        test_exe.addIncludePath(b.path("tests"));
        test_exe.addIncludePath(b.path("include"));

        // backtraces
        test_exe.linkLibrary(universal_tests_lib);

        test_exe.step.dependOn(fmt.builder.getInstallStep());
        try tests.append(test_exe);
    }

    const run_tests_step = b.step("run_tests", "Compile and run all the tests");
    const install_tests_step = b.step("install_tests", "Install all the tests but don't run them");
    for (tests.items, 0..) |test_exe, idx| {
        const test_install = b.addInstallArtifact(test_exe, .{});
        install_tests_step.dependOn(&test_install.step);

        const test_run = b.addRunArtifact(test_exe);
        if (b.args) |args| {
            test_run.addArgs(args);
        }
        run_tests_step.dependOn(&test_run.step);

        // induvidual run step for just this test
        const local_test_step = b.step(std.fs.path.stem(test_source_files[idx]), "Install and run this specific test .cpp file");
        local_test_step.dependOn(&test_run.step);
        local_test_step.dependOn(&test_install.step);
    }

    zcc.createStep(b, "cdb", try tests.toOwnedSlice());
}
