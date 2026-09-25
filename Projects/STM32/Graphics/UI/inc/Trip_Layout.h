#ifndef PRODUCT_TRIP_LAYOUT_H
#define PRODUCT_TRIP_LAYOUT_H
/* Coordinates relative to the336px body (screen X72). The entire reserved
 * distance row is centered at X240: pictogram,16px padding, number, unit.
 * Values never resize these slots or move the pictogram. */
#define TRIP_DISTANCE_X 132
#define TRIP_DISTANCE_WIDTH 130
#define TRIP_DISTANCE_UNIT_X 270
#define TRIP_DISTANCE_UNIT_WIDTH 32
#define TRIP_DISTANCE_SIGN_X 34
#define TRIP_DISTANCE_BIKE_X 90
#define TRIP_DISTANCE_DOTS_X 63
#define TRIP_DISTANCE_DOT_STEP 8

/* D-DIN40 has20px digit advances and an8px colon: HH:MM=88px. Center the
 * five cells inside the existing132px time slot; real hours above99 still
 * fit that slot without truncating accumulated riding time. */
#define TRIP_TIME_X 102
#define TRIP_TIME_WIDTH 132
#define TRIP_TIME_PAD_X 124
#define TRIP_TIME_DIGIT_WIDTH 20
#define TRIP_TIME_SUFFIX_X 144
#define TRIP_TIME_SUFFIX_WIDTH 68
#endif
