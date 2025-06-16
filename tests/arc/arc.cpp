#include "test_header.h"
// test header must be first
#include "okay/allocators/c_allocator.h"
#include "okay/containers/array.h"
#include "okay/smart_pointers/arc.h"

using namespace ok;

TEST_SUITE("arc smart pointers")
{
    c_allocator_t c_allocator;
    TEST_CASE("unique arc all member functions")
    {

        static_assert(!std::is_default_constructible_v<unique_rw_arc_t<int>>);
        static_assert(!std::is_copy_constructible_v<unique_rw_arc_t<int>>);
        SUBCASE("factory function")
        {
            auto int_arc = ok::into_arc(1, c_allocator);
            REQUIRE(int_arc.okay());
        }

        SUBCASE("directly calling make::with")
        {
            status<alloc::error> out_status;
            auto int_arc =
                unique_rw_arc_t<int>::make::with(out_status, c_allocator);
            REQUIRE(out_status.okay());
        }

        SUBCASE("move constructor")
        {
            status<alloc::error> out_status;
            auto int_arc =
                unique_rw_arc_t<int>::make::with(out_status, c_allocator);
            REQUIRE(out_status.okay());
            auto int_arc_2 = std::move(int_arc);
            REQUIREABORTS(auto int_arc_3 = std::move(int_arc));
        }

        SUBCASE("move assignment")
        {
            auto int_arc = ok::into_arc(1, c_allocator).release();
            auto int_arc_2 = ok::into_arc(2, c_allocator).release();
            int_arc_2 = std::move(int_arc);
            REQUIREABORTS(int_arc_2 = std::move(int_arc));
        }

        SUBCASE("conversion to generic allocator")
        {
            unique_rw_arc_t<int> i = ok::into_arc(1, c_allocator).release();

            REQUIRE(i.deref() == 1);
        }

        SUBCASE("const deref")
        {
            auto int_arc = ok::into_arc(1, c_allocator).release();
            const auto int_arc_2 = ok::into_arc(2, c_allocator).release();
            const auto int_arc_3 = std::move(int_arc);
            REQUIREABORTS(auto& _ = int_arc.deref());

            REQUIRE(int_arc_2.deref() == 2);
            REQUIRE(int_arc_3.deref() == 1);
            static_assert(
                is_const_c<
                    std::remove_reference_t<decltype(int_arc_2.deref())>>);
        }

        SUBCASE("nonconst deref")
        {
            auto int_arc = ok::into_arc(1, c_allocator).release();
            auto int_arc_2 = ok::into_arc(2, c_allocator).release();
            auto int_arc_3 = std::move(int_arc);
            REQUIREABORTS(auto& _ = int_arc.deref());

            REQUIRE(int_arc_2.deref() == 2);
            REQUIRE(int_arc_3.deref() == 1);
            static_assert(
                !is_const_c<
                    std::remove_reference_t<decltype(int_arc_2.deref())>>);
        }

        SUBCASE("demote to readonly")
        {
            auto int_arc = ok::into_arc(1, c_allocator).release();
            auto int_arc_2 = ok::into_arc(2, c_allocator).release();

            auto const_arc = int_arc.demote_to_readonly();
            REQUIREABORTS(auto&& _ = int_arc.demote_to_readonly());
            auto const_arc_2 = int_arc_2.demote_to_readonly();
        }
    }

    TEST_CASE("readonly arc all member functions")
    {
        SUBCASE("move construction and assignment")
        {
            auto int_arc =
                ok::into_arc(1, c_allocator).release().demote_to_readonly();
            auto int_arc_2 =
                ok::into_arc(2, c_allocator).release().demote_to_readonly();

            auto int_arc_3 = std::move(int_arc);
            int_arc_3 = std::move(int_arc_2);
            REQUIREABORTS(auto&& _ = int_arc.deref());
            REQUIREABORTS(auto&& _ = int_arc_2.deref());
        }

        SUBCASE("conversion to generic allocator")
        {
            ro_arc_t<int> readonly_arc =
                ok::into_arc(1, c_allocator).release().demote_to_readonly();

            REQUIRE(readonly_arc.deref() == 1);
        }

        SUBCASE("duplicate and deref")
        {
            auto int_arc =
                ok::into_arc(1, c_allocator).release().demote_to_readonly();
            auto int_arc_2 =
                ok::into_arc(2, c_allocator).release().demote_to_readonly();

            {
                auto int_arc_3 = int_arc.duplicate();
                auto int_arc_4 = int_arc_2.duplicate();

                REQUIRE(int_arc.deref() == int_arc_3.deref());
                REQUIRE(int_arc_3.deref() == 1);
                REQUIRE(int_arc_4.deref() == int_arc_2.deref());
                // destructors get called
            }

            // make sure memory still gets cleaned up when duplicates are
            // present
            auto int_arc_3 = int_arc.duplicate();
            auto int_arc_4 = int_arc_2.duplicate();
        }

        SUBCASE("try_promote_and_consume_into_unique")
        {
            auto int_arc =
                ok::into_arc(1, c_allocator).release().demote_to_readonly();
            auto int_arc_2 =
                ok::into_arc(2, c_allocator).release().demote_to_readonly();

            auto duplicate_2 = int_arc_2.duplicate();
            auto promoted =
                int_arc.try_promote_and_consume_into_unique().ref_or_panic();
            REQUIRE(
                !duplicate_2.try_promote_and_consume_into_unique().has_value());
        }

        SUBCASE("demote to weak")
        {
            auto int_arc = ok::into_arc(1, c_allocator)
                               .release()
                               .demote_to_readonly()
                               .demote_to_weak();
            auto int_arc_2 = ok::into_arc(2, c_allocator)
                                 .release()
                                 .demote_to_readonly()
                                 .demote_to_weak();
        }

        SUBCASE("spawn weak arc")
        {
            auto original_arc =
                ok::into_arc(1, c_allocator).release().demote_to_readonly();
            auto weak_arc = original_arc.spawn_weak_arc();
            {
                auto int_arc =
                    ok::into_arc(1, c_allocator).release().demote_to_readonly();

                weak_arc = int_arc.spawn_weak_arc();

                auto strong = weak_arc.try_spawn_readonly().ref_or_panic();
            }

            // the thing the weak arc was pointing to has gone out of scope
            REQUIRE(!weak_arc.try_spawn_readonly());
        }
    }

    TEST_CASE("weak arc all member functions")
    {
        SUBCASE("move constructor and assignment")
        {
            auto int_arc =
                ok::into_arc(1, c_allocator).release().demote_to_readonly();
            auto int_arc_2 =
                ok::into_arc(2, c_allocator).release().demote_to_readonly();

            auto int_weak = int_arc.duplicate().demote_to_weak();
            auto int_weak_2 = int_arc_2.duplicate().demote_to_weak();

            auto int_weak_3 = std::move(int_weak);
            auto int_weak_4 = std::move(int_weak_2);

            int_weak_3 = std::move(int_weak_4);
        }

        SUBCASE("conversion to generic allocator")
        {
            ro_arc_t<int> strong_ref =
                ok::into_arc(1, c_allocator).release().demote_to_readonly();
            weak_arc_t<int> weak_ref = strong_ref.spawn_weak_arc();

            REQUIRE(weak_ref.try_spawn_readonly().ref_or_panic().deref() == 1);
        }

        SUBCASE("duplicate")
        {
            auto int_arc =
                ok::into_arc(1, c_allocator).release().demote_to_readonly();
            auto int_arc_2 =
                ok::into_arc(2, c_allocator).release().demote_to_readonly();
            ok::array_t arcs = {
                int_arc.duplicate().demote_to_weak(),
                int_arc_2.duplicate().demote_to_weak(),
            };
            ok::array_t arcs2 = {
                arcs[0].duplicate(),
                arcs[1].duplicate(),
            };
        }

        SUBCASE("spawn readonly arc")
        {
            auto shared_arc =
                ok::into_arc(1, c_allocator).release().demote_to_readonly();
            auto unique_arc = ok::into_arc(2, c_allocator).release();

            auto weak_to_shared = shared_arc.spawn_weak_arc();
            auto weak_to_unique = unique_arc.spawn_weak_arc();

            REQUIRE(weak_to_shared.try_spawn_readonly());
            REQUIRE(!weak_to_unique.try_spawn_readonly());
        }
    }

    TEST_CASE("variant arc")
    {
        SUBCASE("move construction and assignment, converting constructors")
        {
            variant_arc_t arc = into_arc(1, c_allocator).release();
            arc =
                arc.try_convert_and_consume_into_readonly_arc().ref_or_panic();
            variant_arc_t arc2 =
                into_arc(2, c_allocator).release().demote_to_readonly();
            // spawn weak reference and then immediately destroy object.
            // just testing constructor weak -> variant
            variant_arc_t weak =
                into_arc(3, c_allocator).release().spawn_weak_arc();
            REQUIRE(!weak.try_deref());

            auto arc3 = std::move(arc2);
            auto arc4 = arc.try_duplicate().ref_or_panic();
            arc4 = std::move(arc3);
        }

        SUBCASE("conversion to generic allocator")
        {
            variant_arc_t<int, c_allocator_t> arc =
                into_arc(1, c_allocator).release();
            variant_arc_t arc2 =
                into_arc(2, c_allocator).release().demote_to_readonly();
            static_assert(std::is_same_v<decltype(arc2),
                                         variant_arc_t<int, c_allocator_t>>);
            variant_arc_t weak =
                into_arc(3, c_allocator).release().spawn_weak_arc();
            static_assert(std::is_same_v<decltype(weak),
                                         variant_arc_t<int, c_allocator_t>>);

            variant_arc_t<int> arc_generic = std::move(arc);
            variant_arc_t<int> arc2_generic = std::move(arc2);
            variant_arc_t<int> weak_generic = std::move(weak);

            static_assert(std::is_same_v<decltype(arc_generic),
                                         variant_arc_t<int, ok::allocator_t>>);

            REQUIRE(arc_generic.ownership_mode() == arc_ownership::unique_rw);
            REQUIRE(arc2_generic.ownership_mode() == arc_ownership::shared_ro);
            REQUIRE(weak_generic.ownership_mode() == arc_ownership::weak);

            REQUIRE(arc_generic.try_deref());
            REQUIRE(arc_generic.try_convert_and_consume_into_readonly_arc());
            REQUIRE(arc2_generic.try_deref());
            REQUIRE(arc2_generic.try_convert_and_consume_into_unique_arc());
            REQUIRE(!weak_generic.try_deref());
        }

        SUBCASE("ownership_mode and converting constructors set right mode")
        {
            variant_arc_t arc = into_arc(1, c_allocator).release();
            REQUIRE(arc.ownership_mode() == arc_ownership::unique_rw);
            arc = arc.try_consume_into_contained_unique_arc()
                      .ref_or_panic()
                      .demote_to_readonly();
            REQUIRE(arc.ownership_mode() == arc_ownership::shared_ro);
            variant_arc_t arc2 =
                into_arc(2, c_allocator).release().demote_to_readonly();
            REQUIRE(arc2.ownership_mode() == arc_ownership::shared_ro);
            variant_arc_t weak =
                into_arc(3, c_allocator).release().spawn_weak_arc();
            REQUIRE(weak.ownership_mode() == arc_ownership::weak);
        }

        SUBCASE("spawn weak arc")
        {
            variant_arc_t unique = into_arc(1, c_allocator).release();
            variant_arc_t shared =
                into_arc(2, c_allocator).release().demote_to_readonly();
            variant_arc_t weak =
                into_arc(3, c_allocator).release().spawn_weak_arc();

            auto weakref_to_unique = unique.spawn_weak_arc();
            auto weakref_to_shared = shared.spawn_weak_arc();
            auto weakref_to_destroyed = weak.spawn_weak_arc();
            REQUIRE(!weakref_to_unique.try_spawn_readonly());
            REQUIRE(weakref_to_shared.try_spawn_readonly());
            REQUIRE(!weakref_to_destroyed.try_spawn_readonly());
        }

        SUBCASE("try duplicate")
        {
            variant_arc_t unique = into_arc(1, c_allocator).release();
            variant_arc_t shared =
                into_arc(2, c_allocator).release().demote_to_readonly();
            variant_arc_t weak =
                into_arc(3, c_allocator).release().spawn_weak_arc();

            REQUIRE(!unique.try_duplicate());
            REQUIRE(shared.try_duplicate());
            REQUIRE(weak.try_duplicate());
            auto weak2 = weak.try_duplicate()
                             .ref_or_panic()
                             .try_consume_into_contained_weak_arc()
                             .ref_or_panic();
            REQUIRE(!weak2.try_spawn_readonly());
            auto dup = shared.try_duplicate();
        }

        SUBCASE("try deref")
        {
            variant_arc_t unique = into_arc(1, c_allocator).release();
            variant_arc_t shared =
                into_arc(2, c_allocator).release().demote_to_readonly();
            variant_arc_t weak =
                into_arc(3, c_allocator).release().spawn_weak_arc();
            variant_arc_t weak2 = shared.spawn_weak_arc();

            REQUIRE(unique.try_deref());
            REQUIRE(shared.try_deref());
            REQUIRE(!weak.try_deref());
            REQUIRE(!weak2.try_deref());

            REQUIRE(unique.try_deref_nonconst());
            REQUIRE(!shared.try_deref_nonconst());
            REQUIRE(!weak.try_deref_nonconst());
            REQUIRE(!weak2.try_deref_nonconst());
        }

        SUBCASE("try consume into contained")
        {
            variant_arc_t unique = into_arc(1, c_allocator).release();
            variant_arc_t shared =
                into_arc(2, c_allocator).release().demote_to_readonly();
            variant_arc_t weak =
                into_arc(3, c_allocator).release().spawn_weak_arc();
            variant_arc_t weak2 = shared.spawn_weak_arc();

            REQUIRE(!unique.try_consume_into_contained_weak_arc());
            REQUIRE(!unique.try_consume_into_contained_readonly_arc());
            REQUIRE(!shared.try_consume_into_contained_unique_arc());
            REQUIRE(!shared.try_consume_into_contained_weak_arc());
            REQUIRE(!weak.try_consume_into_contained_readonly_arc());
            REQUIRE(!weak.try_consume_into_contained_unique_arc());
            REQUIRE(!weak2.try_consume_into_contained_readonly_arc());
            REQUIRE(!weak2.try_consume_into_contained_unique_arc());

            unique_rw_arc_t unique_actual =
                unique.try_consume_into_contained_unique_arc().ref_or_panic();
            ro_arc_t shared_actual =
                shared.try_consume_into_contained_readonly_arc().ref_or_panic();
            weak_arc_t weak_actual =
                weak.try_consume_into_contained_weak_arc().ref_or_panic();
            weak_arc_t weak2_actual =
                weak2.try_consume_into_contained_weak_arc().ref_or_panic();
        }

        SUBCASE("try convert and consume")
        {
            variant_arc_t unique = into_arc(1, c_allocator).release();
            variant_arc_t shared =
                into_arc(2, c_allocator).release().demote_to_readonly();
            variant_arc_t weak =
                into_arc(3, c_allocator).release().spawn_weak_arc();
            variant_arc_t weak2 = shared.spawn_weak_arc();

            {
                REQUIRE(unique.ownership_mode() == arc_ownership::unique_rw);
                unique = unique.try_convert_and_consume_into_readonly_arc()
                             .ref_or_panic();
                REQUIRE(unique.ownership_mode() == arc_ownership::shared_ro);
                unique = unique.try_convert_and_consume_into_unique_arc()
                             .ref_or_panic();
                REQUIRE(unique.ownership_mode() == arc_ownership::unique_rw);
                // no error to convert a unique arc into a unique arc
                unique = unique.try_convert_and_consume_into_unique_arc()
                             .ref_or_panic();
                REQUIRE(unique.ownership_mode() == arc_ownership::unique_rw);
            }

            {
                REQUIRE(shared.ownership_mode() == arc_ownership::shared_ro);
                shared = shared.try_convert_and_consume_into_unique_arc()
                             .ref_or_panic();
                REQUIRE(shared.ownership_mode() == arc_ownership::unique_rw);
                shared = shared.try_convert_and_consume_into_readonly_arc()
                             .ref_or_panic();
                REQUIRE(shared.ownership_mode() == arc_ownership::shared_ro);
                // no error trying to convert a readonly arc to readonly
                shared = shared.try_convert_and_consume_into_readonly_arc()
                             .ref_or_panic();
            }

            // cannot create a unique arc from weak because the original payload
            // is gone
            REQUIRE(!weak.try_convert_and_consume_into_unique_arc());
            // cannot make unique arc from weak2, a shared ref is live
            REQUIRE(!weak2.try_convert_and_consume_into_unique_arc());
            // cannot upgrade weak, original payload gone
            REQUIRE(!weak.try_convert_and_consume_into_readonly_arc());
            // can upgrade weak to readonly
            REQUIRE(weak2.try_convert_and_consume_into_readonly_arc());

            weak = unique.spawn_weak_arc();
            // cannot create two unique arcs
            REQUIRE(!weak.try_convert_and_consume_into_unique_arc());
        }
    }
}
