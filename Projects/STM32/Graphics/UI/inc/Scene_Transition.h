#ifndef SCENE_TRANSITION_H
#define SCENE_TRANSITION_H
#include <stdint.h>
#define SCENE_TRANSITION_MS 400U
typedef struct {uint32_t from,target,value,start_ms,active;} SceneTransition;
typedef struct {uint32_t progress,ring_radius,ring_alpha,settings_alpha;int32_t clock_y,footer_y;} ScenePose;
/* Pure reversible scene progress0..1024. Retarget from current pose, without
 * reallocating objects or restoring stale telemetry. Same cubic as page UI. */
void SceneTransition_Request(SceneTransition *s,uint32_t settings,uint32_t now);
void SceneTransition_Step(SceneTransition *s,uint32_t now,ScenePose *out);
#endif
