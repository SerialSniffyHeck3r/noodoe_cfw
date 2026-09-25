#ifndef PAGE_TRANSITION_H
#define PAGE_TRANSITION_H
#include <stdint.h>
#define PAGE_TRANSITION_MS 240U
#define PAGE_TRANSITION_DISTANCE 28
/* Reusable pure timing model. Two permanent renderer slots hold outgoing and
 * incoming sections. Rapid requests coalesce to the latest target; no queue,
 * allocation or partially transparent section is suddenly repurposed. */
typedef struct {uint32_t current,target,pending,start_ms,active,initialized;} PageTransition;
typedef struct {int32_t outgoing_x,incoming_x;uint32_t outgoing_alpha,incoming_alpha;} PageTransitionFrame;
typedef enum {PAGE_AXIS_HORIZONTAL,PAGE_AXIS_VERTICAL} PageTransitionAxis;
typedef struct {int32_t outgoing_x,outgoing_y,incoming_x,incoming_y;} PageTransitionPose;
/* Shared slow-fast-slow curve, Q10 [0,1024], clamped at240ms. Both axes and
 * the mode strip use this exact polynomial; never maintain separate easing. */
uint32_t PageTransition_Ease(uint32_t elapsed_ms);
/* Axis/direction is fixed when a section starts. +1 means next (outgoing
 * left/up, incoming right/bottom); -1 means previous. Pure coordinate mapping. */
void PageTransition_Project(const PageTransitionFrame *frame,PageTransitionAxis axis,
    int32_t direction,PageTransitionPose *pose);
/* Returns1 when caller must bind a new incoming section,2 on first binding. */
uint32_t PageTransition_Request(PageTransition *state,uint32_t key,uint32_t now);
/* Returns1 on completion; caller swaps the permanent slots. */
uint32_t PageTransition_Step(PageTransition *state,uint32_t now,PageTransitionFrame *frame);
#endif
