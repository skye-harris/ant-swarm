#pragma once

#include "resource.h"

#define M_PI 3.14159f

#define DISPLAY_WIDTH 800
#define DISPLAY_HEIGHT 600
#define THREAD_COUNT 6

#define CANVAS_WIDTH 800
#define CANVAS_HEIGHT 600
#define FRAME_DELTA (1000/60)
#define SWARM_COUNT 20000
#define SWARM_SPEED 0.2f
#define NATURAL_MOVEMENT_VARIATION_MAX 1
#define NATURAL_MOVEMENT_VARIATION_CHANCE 10000000

#define BIG_MOVEMENT_VARIATION_MAX 20
#define BIG_MOVEMENT_VARIATION_CHANCE 10000000

#define JOIN_FRIEND_MOVEMENT_VARIATION 0.5f
#define VIEW_ANGLE 45
#define VIEW_DISTANCE 50
#define VIEW_DISTANCE_SQRD (VIEW_DISTANCE*VIEW_DISTANCE)

class Ant {
public:
	void think();
	void nudgeTowardsNearby();
	void nudgeTowards(Ant* other);
	bool canSee(Ant *other);

	float positionX = 0.0f;
	float positionY = 0.0f;
	float facing = 0.0f;
	bool isSeen = false;
	int lastSeen = -1;
};