#include "TestFramework.hpp"
#include "engine/world/WorldSystem.hpp"

TEST(entity_create_destroy_and_stale_handles) {
    ge::EntityWorld w;
    const ge::Entity a = w.create();
    CHECK(w.alive(a));
    CHECK_EQ(w.size(), 1u);
    w.destroy(a);
    CHECK(!w.alive(a));
    const ge::Entity b = w.create();  // reuses the slot with a new generation
    CHECK_EQ(b.index, a.index);
    CHECK(!(a == b));
    CHECK(!w.alive(a));
    CHECK(w.alive(b));
}

TEST(entity_components_add_get_remove) {
    ge::EntityWorld w;
    const ge::Entity e = w.create();
    CHECK(!w.has<ge::Health>(e));
    w.add<ge::Health>(e, ge::Health{50, 100});
    CHECK(w.has<ge::Health>(e));
    CHECK_EQ(w.get<ge::Health>(e)->current, 50);
    w.remove<ge::Health>(e);
    CHECK(w.get<ge::Health>(e) == nullptr);
}

TEST(entity_destroy_drops_components_and_keeps_others_valid) {
    ge::EntityWorld w;
    const ge::Entity a = w.create(), b = w.create(), c = w.create();
    w.add<ge::Health>(a, ge::Health{1, 1});
    w.add<ge::Health>(b, ge::Health{2, 2});
    w.add<ge::Health>(c, ge::Health{3, 3});
    w.destroy(a);  // swap-remove moves c's data into a's slot
    CHECK_EQ(w.get<ge::Health>(b)->current, 2);
    CHECK_EQ(w.get<ge::Health>(c)->current, 3);
    CHECK(w.get<ge::Health>(a) == nullptr);
}

TEST(entity_each_visits_only_matching_entities) {
    ge::EntityWorld w;
    const ge::Entity mover = w.create(), still = w.create();
    w.add<ge::Transform>(mover, ge::Transform{{0, 0}, 0});
    w.add<ge::Velocity>(mover, ge::Velocity{{1, 2}});
    w.add<ge::Transform>(still, ge::Transform{{5, 5}, 0});
    int visited = 0;
    w.each<ge::Transform, ge::Velocity>([&](ge::Entity, ge::Transform& t, ge::Velocity& v) {
        t.position = t.position + v.linear;
        ++visited;
    });
    CHECK_EQ(visited, 1);
    CHECK_NEAR(w.get<ge::Transform>(mover)->position.y, 2.0, 1e-12);
    CHECK_NEAR(w.get<ge::Transform>(still)->position.x, 5.0, 1e-12);
}
