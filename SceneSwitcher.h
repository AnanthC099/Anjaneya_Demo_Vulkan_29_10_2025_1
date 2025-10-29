#pragma once

#define MYICON 1001

// Scene sequence phase constants
enum SequencePhase
{
    SEQUENCE_PHASE_SCENE0,
    SEQUENCE_PHASE_FADE_TO_BLACK_1,
    SEQUENCE_PHASE_SCENE1,
    SEQUENCE_PHASE_FADE_TO_BLACK_2,
    SEQUENCE_PHASE_SCENE2,
    SEQUENCE_PHASE_SCENE3,
    SEQUENCE_PHASE_COMPLETE
};

#ifndef RC_INVOKED

#include <windows.h>

/* avoid C++ std headers; stick to C headers */
#include <stdio.h>
#include <stdint.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

typedef struct GlobalContext_VertexData
{
    VkBuffer vkBuffer;
    VkDeviceMemory vkDeviceMemory;
} GlobalContext_VertexData;

typedef struct GlobalContext_UniformData
{
    VkBuffer vkBuffer;
    VkDeviceMemory vkDeviceMemory;
} GlobalContext_UniformData;

typedef struct GlobalContext_MyUniformData
{
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    /* Scene 0: fade.x.
       Scene 1: fade = (fadeAmt, screenWidth, screenHeight, overlaySizeFrac).
       Scene 2: reserved (set to zero).
       Composite: fade.z carries focus-pull strength (0..1). */
    glm::vec4 fade;
} GlobalContext_MyUniformData;

/* --------------------------------------------------------------------------
   Centralized key bindings for scene switching (WM_KEYDOWN / virtual-keys)
   Change these if you prefer different keys in the future.
   -------------------------------------------------------------------------- */
#ifndef SCENESWITCHER_KEY_SCENE0
#define SCENESWITCHER_KEY_SCENE0 VK_F1
#endif
#ifndef SCENESWITCHER_KEY_SCENE1
#define SCENESWITCHER_KEY_SCENE1 VK_F2
#endif
#ifndef SCENESWITCHER_KEY_SCENE2
#define SCENESWITCHER_KEY_SCENE2 VK_F3
#endif
#ifndef SCENESWITCHER_KEY_SCENE3
#define SCENESWITCHER_KEY_SCENE3 VK_F5
#endif

#ifndef WIN_WIDTH
#define WIN_WIDTH 800
#endif

#ifndef WIN_HEIGHT
#define WIN_HEIGHT 600
#endif

typedef struct Win32WindowContext_Switcher
{
    const char* gpszAppName;

    // Global variables
    HWND ghwnd;
    HDC ghdc;
    HGLRC ghrc;

    DWORD dwStyle;
    WINDOWPLACEMENT wpPrev;

    int gbActiveWindow;
    int gbFullscreen;
    int gbWindowMinimized;

    int giHeight;
    int giWidth;
} Win32WindowContext_Switcher;

typedef struct GlobalContext_Switcher
{
    FILE* gpFile;

    // Vulkan related global variables
    // Instance Extension related variables
    uint32_t enabledInstanceExtensionCount;
    // VK_KHR_SURFACE_EXTENSION_NAME
    // VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    // VK_EXT_DEBUG_REPORT_EXTENSION_NAME
    const char* enabledInstanceExtensionNames_array[3];

    // Vulkan Instance
    VkInstance vkInstance;

    // Vulkan Presentation Surface
    VkSurfaceKHR vkSurfaceKHR;

    // Vulkan Physical Device related
    VkPhysicalDevice vkPhysicalDevice_selected;
    uint32_t graphicsQueueFamilyIndex_selected;
    VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;

    // Vulkan Printing VkInfo related
    VkPhysicalDevice* vkPhysicalDevice_array;
    uint32_t physicalDeviceCount;

    // Device Extension related variables
    uint32_t enabledDeviceExtensionCount;
    // VK_KHR_SWAPCHAIN_EXTENSION_NAME
    const char* enabledDeviceExtensionNames_array[1];

    // Vulkan Device
    VkDevice vkDevice;

    // DeviceQueue
    VkQueue vkQueue;

    // Color format and color Space
    VkFormat vkFormat_color;
    VkColorSpaceKHR vkColorSpaceKHR;

    // Presentation Mode
    VkPresentModeKHR vkPresentModeKHR;

    // Swapchain related
    int winWidth;
    int winHeight;
    VkSwapchainKHR vkSwapchainKHR;
    VkExtent2D vkExtent2D_swapchain;

    // for color images
    // SwapchainImage and swapchainImagesViews related
    uint32_t swapchainImageCount;
    VkImage* swapchainImage_array;
    VkImageView* swapchainImageView_array;

    // for depth image
    VkFormat vkFormat_depth;
    VkImage vkImage_depth;
    VkDeviceMemory vkDeviceMemory_depth;
    VkImageView vkImageView_depth;

    // commandPool
    VkCommandPool vkCommandPool;

    // Command Buffers per scene
    VkCommandBuffer* vkCommandBuffer_scene0_array;
    VkCommandBuffer* vkCommandBuffer_scene1_array;
    VkCommandBuffer* vkCommandBuffer_scene2_array;
    VkCommandBuffer* vkCommandBuffer_scene3_array;

    // RenderPass
    VkRenderPass vkRenderPass;

    // FrameBuffers
    VkFramebuffer* vkFrameBuffer_array;

    // semaphore
    VkSemaphore vkSemaphore_backBuffer;
    VkSemaphore vkSemaphore_renderComplete;

    // Fence
    VkFence* vkFence_array;

    // BuildCommandBuffers
    // clear color values
    VkClearColorValue vkClearColorValue;  // 3 arrays
    VkClearDepthStencilValue vkClearDepthStencilValue;  // for depth

    // Render
    VkBool32 bInitialized;
    uint32_t currentImageIndex;

    // Validation
    BOOL bValidation;
    uint32_t enabledValidationLayerCount;
    const char* enabledValidationLayerNames_array[1]; // for VK_LAYER_KHRONOS_validation
    VkDebugReportCallbackEXT vkDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT_fnptr;

    // VertexBuffer related variables
    GlobalContext_VertexData vertexData_position;
    GlobalContext_VertexData vertexData_texcoord;

    // Uniform related declarations
    GlobalContext_UniformData uniformData_scene0;
    GlobalContext_UniformData uniformData_scene1;
    GlobalContext_UniformData uniformData_scene2;
    GlobalContext_UniformData uniformData_scene3;

    // Shader related variables
    VkShaderModule vkShaderModule_vertex_shader;
    VkShaderModule vkShaderModule_fragment_shader;

    // DescriptorSetLayout related variables
    VkDescriptorSetLayout vkDescriptorSetLayout;

    // PipelineLayout related variables
    VkPipelineLayout vkPipelineLayout;

    // Descriptor pool
    VkDescriptorPool vkDescriptorPool;

    // Descriptor set
    VkDescriptorSet vkDescriptorSet_scene0;
    VkDescriptorSet vkDescriptorSet_scene1;
    VkDescriptorSet vkDescriptorSet_scene2;
    VkDescriptorSet vkDescriptorSet_scene3;

    // Pipeline
    VkViewport vkViewport;
    VkRect2D vkRect2D_scissor; // mostly viewport and scissor dimensions are same
    VkPipeline vkPipeline_scene0;
    VkPipeline vkPipeline_scene1;
    VkPipeline vkPipeline_scene2;
    VkPipeline vkPipeline_scene3;
    VkPipelineCache vkPipelineCache;

    float angle;

    // Texture related global data
    VkImage vkImage_texture;
    VkDeviceMemory vkDeviceMemory_texture;
    VkImageView vkImageView_texture;
    VkSampler vkSampler_texture;

    // Texture related global data for Scene 2
    VkImage vkImage_texture_scene2;
    VkDeviceMemory vkDeviceMemory_texture_scene2;
    VkImageView vkImageView_texture_scene2;
    VkSampler vkSampler_texture_scene2;

    // Texture related global data for Scene 3
    VkImage vkImage_texture_scene3;
    VkDeviceMemory vkDeviceMemory_texture_scene3;
    VkImageView vkImageView_texture_scene3;
    VkSampler vkSampler_texture_scene3;

    // Global
    float gFade;
    BOOL  gScene01DoubleExposureActive;
    BOOL  gScene12CrossfadeActive;
    BOOL  gScene23FocusPullActive;
    float gScene23FocusPullFactor;

    // Scene sequence state
    SequencePhase gSequencePhase;

    BOOL gSequenceActive;
} GlobalContext_Switcher;

#endif // RC_INVOKED

extern Win32WindowContext_Switcher gWin32WindowCtx_Switcher;
extern GlobalContext_Switcher gCtx_Switcher;

typedef struct Win32FunctionTable_Switcher
{
    LRESULT (CALLBACK* WndProc)(HWND, UINT, WPARAM, LPARAM);
    void (*Update)(void);
    VkResult (*Initialize)(void);
    VkResult (*Resize)(int, int);
    VkResult (*Display)(void);
    void (*Uninitialize)(void);
    void (*ToggleFullscreen)(void);
} Win32FunctionTable_Switcher;

extern Win32FunctionTable_Switcher gWin32FunctionTable_Switcher;

typedef struct FunctionTable_Switcher
{
    VkResult (*createVulkanInstance)(void);
    VkResult (*fillInstanceExtensionNames)(void);
    VkResult (*fillValidationLayerNames)(void);
    VkResult (*createValidationCallbackFunction)(void);
    VkResult (*getSupportedSurface)(void);
    VkResult (*getPhysicalDevice)(void);
    VkResult (*printVkInfo)(void);
    VkResult (*fillDeviceExtensionNames)(void);
    VkResult (*createVulkanDevice)(void);
    void (*getDeviceQueue)(void);
    VkResult (*getPhysicalDeviceSurfaceFormatAndColorSpace)(void);
    VkResult (*getPhysicalDevicePresentMode)(void);
    VkResult (*createSwapchain)(VkBool32);
    VkBool32 (*HasStencilComponent)(VkFormat);
    VkResult (*createImagesAndImageViews)(void);
    VkResult (*GetSupportedDepthFormat)(void);
    VkResult (*createCommandPool)(void);
    VkResult (*createCommandBuffers)(void);
    VkResult (*createVertexBuffer)(void);
    VkResult (*createTexture)(const char*);
    VkResult (*createUniformBuffer)(void);
    VkResult (*updateUniformBuffer)(void);
    VkResult (*createShaders)(void);
    VkResult (*createDescriptorSetLayout)(void);
    VkResult (*createPipelineLayout)(void);
    VkResult (*createDescriptorPool)(void);
    VkResult (*createDescriptorSet)(void);
    VkResult (*createRenderPass)(void);
    VkResult (*createPipeline)(void);
    VkResult (*createFrameBuffers)(void);
    VkResult (*createSemaphores)(void);
    VkResult (*createFences)(void);
    VkResult (*buildCommandBuffers)(void);
    VKAPI_ATTR VkBool32 (VKAPI_CALL* debugReportCallback)(VkDebugReportFlagsEXT,
                                                          VkDebugReportObjectTypeEXT,
                                                          uint64_t,
                                                          size_t,
                                                          int32_t,
                                                          const char*,
                                                          const char*,
                                                          void*);
} FunctionTable_Switcher;

extern FunctionTable_Switcher gFunctionTable_Switcher;

void InitializeFunctionTable(void);
void InitializeSceneSwitcherGlobals(void);

// Scene sequence management
void BeginScene0Audio(void);
void StartSceneSequence(void);
void UpdateSceneSequence(void);
void StopSceneSequence(void);
BOOL IsSceneSequenceActive(void);

