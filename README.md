![flecs](https://user-images.githubusercontent.com/9919222/104115165-0a4e4700-52c1-11eb-85d6-9bdfa9a0265f.png)

[![CI build](https://github.com/SanderMertens/flecs/workflows/CI/badge.svg)](https://github.com/SanderMertens/flecs/actions)
[![codecov](https://codecov.io/gh/SanderMertens/flecs/branch/master/graph/badge.svg)](https://codecov.io/gh/SanderMertens/flecs)
[![Discord Chat](https://img.shields.io/discord/633826290415435777.svg)](https://discord.gg/BEzP5Rgrrp)
[![Try online](https://img.shields.io/badge/try-online-brightgreen)](https://godbolt.org/z/bs11T3)
[![Documentation](https://img.shields.io/badge/docs-docsforge-blue)](http://flecs.docsforge.com/)

Flecs is a fast and lightweight Entity Component System with a focus on high performance game development ([join the Discord!](https://discord.gg/MRSAZqb)). The highlights of the framework are:

- C99 core, modern C++11 API, no dependencies on STL types
- Batched iteration with direct access to component arrays
- SoA/Archetype storage for efficient CPU caching & vectorization
- Automatic component registration across binaries
- Hierarchies
- Prefabs
- Entity relationships
- Fast graph queries
- Thread safe, lockless API
- Systems that are ran manually, every frame or at a time/rate interval
- Queries that can be iterated from free functions
- Modules for organizing components & systems
- Builtin statistics & introspection
- Modular core with compile-time disabling of optional features
- A dashboard module for visualizing statistics:

<img width="942" alt="Screen Shot 2020-12-02 at 1 28 04 AM" src="https://user-images.githubusercontent.com/9919222/100856510-5eebe000-3440-11eb-908e-f4844c335f37.png">

## What is an Entity Component System?
ECS (Entity Component System) is a design pattern used in games and simulations that produces fast and reusable code. Dynamic composition is a first-class citizen in ECS, and there is a strict separation between data and behavior. A framework is an Entity Component System if it:

- Has _entities_ that are unique identifiers
- Has _components_ that are plain data types
- Has _systems_ which are behavior matched with entities based on their components

## Documentation
If you are still learning Flecs, these resources are a good start:
- [Flecs not for dummies (presentation)](https://github.com/SanderMertens/flecs_not_for_dummies)
- [Quickstart](docs/Quickstart.md) ([docsforge](https://flecs.docsforge.com/master/quickstart/))
- [Designing with Flecs](docs/DesignWithFlecs.md) ([docsforge](https://flecs.docsforge.com/master/designing-with-flecs/))

The FAQ is where some of the most asked questions are listed:
- [FAQ](docs/FAQ.md) ([docsforge](https://flecs.docsforge.com/master/faq/))

The manual and examples come in handy if you're looking for information on specific features:
- [Manual](docs/Manual.md) ([docsforge](https://flecs.docsforge.com/master/manual/))
- [C examples](examples/c)
- [C++ examples](examples/cpp)

If you are migrating from Flecs v1 to v2, check the migration guide:
- [Migration guide](docs/MigrationGuide.md) ([docsforge](https://flecs.docsforge.com/master/migrationguide/))

Here is some awesome content provided by the community (thanks everyone! :heart:):
- [Bringing Flecs to UE4](https://bebylon.dev/blog/ecs-flecs-ue4/)
- [Flecs + UE4 is magic](https://jtferson.github.io/blog/flecs_and_unreal/)
- [Quickstart with Flecs in UE4](https://jtferson.github.io/blog/quickstart_with_flecs_in_unreal_part_1/) 
- [Automatic component registration in UE4](https://jtferson.github.io/blog/automatic_flecs_component_registration_in_unreal/)
- [Building a space battle with Flecs in UE4](https://twitter.com/ajmmertens/status/1361070033334456320) 
- [Flecs + SDL + Web ASM example](https://github.com/HeatXD/flecs_web_demo) ([live demo](https://heatxd.github.io/flecs_web_demo/))
- [Flecs + gunslinger example](https://github.com/MrFrenik/gs_examples/blob/main/18_flecs/source/main.c)

## Examples
This is a simple flecs example in the C99 API:

```c
typedef struct {
  float x;
  float y;
} Position, Velocity;

void Move(ecs_iter_t *it) {
  Position *p = ecs_term(it, Position, 1);
  Velocity *v = ecs_term(it, Velocity, 2);
  
  for (int i = 0; i < it->count; i ++) {
    p[i].x += v[i].x * it->delta_time;
    p[i].y += v[i].y * it->delta_time;
    printf("Entity %s moved!\n", ecs_get_name(it->world, it->entities[i]));
  }
}

int main(int argc, char *argv[]) {
  ecs_world_t *ecs = ecs_init();

  // Register the Position component
  ecs_entity_t pos = ecs_component_init(ecs, &(ecs_component_desc_t){
    .entity.name = "Position",
    .size = sizeof(Position), .alignment = ECS_ALIGNOF(Position)
  });

  // Register the Velocity component
  ecs_entity_t vel = ecs_component_init(ecs, &(ecs_component_desc_t){
    .entity.name = "Velocity",
    .size = sizeof(Velocity), .alignment = ECS_ALIGNOF(Velocity)
  });

  // Create the Move system
  ecs_system_init(ecs, &(ecs_system_desc_t){
    .entity = { .name = "Move", .add = {EcsOnUpdate} },
    .query.filter.terms = {{pos}, {vel, .inout = EcsIn}},
    .callback = Move,
  });

  // Create a named entity
  ecs_entity_t e = ecs_entity_init(ecs, &(ecs_entity_desc_t){
    .name = "MyEntity"
  });

  // Set components
  ecs_set_id(ecs, e, pos, sizeof(Position), &(Position){0, 0});
  ecs_set_id(ecs, e, vel, sizeof(Velocity), &(Velocity){1, 1});

  // Run the main loop
  while (ecs_progress(ecs, 0)) { }
}
```

Because C99 lacks generics, the API provides a number of convenience macro's 
that reduce boilerplate & improve type safety. This is the same example
with macro's (system implementation ommitted since it doesn't change):

```c
int main(int argc, char *argv[]) {
  ecs_world_t *ecs = ecs_init();

  ECS_COMPONENT(ecs, Position);
  ECS_COMPONENT(ecs, Velocity);

  ECS_SYSTEM(ecs, Move, EcsOnUpdate, Position, [in] Velocity);

  ecs_entity_t e = ecs_entity_init(ecs, &(ecs_entity_desc_t){
    .name = "MyEntity"
  });

  ecs_set(ecs, e, Position, {0, 0});
  ecs_set(ecs, e, Velocity, {1, 1});

  while (ecs_progress(ecs, 0)) { }
}
```

This is the same example in the C++11 API:

```c++
struct Position {
  float x;
  float y;
};

struct Velocity {
  float x;
  float y;
};

int main(int argc, char *argv[]) {
  flecs::world ecs;

  ecs.system<Position, const Velocity>()
    .each([](flecs::entity e, Position& p, const Velocity& v) {
      p.x += v.x * e.delta_time();
      p.y += v.y * e.delta_time();
      std::cout << "Entity " << e.name() << " moved!" << std::endl;
    });

  ecs.entity("MyEntity")
    .set<Position>({0, 0})
    .set<Velocity>({1, 1});

  while (ecs.progress()) { }
}
```

## Building
The easiest way to add Flecs to a project is to add [flecs.c](https://raw.githubusercontent.com/SanderMertens/flecs/master/flecs.c) and [flecs.h](https://raw.githubusercontent.com/SanderMertens/flecs/master/flecs.h) to your source code. These files can be added to both C and C++ projects (the C++ API is embedded in flecs.h). Alternatively you can also build Flecs as a library by using the cmake, meson, bazel or bake buildfiles.

### Custom builds
The Flecs source has a modular design which makes it easy to strip out code you don't need. At its core, Flecs is a minimalistic ECS library with a lot of optional features that you can choose to include or not. [This section of the manual](https://github.com/SanderMertens/flecs/blob/master/docs/Manual.md#custom-builds) describes how to customize which features to include. 

## Software Quality
To ensure stability of Flecs, the code is thoroughly tested on every commit:

- 40.000 lines of test code, for 18.000 lines of framework code
- More than 1600 testcases
- Over 90% code coverage

The code is validated on the following platforms/compilers:

- **Windows**
  - msvc
- **Ubuntu**
  - gcc 7, 8, 9, 10
  - clang 8, 9
- **MacOS**
  - gcc 10
  - clang 9

The framework code and example code is compiled warning free on all platforms with the strictest warning settings. A sanitized build is ran on each commit to test for memory corruption and undefined behavior.

Performance is tracked on a per-release basis, with the results for the latest release published here: https://github.com/SanderMertens/ecs_benchmark

### API stability
API (programming interface) stability is guaranteed between minor releases, except in the rare case when an API is found to be an obvious source of confusion or bugs. When breaking changes do happen, the release notes will mention it with potential workarounds. 

ABI (binary interface) stability is _not_ guaranteed inbetween versions, as non-opaque types and signatures may change at any point in time, as long as they don't break compilation of code that uses the public API. Headers under include/private are not part of the public API, and may introduce breaking changes at any point.

It is generally safe to use the master branch, which contains the latest version of the code. New features that are on master but are not yet part of a release may still see changes in their API. Once a feature is part of a release, its API will not change until at least the next major release.

## Modules
The following modules are available in [flecs-hub](https://github.com/flecs-hub). Note that modules are mostly intended as example code, and their APIs may change at any point in time.

Module      | Description      
------------|------------------
[flecs.meta](https://github.com/flecs-hub/flecs-meta) | Reflection for Flecs components
[flecs.json](https://github.com/flecs-hub/flecs-json) | JSON serializer for Flecs components
[flecs.rest](https://github.com/flecs-hub/flecs-rest) | A REST interface for introspecting & editing entities
[flecs.player](https://github.com/flecs-hub/flecs-player) | Play, stop and pause simulations
[flecs.monitor](https://github.com/flecs-hub/flecs-monitor) | Web-based monitoring of statistics
[flecs.dash](https://github.com/flecs-hub/flecs-dash) | Web-based dashboard for remote monitoring and debugging of Flecs apps
[flecs.components.input](https://github.com/flecs-hub/flecs-components-input) | Components that describe keyboard and mouse input
[flecs.components.transform](https://github.com/flecs-hub/flecs-components-transform) | Components that describe position, rotation and scale
[flecs.components.physics](https://github.com/flecs-hub/flecs-components-physics) | Components that describe physics and movement
[flecs.components.geometry](https://github.com/flecs-hub/flecs-components-geometry) | Components that describe geometry
[flecs.components.graphics](https://github.com/flecs-hub/flecs-components-graphics) | Components used for computer graphics
[flecs.components.gui](https://github.com/flecs-hub/flecs-components-gui) | Components used to describe GUI components
[flecs.components.http](https://github.com/flecs-hub/flecs-components-http) | Components describing an HTTP server
[flecs.systems.transform](https://github.com/flecs-hub/flecs-systems-transform) | Hierarchical transforms for scene graphs
[flecs.systems.sdl2](https://github.com/flecs-hub/flecs-systems-sdl2) | SDL window creation & input management
[flecs.systems.sokol](https://github.com/flecs-hub/flecs-systems-sokol) | Sokol-based renderer
[flecs.systems.civetweb](https://github.com/flecs-hub/flecs-systems-civetweb) | A civetweb-based implementation of flecs.components.http

## Language bindings
- [Lua](https://github.com/flecs-hub/flecs-lua)
- [Zig](https://github.com/prime31/zig-flecs)

## Useful Links
- [ECS FAQ](https://github.com/SanderMertens/ecs-faq)
- [Medium](https://ajmmertens.medium.com)
- [Twitter](https://twitter.com/ajmmertens)
- [Reddit](https://www.reddit.com/r/flecs)

## Supporting Flecs
Supporting Flecs goes a long way towards keeping the project going and the community alive! If you like the project, consider:
- Giving it a star
- Becoming a sponsor: https://github.com/sponsors/SanderMertens

Thanks in advance!
