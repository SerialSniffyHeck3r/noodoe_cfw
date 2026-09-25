#ifndef POWER_SCENE_H
#define POWER_SCENE_H
#include "Scene_Transition.h"
/* Independent ring/shell tracks share the global reversible cubic. */
typedef struct {SceneTransition ring,shell;} PowerScene;
void PowerScene_Seed(PowerScene *s,const ScenePose *visible);
void PowerScene_HoldRing(PowerScene *s);
void PowerScene_Request(PowerScene *s,uint32_t ring_hidden,uint32_t shell_hidden,uint32_t now);
void PowerScene_Step(PowerScene *s,uint32_t now,ScenePose *out);
#endif
