#include <windows.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <cmath>
#include <vector>
#include <time.h>
#include <thread>
#include <mutex>

#include "swarm.h"

Ant swarm[SWARM_COUNT];

const float VIEW_ANGLES = (VIEW_ANGLE/2) * M_PI / 180.0f;

// Thread function to run think() on a portion of our swarm
void antThinkThread(int start, int end) {
    for (int i = start; i < end; ++i) {
        swarm[i].think();
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_CLOSE || uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

GLuint previousFrameTexture;
//uint8_t pixels[CANVAS_WIDTH][CANVAS_HEIGHT][4];


void InitOpenGL(HDC hdc) {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        24, 8, 0,
        PFD_MAIN_PLANE,
        0, 0, 0, 0
    };

    int format = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, format, &pfd);
    HGLRC hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);

    glewInit();

    // Set up orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, CANVAS_WIDTH, 0.0, CANVAS_HEIGHT, -1.0, 1.0);  // Left, Right, Bottom, Top, Near, Far
    glMatrixMode(GL_MODELVIEW);

    glClearColor(0, 0, 0, 1.0f);  // Background color
    glPointSize(2.0f);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(0.1f);

    // Initialization
    glGenTextures(1, &previousFrameTexture);
    glBindTexture(GL_TEXTURE_2D, previousFrameTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, CANVAS_WIDTH, CANVAS_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void renderSwarm() {
    // Set up vertex pointer to directly read positions from our swarm
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(Ant), &swarm[0].positionX);  // 2D points, stride matches Ant size

    // Render our swarm in a single draw call
    glDrawArrays(GL_POINTS, 0, SWARM_COUNT);

    // Disable vertex arrays again
    glDisableClientState(GL_VERTEX_ARRAY);
}

void RenderLoop(HDC hdc) {
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT);

    // Render the previous frames texture as a background
    glBindTexture(GL_TEXTURE_2D, previousFrameTexture);
    glEnable(GL_TEXTURE_2D);

    // Draw our previous frame, to provide a "motion blur" effect, and tint it red
    glBegin(GL_QUADS);
    glColor4f(1.0f, 0.5f, 0.5f, 0.9f);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0, 0);
    glTexCoord2f(0.98f, 0.0f); glVertex2f(CANVAS_WIDTH, 0);
    glTexCoord2f(0.98f, 0.935f); glVertex2f(CANVAS_WIDTH, CANVAS_HEIGHT);
    glTexCoord2f(0.0f, 0.935f); glVertex2f(0, CANVAS_HEIGHT);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    // Render out the swarm for this frame
    glColor4f(0.2f, 0.2f, 1.0f, 0.2f);
    renderSwarm();

    // Copy the current frame into the texture for the next frame
    glBindTexture(GL_TEXTURE_2D, previousFrameTexture);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, 0);

    // Read back our image pixels for ant navigation optimisations -- currently disabled
    //glReadPixels(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // And display our image
    SwapBuffers(hdc);

    // Break our swarm updates up into chunks
    int chunkSize = SWARM_COUNT / THREAD_COUNT;
    std::vector<std::thread> threads;

    for (int i = 0; i < THREAD_COUNT; ++i) {
        int start = i * chunkSize;
        int end = (i == THREAD_COUNT - 1) ? SWARM_COUNT : start + chunkSize; // Handle last chunk
        threads.emplace_back(antThinkThread, start, end);
    }

    // Simple delay, ~60fps
    Sleep(FRAME_DELTA);

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }
}

void RunMessageLoop(HDC hdc) {
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            RenderLoop(hdc);  // Call the render function
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OpenGLWindowClass";

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, wc.lpszClassName, L"OpenGL Window", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                               NULL, NULL, hInstance, NULL);

    HDC hdc = GetDC(hwnd);
    InitOpenGL(hdc);
    ShowWindow(hwnd, SW_SHOW);

    // Init ants
    srand((unsigned int)time(NULL));
    for (int i = 0; i < SWARM_COUNT; i++) {
        swarm[i].facing = (float)(rand() % 360);
        swarm[i].positionX = (float)(rand() % CANVAS_WIDTH);
        swarm[i].positionY = (float)(rand() % CANVAS_HEIGHT);
    }

    RunMessageLoop(hdc);

    wglMakeCurrent(NULL, NULL);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);

    return 0;
}

//void Ant::nudgeTowardsNearby() {
//    // Calculate positions to sample in front of the ant
//    float radianFacing = facing * M_PI / 180.0f;
//
//    // Positions for checking (front, left, right)
//    float checkDistance = VIEW_DISTANCE;
//
//    // Directly ahead
//    int frontX = static_cast<int>(positionX + checkDistance * cos(radianFacing));
//    int frontY = static_cast<int>(positionY + checkDistance * sin(radianFacing));
//
//    // 45 degrees to the left
//    float radianLeft = radianFacing + M_PI / 8;
//    int leftX = static_cast<int>(positionX + checkDistance * cos(radianLeft));
//    int leftY = static_cast<int>(positionY + checkDistance * sin(radianLeft));
//
//    // 45 degrees to the right
//    float radianRight = radianFacing - M_PI / 8;
//    int rightX = static_cast<int>(positionX + checkDistance * cos(radianRight));
//    int rightY = static_cast<int>(positionY + checkDistance * sin(radianRight));
//
//    // Helper function to get pixel brightness
//    auto getPixelBrightness = [](int centerX, int centerY) -> int {
//        int brightnessSum = 0;
//        int count = 0;
//        int blockHalfSize = 1;
//
//        // Loop through a 3x3 block centered at (centerX, centerY)
//        for (int dx = -blockHalfSize; dx <= blockHalfSize; ++dx) {
//            for (int dy = -blockHalfSize; dy <= blockHalfSize; ++dy) {
//                int x = centerX + dx;
//                int y = centerY + dy;
//
//                // Check bounds
//                if (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT) {
//                    brightnessSum += pixels[x][y][2];  // Sum up blue channel
//                    ++count;
//                }
//            }
//        }
//
//        return count > 0 ? brightnessSum / count : (9*255);  // Return average brightness or max if no pixels
//        };
//
//
//    // Get brightness values from pixel data
//    int frontBrightness = getPixelBrightness(frontX, frontY);
//    int leftBrightness = getPixelBrightness(leftX, leftY);
//    int rightBrightness = getPixelBrightness(rightX, rightY);
//
//    if (leftBrightness < frontBrightness) {
//        facing -= rand() % JOIN_FRIEND_MOVEMENT_VARIATION;
//    }
//    else if (leftBrightness > frontBrightness) {
//        facing += rand() % JOIN_FRIEND_MOVEMENT_VARIATION;
//    }
//    else {
//        return;
//    }
//
//    // Wrap facing within [0, 360]
//    if (facing < 0) facing += 360.0f;
//    if (facing >= 360.0f) facing -= 360.0f;
//}

void Ant::nudgeTowards(Ant* other) {
    // Calculate angle to the other ant
    float deltaX = other->positionX - positionX;
    float deltaY = other->positionY - positionY;
    float angleToOther = std::atan2(deltaY, deltaX) * 180.0f / M_PI;  // Convert to degrees

    // Calculate the difference between current facing and target angle
    float angleDifference = angleToOther - facing;

    // Normalize the angle difference to [-180, 180] range
    if (angleDifference > 180.0f) angleDifference -= 360.0f;
    if (angleDifference < -180.0f) angleDifference += 360.0f;

    // Nudge facing towards the target angle by a small amount
    if (angleDifference > 0) {
        facing += min(angleDifference, JOIN_FRIEND_MOVEMENT_VARIATION);
    }
    else {
        facing += max(angleDifference, -JOIN_FRIEND_MOVEMENT_VARIATION);
    }

    // Ensure facing is in the 0-360 range
    if (facing < 0) facing += 360.0f;
    if (facing >= 360.0f) facing -= 360.0f;
}

void Ant::nudgeTowardsNearby() {
    if (lastSeen != -1) {
        nudgeTowards(&swarm[lastSeen]);

        lastSeen = -1;
    }

    for (int i = 0; i < SWARM_COUNT; i++) {
        if (&swarm[i] != this) {
            if (canSee(&swarm[i])) {
                nudgeTowards(&swarm[i]);

                // Lets follow this ant next frame as well
                lastSeen = i;

                return;
            }
        }
    }
}

void Ant::think() {
    // Move 10 units per second based on deltaTime
    float deltaTime = FRAME_DELTA;
    float distanceToMove = SWARM_SPEED * deltaTime;

    // Can we see another ant? If so, nudge facing towards it slightly
    this->nudgeTowardsNearby();

    if ((rand() % NATURAL_MOVEMENT_VARIATION_CHANCE) <= 2) {
        int initialVariationMax = (int)NATURAL_MOVEMENT_VARIATION_MAX * FRAME_DELTA;
        float initialRandomAdjustment = (float)(std::rand() % ((initialVariationMax * 2) + 1) - initialVariationMax);
        facing += initialRandomAdjustment;
    }

    if ((rand() % BIG_MOVEMENT_VARIATION_CHANCE) <= 2) {
        int initialVariationMax = (int)BIG_MOVEMENT_VARIATION_MAX * FRAME_DELTA;
        float initialRandomAdjustment = (float)(std::rand() % ((initialVariationMax * 2) + 1) - initialVariationMax);
        facing += initialRandomAdjustment;
    }

    float facingRadians = facing * M_PI / 180.0f;

    // Calculate potential new position
    float newX = positionX + std::cos(facingRadians) * distanceToMove;
    float newY = positionY + std::sin(facingRadians) * distanceToMove;

    // If it hits the left or right boundary, reflect along the y-axis
    if (newX < 0 || newX > CANVAS_WIDTH) {
        facing = 180.0f - facing;  // Reflect angle horizontally
    }

    // If it hits the top or bottom boundary, reflect along the x-axis
    if (newY < 0 || newY > CANVAS_HEIGHT) {
        facing = -facing;  // Reflect angle vertically
    }

    // Ensure facing angle stays within 0-360 degrees
    if (facing < 0) facing += 360.0f;
    if (facing >= 360.0f) facing -= 360.0f;

    // Update position only if no boundary was hit; otherwise, recalculate with new facing
    positionX += std::cos(facing * M_PI / 180.0f) * distanceToMove;
    positionY += std::sin(facing * M_PI / 180.0f) * distanceToMove;
}

bool Ant::canSee(Ant *other) {
    // Check if other ant is within visibility distance
    float deltaX = other->positionX - positionX;
    float deltaY = other->positionY - positionY;
    float distanceSquared = (deltaX * deltaX) + (deltaY * deltaY);

    if (distanceSquared > VIEW_DISTANCE_SQRD || distanceSquared < 10) {
        return false;  // Too far to see, or too close
    }

    // Calculate angle to the other ant
    float angleToOther = atan2(deltaY, deltaX) * (180.0f / M_PI);
    float facingAngle = facing;

    // Normalize angles to [0, 360]
    angleToOther = fmod((angleToOther + 360.0f), 360.0f);
    facingAngle = fmod((facingAngle + 360.0f), 360.0f);

    // Calculate angle difference and check if it's within the field of view
    float angleDifference = fmod((angleToOther - facingAngle + 540.0f), 360.0f) - 180.0f; // Normalize to [-180, 180]

    if (fabs(angleDifference) <= (VIEW_ANGLE)) {
        return true;  // Can see the other ant
    }

    return false;  // Not within the field of view
}