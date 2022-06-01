#include "flecs.h"
#include "../private_api.h"

#ifdef FLECS_MONITOR

ECS_COMPONENT_DECLARE(FlecsMonitor);
ECS_COMPONENT_DECLARE(EcsWorldStats);
ECS_DECLARE(EcsPeriod1s);
ECS_DECLARE(EcsPeriod1m);
ECS_DECLARE(EcsPeriod1h);
ECS_DECLARE(EcsPeriod1d);
ECS_DECLARE(EcsPeriod1w);

static int32_t flecs_day_interval_count = 24;
static int32_t flecs_week_interval_count = 168;

static
void MonitorWorldStats(ecs_iter_t *it) {
    EcsWorldStats *stats = ecs_term(it, EcsWorldStats, 1);

    FLECS_FLOAT elapsed = stats->elapsed;
    stats->elapsed += it->delta_time;

    int32_t t_last = (int32_t)(elapsed * 60);
    int32_t t_next = (int32_t)(stats->elapsed * 60);
    int32_t i, dif = t_last - t_next;

    ecs_world_stats_t last;
    last.t = 0;
    if (!dif) {
        /* Copy last value so we can pass it to reduce_last */
        ecs_world_stats_copy_last(&last, &stats->stats);
    }

    ecs_world_stats_get(it->real_world, &stats->stats);

    if (!dif) {
        /* Still in same interval, combine with last measurement */
        stats->reduce_count ++;
        ecs_world_stats_reduce_last(&stats->stats, &last, stats->reduce_count);
    } else if (dif > 1) {
        /* More than 16ms has passed, backfill */
        for (i = 1; i < dif; i ++) {
            ecs_world_stats_repeat_last(&stats->stats);
        }
        stats->reduce_count = 0;
    }
}

static
void ReduceWorldStats(ecs_iter_t *it) {
    EcsWorldStats *dst = ecs_term(it, EcsWorldStats, 1);
    EcsWorldStats *src = ecs_term(it, EcsWorldStats, 2);

    ecs_world_stats_reduce(&dst->stats, &src->stats);
}

static
void AggregateWorldStats(ecs_iter_t *it) {
    int32_t interval = *(int32_t*)it->ctx;

    EcsWorldStats *dst = ecs_term(it, EcsWorldStats, 1);
    EcsWorldStats *src = ecs_term(it, EcsWorldStats, 2);

    ecs_world_stats_t last;
    last.t = 0;
    if (dst->reduce_count != 0) {
        /* Copy last value so we can pass it to reduce_last */
        ecs_world_stats_copy_last(&last, &dst->stats);
    }

    /* Reduce from minutes to the current day */
    ecs_world_stats_reduce(&dst->stats, &src->stats);

    if (dst->reduce_count != 0) {
        ecs_world_stats_reduce_last(&dst->stats, &last, dst->reduce_count);
    }

    /* A day has 60 24 minute intervals */
    dst->reduce_count ++;
    if (dst->reduce_count >= interval) {
        dst->reduce_count = 0;
    }
}

void FlecsMonitorImport(
    ecs_world_t *world)
{
    ECS_MODULE_DEFINE(world, FlecsMonitor);

    ecs_set_name_prefix(world, "Ecs");

    ECS_COMPONENT_DEFINE(world, EcsWorldStats);
    ECS_TAG_DEFINE(world, EcsPeriod1s);
    ECS_TAG_DEFINE(world, EcsPeriod1m);
    ECS_TAG_DEFINE(world, EcsPeriod1h);
    ECS_TAG_DEFINE(world, EcsPeriod1d);
    ECS_TAG_DEFINE(world, EcsPeriod1w);
    
    // Called each frame, collects 60 measurements per second
    ecs_system_init(world, &(ecs_system_desc_t) {
        .entity = { .name = "MonitorWorld1s", .add = {EcsPreFrame} },
        .query.filter.terms = {{
            .id = ecs_pair(ecs_id(EcsWorldStats), EcsPeriod1s),
            .subj.entity = EcsWorld 
        }},
        .callback = MonitorWorldStats
    });

    // Called each second, reduces into 60 measurements per minute
    ecs_entity_t mw1m = ecs_system_init(world, &(ecs_system_desc_t) {
        .entity = { .name = "MonitorWorld1m", .add = {EcsPreFrame} },
        .query.filter.terms = {{
            .id = ecs_pair(ecs_id(EcsWorldStats), EcsPeriod1m),
            .subj.entity = EcsWorld 
        }, {
            .id = ecs_pair(ecs_id(EcsWorldStats), EcsPeriod1s),
            .subj.entity = EcsWorld 
        }},
        .callback = ReduceWorldStats,
        .interval = 1.0
    });

    // Called each minute, reduces into 60 measurements per hour
    ecs_system_init(world, &(ecs_system_desc_t) {
        .entity = { .name = "MonitorWorld1h", .add = {EcsPreFrame} },
        .query.filter.terms = {{
            .id = ecs_pair(ecs_id(EcsWorldStats), EcsPeriod1h),
            .subj.entity = EcsWorld 
        }, {
            .id = ecs_pair(ecs_id(EcsWorldStats), EcsPeriod1m),
            .subj.entity = EcsWorld 
        }},
        .callback = ReduceWorldStats,
        .rate = 60,
        .tick_source = mw1m
    });

    // Called each minute, reduces into 60 measurements per day
    ecs_system_init(world, &(ecs_system_desc_t) {
        .entity = { .name = "MonitorWorld1d", .add = {EcsPreFrame} },
        .query.filter.terms = {{
            .id = ecs_pair(ecs_id(EcsWorldStats), EcsPeriod1d),
            .subj.entity = EcsWorld 
        }, {
            .id = ecs_pair(ecs_id(EcsWorldStats), EcsPeriod1m),
            .subj.entity = EcsWorld 
        }},
        .callback = AggregateWorldStats,
        .rate = 60,
        .tick_source = mw1m,
        .ctx = &flecs_day_interval_count
    });

    // Called each hour, reduces into 60 measurements per week
    ecs_system_init(world, &(ecs_system_desc_t) {
        .entity = { .name = "MonitorWorld1w", .add = {EcsPreFrame} },
        .query.filter.terms = {{
            .id = ecs_pair(ecs_id(EcsWorldStats), EcsPeriod1w),
            .subj.entity = EcsWorld 
        }, {
            .id = ecs_pair(ecs_id(EcsWorldStats), EcsPeriod1h),
            .subj.entity = EcsWorld 
        }},
        .callback = AggregateWorldStats,
        .rate = 60,
        .tick_source = mw1m,
        .ctx = &flecs_week_interval_count
    });

    ecs_set_pair(world, EcsWorld, EcsWorldStats, EcsPeriod1s, {0});
    ecs_set_pair(world, EcsWorld, EcsWorldStats, EcsPeriod1m, {0});
    ecs_set_pair(world, EcsWorld, EcsWorldStats, EcsPeriod1h, {0});
    ecs_set_pair(world, EcsWorld, EcsWorldStats, EcsPeriod1d, {0});
    ecs_set_pair(world, EcsWorld, EcsWorldStats, EcsPeriod1w, {0});

    if (ecs_os_has_time()) {
        ecs_measure_frame_time(world, true);
        ecs_measure_system_time(world, true);
    }
}

#endif
