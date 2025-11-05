#include "camManager.hpp"

CamerasManager::CamerasManager(float extentRatio)
    : novelView(extentRatio) {
    camArray.reserve(2);
    for (int i = 0; i < 2; i++) {
        camArray.emplace_back(extentRatio);
    }

    activeCam = &novelView;
}

CamerasManager::~CamerasManager() {

}

void CamerasManager::updateRatios(float newRatio) {
    novelView.updateRatio(newRatio);
    for (auto cam: camArray){
        cam.updateRatio(newRatio);
    }
}

void CamerasManager::toggleNovel() {
    novelViewActive = novelViewActive ? false:true;
    std::cout << "Novel view " << novelViewActive << std::endl;
    if (novelViewActive) {
        activeCam = &novelView;
    }
    else {
        activeCam = &camArray[activeIndex];
    }
}

void CamerasManager::nextCam() {
    if (novelViewActive)
        return;

    activeIndex = (activeIndex+1)%(camArray.size());
    activeCam = &camArray[activeIndex];
}