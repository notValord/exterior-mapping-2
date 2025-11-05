#pragma once

#include "camera.hpp"

#include <vector>

class CamerasManager {
public:
    Camera* activeCam;

    CamerasManager(float extentRatio);
    ~CamerasManager();

    void updateRatios(float newRatio);
    void toggleNovel();
    void nextCam();

    uint32_t activeIndex = 0;
private:
    std::vector<Camera> camArray;
    Camera novelView;

    bool novelViewActive = true;
};