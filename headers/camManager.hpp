#pragma once

// user includes
#include <camera.hpp>
#include <textures.hpp>

// system includes
#include <vector>

extern const size_t MAX_FRAMES_IN_FLIGHT;

enum class SaveImageFormat;
struct CamSetupJson;

/**
 * @class OfflineResources
 * @brief Manages offline rendering resources for camera arrays.
 * 
 * Handles layered images, depth mipmaps, and samplers for offline snapshot rendering.
 */
class OfflineResources {
public:
    Sampler imageSampler;
    Sampler depthSampler;  // needs different setup

    VkImage layeredImage = VK_NULL_HANDLE;
    VkImage depthLayeredImage = VK_NULL_HANDLE;

    VkImage depthMipMap0 = VK_NULL_HANDLE;
    VkImage depthMipMap1 = VK_NULL_HANDLE;
    VkImage depthMipMap2 = VK_NULL_HANDLE;
    VkImage depthMipMap3 = VK_NULL_HANDLE;

    bool offlineImagesRendered = false;
    bool imagesInvalid = false;

    /**
     * @brief Initialize offline resources.
     * @param device Vulkan device.
     * @param prop Physical device properties.
     * @param memMan Memory manager.
     * @param colorFormat Color image format.
     * @param depthFormat Depth image format.
     * @param swapChainExtent Swapchain extent.
     */
    OfflineResources(VkDevice device, const VkPhysicalDeviceProperties& prop, MemoryManager& memMan, VkFormat colorFormat, VkFormat depthFormat, VkExtent2D swapChainExtent);
    ~OfflineResources();

    /**
     * @brief Update layered images for new extent.
     * @param swapChainExtent New swapchain extent.
     */
    void updateLayered(VkExtent2D swapChainExtent);

    /**
     * @brief Save layered images to file.
     * @param filename Output filename.
     * @param depthSaveFormat Depth save format.
     * @param colorFormat Color format.
     * @param depthFormat Depth format.
     * @param nearFar Near/far plane values.
     */
    void saveLayeredImages(std::string& filename, SaveImageFormat depthSaveFormat, VkFormat colorFormat, VkFormat depthFormat, glm::vec2 nearFar);

    void createLayeredImage(VkFormat colorFormat, VkFormat depthFormat, bool dummy = false);
    void createMipMapImages(VkFormat depthFormat, bool dummy = false, VkCommandBuffer commandBuffer = VK_NULL_HANDLE);

    /**
     * @brief Get image view for type and mipmap.
     * @param type Image view type.
     * @param mipMapID Mipmap level (-1 for base).
     * @return Image view handle.
     */
    VkImageView getImageView(ImageViewType type, int mipMapID = -1);
    void setLayerCount(u_int32_t count);
    void setRendered();

    /**
     * @brief Transfer layered images to new layout.
     * @param newLayout New image layout.
     * @param colorFormat Color format.
     * @param depthFormat Depth format.
     * @param commandBuffer Command buffer.
     */
    void transferLayered(VkImageLayout newLayout, VkFormat colorFormat, VkFormat depthFormat, VkCommandBuffer commandBuffer);

private:
    VmaAllocation layeredImageMemory;
    VkImageView layeredImageView;

    VmaAllocation depthLayeredImageMemory;
    VkImageView depthLayeredImageView;

    VmaAllocation depthMipMap0Memory;
    VmaAllocation depthMipMap1Memory;
    VmaAllocation depthMipMap2Memory;
    VmaAllocation depthMipMap3Memory;
    VkImageView depthMipMap0View;
    VkImageView depthMipMap1View;
    VkImageView depthMipMap2View;
    VkImageView depthMipMap3View;

    VkImageLayout layeredLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D layeredExtent;
    uint32_t usedLayerCount;            // equal to camCount
    uint32_t allocatedLayerCount = 0;       // the image is not recreated when deleting cameras

    VkExtent2D mipMap0Extent = {1024, 1024};
    VkExtent2D mipMap1Extent = {256, 128};
    VkExtent2D mipMap2Extent = {32, 32};
    VkExtent2D mipMap3Extent = {8, 4};

    VkDevice deviceHandle;
    MemoryManager& memManager;

    void setMipMapDataLayout(VkFormat depthFormat, VkCommandBuffer commandBuffer);

    void destroyLayeredImage();
    void destroyMipMapImages();
};

/**
 * @class NovelResources
 * @brief Manages resources for novel view rendering.
 */
class NovelResources {
public:
    std::vector<VkImage> metadataImage;

    std::vector<VkImageView> depthImageView;
    std::vector<VkImageView> metadataImageView;

    std::vector<VkFramebuffer> novelFramebuffers;

    /**
     * @brief Initialize novel resources.
     * @param device Vulkan device.
     * @param memMan Memory manager.
     * @param renderPass Render pass.
     * @param extent Image extent.
     * @param formats Attachment formats.
     * @param swapChainImageViews Swapchain image views.
     */
    NovelResources(VkDevice device, MemoryManager& memMan, VkRenderPass renderPass, VkExtent2D extent, const AttachementsFormats& formats, const std::vector<VkImageView>& swapChainImageViews);
    ~NovelResources();

    /**
     * @brief Update extent and recreate resources.
     * @param newExtent New image extent.
     */
    void updateExtent(VkExtent2D newExtent);

    /**
     * @brief Recreate framebuffers.
     * @param swapChainImageViews Swapchain views.
     */
    void recreateFrameBuffers(const std::vector<VkImageView>& swapChainImageViews);

private:
    std::vector<VmaAllocation> depthImageMemory;
    std::vector<VmaAllocation> metadataImageMemory;

    std::vector<VkImage> depthImage;

    VkExtent2D imageExtent;
    uint32_t imageCount;

    VkFormat depthFormat;
    VkFormat metadataFormat;

    VkDevice deviceHandle;
    VkRenderPass renderpassHandle;
    MemoryManager& memManager;

    void createBaseImages();
    void createFrameBuffers(const std::vector<VkImageView>& swapChainImageViews);

    void destroyFrameBuffers();
};

/**
 * @class CamerasManager
 * @brief Manages multiple cameras for rendering and novel view synthesis.
 */
class CamerasManager {
public:
    Camera* activeCam;    ///< Pointer to the currently active camera.

    std::vector<OfflineCamera> camArray;    ///< Array of offline cameras.
    NovelCamera novelView;                  ///< Novel view camera.
    Camera observer;                        ///< Observer camera.

    uint32_t sampleCount = 16;
    uint32_t sampleDebug = 16;

    /**
     * @brief Initialize camera manager.
     * @param device Vulkan device.
     * @param memMan Memory manager.
     * @param swapChainExtent Swapchain extent.
     * @param formats Attachment formats.
     * @param renderpass Render pass.
     * @param prop Physical device properties.
     * @param swapChainImageViews Swapchain views.
     */
    CamerasManager(VkDevice device, MemoryManager& memMan, VkExtent2D swapChainExtent, const AttachementsFormats& formats, VkRenderPass renderpass, const VkPhysicalDeviceProperties& prop, const std::vector<VkImageView>& swapChainImageViews);
    ~CamerasManager();

    /**
     * @brief Update for swapchain resize.
     * @param swapChainExtent New extent.
     * @param swapChainImageViews New views.
     */
    void updateResize(VkExtent2D swapChainExtent, const std::vector<VkImageView>& swapChainImageViews);
    void toggleNovel();
    void toggleObserver();
    void nextCam(bool ignoreNovelView = false);
    void setActiveCam(uint32_t newIndex);

    void addCam(MemoryManager& memManager);
    void deleteCam(MemoryManager& memManager);

    uint32_t getCamCount() const;
    uint32_t getActiveIndex() const;
    bool novelViewToggled();
    bool observerToggled();
    float getCamArrayFOV() const;
    void setCamArrayFOV(float newFovRadians);

    /**
     * @brief Get image view for type.
     * @param type Image view type.
     * @return Image view handle.
     */
    VkImageView getImageView(ImageViewType type);

    /**
     * @brief Get reduce depth views.
     * @return Array of depth views.
     */
    std::array<VkImageView, 4> getReduceDepthViews();
    std::vector<VkImage> getReduceStorageImages();
    void createLayeredImages();
    void createMipMapImages(VkCommandBuffer commandBuffer);

    bool imagesInvalid();
    bool imagesRendered();
    void setImagesRendered();
    VkSampler getSampler(ImageViewType type);

    /**
     * @brief Save images to file.
     * @param filename Output filename.
     * @param depthSaveFormat Depth format.
     */
    void saveImages(std::string& filename, SaveImageFormat depthSaveFormat);

    /**
     * @brief Transfer layered layout.
     * @param layout New layout.
     * @param commandBuffer Command buffer.
     */
    void transferLayeredLayout(VkImageLayout layout, VkCommandBuffer commandBuffer);

    VkFramebuffer getNovelFramebuffer(uint32_t imageIndex);
    std::vector<VkImageView> getNovelStorageViews();
    VkImage getNovelStorageImage(uint32_t currentFrame);

    /**
     * @brief Create JSON setup of cameras to save.
     * @return Camera setup in JSON format.
     */
    CamSetupJson jsonSetup();

    /**
     * @brief Load cameras from JSON setup.
     * @param setup Camera setup in JSON format.
     * @param memManager Memory manager for resource allocation.
     */
    void loadFromJson(const CamSetupJson& setup, MemoryManager& memManager);

private:
    OfflineResources offlineRes;
    NovelResources novelRes;
    bool framebuffersInvalid = true;

    bool novelViewActive = true;
    bool observerActive = false;

    VkFormat colorFormat;
    VkFormat depthFormat;
    float extentRatio;

    uint32_t camCount;
    uint32_t activeIndex = 0;

    VkExtent2D swapChainExtentHandle;       // size of the rendered images


    // Vulkan Handles
    VkDevice deviceHandle;
    VkRenderPass renderpassHandle;
};