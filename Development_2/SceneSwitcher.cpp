//Header file
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>
//header file for texture
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "SceneSwitcher.h"
#include "Scenes.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
// Vulkan related library
#pragma comment(lib, "vulkan-1.lib")

//Callback function declaration
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void Update(void);


Win32WindowContext_Switcher gWin32WindowCtx_Switcher;
GlobalContext_Switcher gCtx_Switcher;
Win32FunctionTable_Switcher gWin32FunctionTable_Switcher;
FunctionTable_Switcher gFunctionTable_Switcher;

typedef struct OffscreenTargetTag
{
    VkImage        colorImage;
    VkDeviceMemory colorMemory;
    VkImageView    colorView;
    VkImage        depthImage;
    VkDeviceMemory depthMemory;
    VkImageView    depthView;
    VkFramebuffer  framebuffer;
} OffscreenTarget;

static OffscreenTarget gOffscreenTargets[4];
static VkRenderPass    gOffscreenRenderPass = VK_NULL_HANDLE;
static VkSampler       gOffscreenSampler     = VK_NULL_HANDLE;
static GlobalContext_UniformData gCompositeUniformData;
static VkDescriptorSet gCompositeDescriptorSet_array[4] =
{
    VK_NULL_HANDLE,
    VK_NULL_HANDLE,
    VK_NULL_HANDLE,
    VK_NULL_HANDLE
};
static VkCommandBuffer* gCompositeCommandBuffer_array = NULL;
static BOOL gScene0AudioIsPlaying = FALSE;

static VkCommandBuffer BeginOneShotCB(void);
static void EndOneShotCB(VkCommandBuffer commandBuffer);
static void ApplyScenePhase(SequencePhase phase);
static void RequestScenePhase(SequencePhase phase);
static VkResult CreateUniformBufferForScene(GlobalContext_UniformData* uniformData);
void BeginScene0Audio(void);

static void ResetOffscreenTargets(void)
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        gOffscreenTargets[i].colorImage = VK_NULL_HANDLE;
        gOffscreenTargets[i].colorMemory = VK_NULL_HANDLE;
        gOffscreenTargets[i].colorView = VK_NULL_HANDLE;
        gOffscreenTargets[i].depthImage = VK_NULL_HANDLE;
        gOffscreenTargets[i].depthMemory = VK_NULL_HANDLE;
        gOffscreenTargets[i].depthView = VK_NULL_HANDLE;
        gOffscreenTargets[i].framebuffer = VK_NULL_HANDLE;
    }
}

static void DestroyOffscreenTargets(void)
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        if (gOffscreenTargets[i].framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(gCtx_Switcher.vkDevice, gOffscreenTargets[i].framebuffer, NULL);
            gOffscreenTargets[i].framebuffer = VK_NULL_HANDLE;
        }

        if (gOffscreenTargets[i].colorView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(gCtx_Switcher.vkDevice, gOffscreenTargets[i].colorView, NULL);
            gOffscreenTargets[i].colorView = VK_NULL_HANDLE;
        }

        if (gOffscreenTargets[i].depthView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(gCtx_Switcher.vkDevice, gOffscreenTargets[i].depthView, NULL);
            gOffscreenTargets[i].depthView = VK_NULL_HANDLE;
        }

        if (gOffscreenTargets[i].colorImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(gCtx_Switcher.vkDevice, gOffscreenTargets[i].colorImage, NULL);
            gOffscreenTargets[i].colorImage = VK_NULL_HANDLE;
        }

        if (gOffscreenTargets[i].colorMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(gCtx_Switcher.vkDevice, gOffscreenTargets[i].colorMemory, NULL);
            gOffscreenTargets[i].colorMemory = VK_NULL_HANDLE;
        }

        if (gOffscreenTargets[i].depthImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(gCtx_Switcher.vkDevice, gOffscreenTargets[i].depthImage, NULL);
            gOffscreenTargets[i].depthImage = VK_NULL_HANDLE;
        }

        if (gOffscreenTargets[i].depthMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(gCtx_Switcher.vkDevice, gOffscreenTargets[i].depthMemory, NULL);
            gOffscreenTargets[i].depthMemory = VK_NULL_HANDLE;
        }
    }
}

static void DestroyCompositeResources(void)
{
    if (gCompositeUniformData.vkBuffer != VK_NULL_HANDLE)
    {
        if (gCtx_Switcher.vkDevice != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(gCtx_Switcher.vkDevice, gCompositeUniformData.vkBuffer, NULL);
        }
        gCompositeUniformData.vkBuffer = VK_NULL_HANDLE;
    }
    if (gCompositeUniformData.vkDeviceMemory != VK_NULL_HANDLE)
    {
        if (gCtx_Switcher.vkDevice != VK_NULL_HANDLE)
        {
            vkFreeMemory(gCtx_Switcher.vkDevice, gCompositeUniformData.vkDeviceMemory, NULL);
        }
        gCompositeUniformData.vkDeviceMemory = VK_NULL_HANDLE;
    }

    if (gOffscreenSampler != VK_NULL_HANDLE)
    {
        if (gCtx_Switcher.vkDevice != VK_NULL_HANDLE)
        {
            vkDestroySampler(gCtx_Switcher.vkDevice, gOffscreenSampler, NULL);
        }
        gOffscreenSampler = VK_NULL_HANDLE;
    }

    if (gCompositeCommandBuffer_array)
    {
        if (gCtx_Switcher.vkDevice != VK_NULL_HANDLE && gCtx_Switcher.vkCommandPool != VK_NULL_HANDLE)
        {
            for (uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; ++i)
            {
                if (gCompositeCommandBuffer_array[i] != VK_NULL_HANDLE)
                {
                    vkFreeCommandBuffers(gCtx_Switcher.vkDevice,
                                         gCtx_Switcher.vkCommandPool,
                                         1,
                                         &gCompositeCommandBuffer_array[i]);
                }
            }
        }
        free(gCompositeCommandBuffer_array);
        gCompositeCommandBuffer_array = NULL;
    }
}

void BeginScene0Audio(void)
{
    if (gScene0AudioIsPlaying)
    {
        return;
    }

    if (PlaySound(TEXT("Omega.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP))
    {
        gScene0AudioIsPlaying = TRUE;
        if (gCtx_Switcher.gpFile)
        {
            fprintf(gCtx_Switcher.gpFile, "BeginScene0Audio() --> Playing Omega.wav\n");
        }
    }
    else if (gCtx_Switcher.gpFile)
    {
        fprintf(gCtx_Switcher.gpFile, "BeginScene0Audio() --> PlaySound failed\n");
    }
}

static VkResult CreateCompositeUniformBuffer(void)
{
    if (gCompositeUniformData.vkBuffer != VK_NULL_HANDLE)
    {
        return VK_SUCCESS;
    }

    VkBufferCreateInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = sizeof(GlobalContext_MyUniformData);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VkResult vkResult = vkCreateBuffer(gCtx_Switcher.vkDevice, &bufferInfo, NULL, &gCompositeUniformData.vkBuffer);
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    VkMemoryRequirements memReq;
    memset(&memReq, 0, sizeof(memReq));
    vkGetBufferMemoryRequirements(gCtx_Switcher.vkDevice, gCompositeUniformData.vkBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;

    uint32_t fallbackIndex = UINT32_MAX;
    allocInfo.memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((memReq.memoryTypeBits & (1u << i)) == 0)
        {
            continue;
        }

        VkMemoryPropertyFlags flags =
            gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags;
        if ((flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            allocInfo.memoryTypeIndex = i;
            break;
        }

        if (fallbackIndex == UINT32_MAX &&
            (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            fallbackIndex = i;
        }
    }

    if (allocInfo.memoryTypeIndex == UINT32_MAX)
    {
        allocInfo.memoryTypeIndex = fallbackIndex;
    }

    if (allocInfo.memoryTypeIndex == UINT32_MAX)
    {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    vkResult = vkAllocateMemory(gCtx_Switcher.vkDevice, &allocInfo, NULL, &gCompositeUniformData.vkDeviceMemory);
    if (vkResult != VK_SUCCESS)
    {
        vkDestroyBuffer(gCtx_Switcher.vkDevice, gCompositeUniformData.vkBuffer, NULL);
        gCompositeUniformData.vkBuffer = VK_NULL_HANDLE;
        return vkResult;
    }

    vkResult = vkBindBufferMemory(gCtx_Switcher.vkDevice,
                                  gCompositeUniformData.vkBuffer,
                                  gCompositeUniformData.vkDeviceMemory,
                                  0);
    if (vkResult != VK_SUCCESS)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCompositeUniformData.vkDeviceMemory, NULL);
        gCompositeUniformData.vkDeviceMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(gCtx_Switcher.vkDevice, gCompositeUniformData.vkBuffer, NULL);
        gCompositeUniformData.vkBuffer = VK_NULL_HANDLE;
        return vkResult;
    }

    GlobalContext_MyUniformData data;
    memset(&data, 0, sizeof(data));
    data.modelMatrix = glm::mat4(1.0f);
    data.viewMatrix = glm::mat4(1.0f);
    data.projectionMatrix = glm::mat4(1.0f);
    data.fade = glm::vec4(0.0f);

    void* mapped = NULL;
    vkResult = vkMapMemory(gCtx_Switcher.vkDevice,
                           gCompositeUniformData.vkDeviceMemory,
                           0,
                           sizeof(data),
                           0,
                           &mapped);
    if (vkResult != VK_SUCCESS || !mapped)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCompositeUniformData.vkDeviceMemory, NULL);
        gCompositeUniformData.vkDeviceMemory = VK_NULL_HANDLE;
        vkDestroyBuffer(gCtx_Switcher.vkDevice, gCompositeUniformData.vkBuffer, NULL);
        gCompositeUniformData.vkBuffer = VK_NULL_HANDLE;
        return vkResult;
    }

    memcpy(mapped, &data, sizeof(data));
    vkUnmapMemory(gCtx_Switcher.vkDevice, gCompositeUniformData.vkDeviceMemory);

    return VK_SUCCESS;
}

static VkResult UpdateCompositeUniformBuffer(float transition, float focusPull)
{
    if (gCompositeUniformData.vkDeviceMemory == VK_NULL_HANDLE)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (transition < 0.0f)
    {
        transition = 0.0f;
    }
    else if (transition > 1.0f)
    {
        transition = 1.0f;
    }

    if (focusPull < 0.0f)
    {
        focusPull = 0.0f;
    }
    else if (focusPull > 1.0f)
    {
        focusPull = 1.0f;
    }

    GlobalContext_MyUniformData data;
    memset(&data, 0, sizeof(data));
    data.modelMatrix = glm::mat4(1.0f);
    data.viewMatrix = glm::mat4(1.0f);
    data.projectionMatrix = glm::mat4(1.0f);
    data.fade = glm::vec4(transition, 1.0f - transition, focusPull, 0.0f);

    void* mapped = NULL;
    VkResult vkResult = vkMapMemory(gCtx_Switcher.vkDevice,
                                    gCompositeUniformData.vkDeviceMemory,
                                    0,
                                    sizeof(data),
                                    0,
                                    &mapped);
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    memcpy(mapped, &data, sizeof(data));
    vkUnmapMemory(gCtx_Switcher.vkDevice, gCompositeUniformData.vkDeviceMemory);

    return VK_SUCCESS;
}

static VkResult EnsureOffscreenSampler(void)
{
    if (gOffscreenSampler != VK_NULL_HANDLE)
    {
        return VK_SUCCESS;
    }

    VkSamplerCreateInfo samplerInfo;
    memset(&samplerInfo, 0, sizeof(samplerInfo));
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    return vkCreateSampler(gCtx_Switcher.vkDevice, &samplerInfo, NULL, &gOffscreenSampler);
}

static VkResult OneShotTransition(const VkImage image,
                                  VkImageAspectFlags aspectMask,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout,
                                  VkAccessFlags srcAccess,
                                  VkAccessFlags dstAccess,
                                  VkPipelineStageFlags srcStage,
                                  VkPipelineStageFlags dstStage)
{
    VkCommandBuffer cb = BeginOneShotCB();

    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cb,
                         srcStage,
                         dstStage,
                         0,
                         0, NULL,
                         0, NULL,
                         1, &barrier);

    EndOneShotCB(cb);
    return VK_SUCCESS;
}

static VkResult CreateOffscreenTargets(void)
{
    DestroyOffscreenTargets();
    VkResult vkResult = CreateCompositeUniformBuffer();
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    vkResult = EnsureOffscreenSampler();
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    VkExtent2D extent = gCtx_Switcher.vkExtent2D_swapchain;

    for (uint32_t i = 0; i < 4; ++i)
    {
        VkImageCreateInfo imageInfo;
        memset(&imageInfo, 0, sizeof(imageInfo));
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = gCtx_Switcher.vkFormat_color;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        vkResult = vkCreateImage(gCtx_Switcher.vkDevice, &imageInfo, NULL, &gOffscreenTargets[i].colorImage);
        if (vkResult != VK_SUCCESS)
        {
            DestroyOffscreenTargets();
            return vkResult;
        }

        VkMemoryRequirements memReq;
        memset(&memReq, 0, sizeof(memReq));
        vkGetImageMemoryRequirements(gCtx_Switcher.vkDevice, gOffscreenTargets[i].colorImage, &memReq);

        VkMemoryAllocateInfo allocInfo;
        memset(&allocInfo, 0, sizeof(allocInfo));
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;

        for (uint32_t m = 0; m < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++m)
        {
            if ((memReq.memoryTypeBits & (1u << m)) != 0 &&
                (gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[m].propertyFlags &
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            {
                allocInfo.memoryTypeIndex = m;
                break;
            }
        }

        vkResult = vkAllocateMemory(gCtx_Switcher.vkDevice, &allocInfo, NULL, &gOffscreenTargets[i].colorMemory);
        if (vkResult != VK_SUCCESS)
        {
            DestroyOffscreenTargets();
            return vkResult;
        }

        vkResult = vkBindImageMemory(gCtx_Switcher.vkDevice,
                                     gOffscreenTargets[i].colorImage,
                                     gOffscreenTargets[i].colorMemory,
                                     0);
        if (vkResult != VK_SUCCESS)
        {
            DestroyOffscreenTargets();
            return vkResult;
        }

        VkImageViewCreateInfo viewInfo;
        memset(&viewInfo, 0, sizeof(viewInfo));
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = gOffscreenTargets[i].colorImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = gCtx_Switcher.vkFormat_color;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        vkResult = vkCreateImageView(gCtx_Switcher.vkDevice, &viewInfo, NULL, &gOffscreenTargets[i].colorView);
        if (vkResult != VK_SUCCESS)
        {
            DestroyOffscreenTargets();
            return vkResult;
        }

        VkImageCreateInfo depthInfo = imageInfo;
        depthInfo.format = gCtx_Switcher.vkFormat_depth;
        depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        vkResult = vkCreateImage(gCtx_Switcher.vkDevice, &depthInfo, NULL, &gOffscreenTargets[i].depthImage);
        if (vkResult != VK_SUCCESS)
        {
            DestroyOffscreenTargets();
            return vkResult;
        }

        vkGetImageMemoryRequirements(gCtx_Switcher.vkDevice, gOffscreenTargets[i].depthImage, &memReq);
        allocInfo.allocationSize = memReq.size;

        for (uint32_t m = 0; m < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++m)
        {
            if ((memReq.memoryTypeBits & (1u << m)) != 0 &&
                (gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[m].propertyFlags &
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            {
                allocInfo.memoryTypeIndex = m;
                break;
            }
        }

        vkResult = vkAllocateMemory(gCtx_Switcher.vkDevice, &allocInfo, NULL, &gOffscreenTargets[i].depthMemory);
        if (vkResult != VK_SUCCESS)
        {
            DestroyOffscreenTargets();
            return vkResult;
        }

        vkResult = vkBindImageMemory(gCtx_Switcher.vkDevice,
                                     gOffscreenTargets[i].depthImage,
                                     gOffscreenTargets[i].depthMemory,
                                     0);
        if (vkResult != VK_SUCCESS)
        {
            DestroyOffscreenTargets();
            return vkResult;
        }

        VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (gFunctionTable_Switcher.HasStencilComponent(gCtx_Switcher.vkFormat_depth))
        {
            depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        memset(&viewInfo, 0, sizeof(viewInfo));
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = gOffscreenTargets[i].depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = gCtx_Switcher.vkFormat_depth;
        viewInfo.subresourceRange.aspectMask = depthAspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        vkResult = vkCreateImageView(gCtx_Switcher.vkDevice, &viewInfo, NULL, &gOffscreenTargets[i].depthView);
        if (vkResult != VK_SUCCESS)
        {
            DestroyOffscreenTargets();
            return vkResult;
        }

        VkImageView attachments[2] =
        {
            gOffscreenTargets[i].colorView,
            gOffscreenTargets[i].depthView
        };

        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = gOffscreenRenderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;

        vkResult = vkCreateFramebuffer(gCtx_Switcher.vkDevice, &fbInfo, NULL, &gOffscreenTargets[i].framebuffer);
        if (vkResult != VK_SUCCESS)
        {
            DestroyOffscreenTargets();
            return vkResult;
        }

        OneShotTransition(gOffscreenTargets[i].colorImage,
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          0,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        OneShotTransition(gOffscreenTargets[i].depthImage,
                          depthAspect,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                          0,
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    }

    return VK_SUCCESS;
}

static VkResult AllocateCompositeDescriptorSets(void)
{
    if (gCompositeDescriptorSet_array[0] != VK_NULL_HANDLE)
    {
        return VK_SUCCESS;
    }

    VkDescriptorSetLayout layouts[4] =
    {
        gCtx_Switcher.vkDescriptorSetLayout,
        gCtx_Switcher.vkDescriptorSetLayout,
        gCtx_Switcher.vkDescriptorSetLayout,
        gCtx_Switcher.vkDescriptorSetLayout
    };

    VkDescriptorSetAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = gCtx_Switcher.vkDescriptorPool;
    allocInfo.descriptorSetCount = _ARRAYSIZE(layouts);
    allocInfo.pSetLayouts = layouts;

    VkDescriptorSet sets[4];
    VkResult vkResult = vkAllocateDescriptorSets(gCtx_Switcher.vkDevice, &allocInfo, sets);
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    for (uint32_t i = 0; i < 4; ++i)
    {
        gCompositeDescriptorSet_array[i] = sets[i];
    }

    return VK_SUCCESS;
}

static void UpdateCompositeDescriptorSets(void)
{
    if (gCompositeUniformData.vkBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    VkDescriptorBufferInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.buffer = gCompositeUniformData.vkBuffer;
    bufferInfo.range = sizeof(GlobalContext_MyUniformData);

    VkWriteDescriptorSet writes[12];
    uint32_t writeCount = 0;
    VkDescriptorImageInfo imageInfos[4];
    memset(imageInfos, 0, sizeof(imageInfos));

    for (uint32_t i = 0; i < 4; ++i)
    {
        if (gCompositeDescriptorSet_array[i] == VK_NULL_HANDLE ||
            gOffscreenTargets[i].colorView == VK_NULL_HANDLE)
        {
            continue;
        }

        VkDescriptorImageInfo* imageInfo = &imageInfos[i];
        imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo->imageView = gOffscreenTargets[i].colorView;
        imageInfo->sampler = gOffscreenSampler;

        VkWriteDescriptorSet* writeUniform = &writes[writeCount++];
        memset(writeUniform, 0, sizeof(*writeUniform));
        writeUniform->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeUniform->dstSet = gCompositeDescriptorSet_array[i];
        writeUniform->dstBinding = 0;
        writeUniform->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeUniform->descriptorCount = 1;
        writeUniform->pBufferInfo = &bufferInfo;

        VkWriteDescriptorSet* writeTexture = &writes[writeCount++];
        memset(writeTexture, 0, sizeof(*writeTexture));
        writeTexture->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeTexture->dstSet = gCompositeDescriptorSet_array[i];
        writeTexture->dstBinding = 1;
        writeTexture->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeTexture->descriptorCount = 1;
        writeTexture->pImageInfo = imageInfo;

        VkWriteDescriptorSet* writeTexture2 = &writes[writeCount++];
        memset(writeTexture2, 0, sizeof(*writeTexture2));
        writeTexture2->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeTexture2->dstSet = gCompositeDescriptorSet_array[i];
        writeTexture2->dstBinding = 2;
        writeTexture2->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeTexture2->descriptorCount = 1;
        writeTexture2->pImageInfo = imageInfo;
    }

    if (writeCount > 0)
    {
        vkUpdateDescriptorSets(gCtx_Switcher.vkDevice, writeCount, writes, 0, NULL);
    }
}

static VkResult EnsureCompositeCommandBuffers(void)
{
    if (gCompositeCommandBuffer_array)
    {
        return VK_SUCCESS;
    }

    if (gCtx_Switcher.swapchainImageCount == 0)
    {
        return VK_SUCCESS;
    }

    gCompositeCommandBuffer_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * gCtx_Switcher.swapchainImageCount);
    if (!gCompositeCommandBuffer_array)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VkCommandBufferAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = gCtx_Switcher.vkCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    for (uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; ++i)
    {
        VkResult vkResult = vkAllocateCommandBuffers(gCtx_Switcher.vkDevice,
                                                     &allocInfo,
                                                     &gCompositeCommandBuffer_array[i]);
        if (vkResult != VK_SUCCESS)
        {
            return vkResult;
        }
    }

    return VK_SUCCESS;
}

static VkResult InitializeOffscreenResources(void)
{
    VkResult vkResult = CreateOffscreenTargets();
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    vkResult = AllocateCompositeDescriptorSets();
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    UpdateCompositeDescriptorSets();

    vkResult = UpdateCompositeUniformBuffer(0.0f, 0.0f);
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    vkResult = EnsureCompositeCommandBuffers();
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    return VK_SUCCESS;
}

void InitializeSceneSwitcherGlobals(void)
{
    memset(&gWin32WindowCtx_Switcher, 0, sizeof(gWin32WindowCtx_Switcher));
    memset(&gCtx_Switcher, 0, sizeof(gCtx_Switcher));
    ResetOffscreenTargets();
    DestroyCompositeResources();
    gCompositeDescriptorSet_array[0] = VK_NULL_HANDLE;
    gCompositeDescriptorSet_array[1] = VK_NULL_HANDLE;
    gCompositeDescriptorSet_array[2] = VK_NULL_HANDLE;
    gCompositeDescriptorSet_array[3] = VK_NULL_HANDLE;

    gWin32WindowCtx_Switcher.gpszAppName = "ARTR";
    gCtx_Switcher.graphicsQueueFamilyIndex_selected = UINT32_MAX;
    gCtx_Switcher.swapchainImageCount = UINT32_MAX;
    gCtx_Switcher.currentImageIndex = UINT32_MAX;
    gCtx_Switcher.winWidth = WIN_WIDTH;
    gCtx_Switcher.winHeight = WIN_HEIGHT;
    gCtx_Switcher.vkColorSpaceKHR = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    gCtx_Switcher.vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
    gCtx_Switcher.vkFormat_color = VK_FORMAT_UNDEFINED;
    gCtx_Switcher.vkFormat_depth = VK_FORMAT_UNDEFINED;
    gCtx_Switcher.bValidation = TRUE;

    // Initialize scene sequence state
    gCtx_Switcher.gSequencePhase = SEQUENCE_PHASE_COMPLETE;
    gCtx_Switcher.gSequenceActive = FALSE;
    gCtx_Switcher.gFade = 0.0f;
    gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
    gCtx_Switcher.gScene12CrossfadeActive = FALSE;
    gCtx_Switcher.gScene23FocusPullActive = FALSE;
    gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
    gScene0AudioIsPlaying = FALSE;

    memset(&gCompositeUniformData, 0, sizeof(gCompositeUniformData));
}

ActiveScene gActiveScene = ACTIVE_SCENE_NONE;

static void ApplyScenePhase(SequencePhase phase)
{
    static SequencePhase sAppliedPhase = SEQUENCE_PHASE_COMPLETE;

    if (phase == sAppliedPhase)
    {
        return;
    }

    sAppliedPhase = phase;

    switch (phase)
    {
    case SEQUENCE_PHASE_SCENE0:
        BeginScene0Audio();
        gActiveScene = ACTIVE_SCENE_SCENE0;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        /*
         * Scene 0 renders directly to the offscreen target that is used as the
         * base layer during compositing.  The fragment shader multiplies the
         * sampled colour by the fade value stored in the uniform buffer, so a
         * fade of 0.0 results in a fully transparent (black) image.  Because
         * Scene 0 is shown immediately when it is requested (for example when
         * pressing F1), the fade factor must be fully opaque; otherwise the
         * composited image is blank even though the scene rendered correctly.
         */
        gCtx_Switcher.gFade = 1.0f;
        fprintf(gCtx_Switcher.gpFile, "ApplyScenePhase() --> Showing Scene0\n");
        break;
    case SEQUENCE_PHASE_SCENE1:
        gActiveScene = ACTIVE_SCENE_SCENE1;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 1.0f;
        fprintf(gCtx_Switcher.gpFile, "ApplyScenePhase() --> Showing Scene1\n");
        break;
    case SEQUENCE_PHASE_SCENE2:
        gActiveScene = ACTIVE_SCENE_SCENE2;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 1.0f;
        fprintf(gCtx_Switcher.gpFile, "ApplyScenePhase() --> Showing Scene2\n");
        break;
    case SEQUENCE_PHASE_SCENE3:
        gActiveScene = ACTIVE_SCENE_SCENE3;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 1.0f;
        gCtx_Switcher.gFade = 1.0f;
        fprintf(gCtx_Switcher.gpFile, "ApplyScenePhase() --> Showing Scene3\n");
        break;
    default:
        gActiveScene = ACTIVE_SCENE_NONE;
        gCtx_Switcher.gScene01DoubleExposureActive = FALSE;
        gCtx_Switcher.gScene12CrossfadeActive = FALSE;
        gCtx_Switcher.gScene23FocusPullActive = FALSE;
        gCtx_Switcher.gScene23FocusPullFactor = 0.0f;
        gCtx_Switcher.gFade = 0.0f;
        fprintf(gCtx_Switcher.gpFile, "ApplyScenePhase() --> Sequence idle\n");
        break;
    }

    Scene1_MarkCommandBufferDirty();
}

static void RequestScenePhase(SequencePhase phase)
{
    if (phase == SEQUENCE_PHASE_SCENE0)
    {
        BeginScene0Audio();
    }

    gCtx_Switcher.gSequenceActive = TRUE;
    gCtx_Switcher.gSequencePhase = phase;
    ApplyScenePhase(phase);
}

/* Scene 1 texture handles (GPU) */
static VkCommandBuffer BeginOneShotCB(void)
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    memset(&commandBufferAllocateInfo, 0, sizeof(commandBufferAllocateInfo));
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = gCtx_Switcher.vkCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(gCtx_Switcher.vkDevice, &commandBufferAllocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo commandBufferBeginInfo;
    memset(&commandBufferBeginInfo, 0, sizeof(commandBufferBeginInfo));
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    return commandBuffer;
}

static void EndOneShotCB(VkCommandBuffer commandBuffer)
{
    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkEndCommandBuffer(commandBuffer);
    vkQueueSubmit(gCtx_Switcher.vkQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(gCtx_Switcher.vkQueue);
    vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &commandBuffer);
}

static VkResult WaitForGpuIdle(void)
{
    if (gCtx_Switcher.vkDevice == VK_NULL_HANDLE)
    {
        return VK_SUCCESS;
    }

    VkResult vkResult = vkDeviceWaitIdle(gCtx_Switcher.vkDevice);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile,
                "WaitForGpuIdle() --> vkDeviceWaitIdle() failed errorcode = %d\n",
                vkResult);
    }

    return vkResult;
}

VkResult CreateTexture2D(const char* path,
                                VkImage* outImg,
                                VkDeviceMemory* outMem,
                                VkImageView* outView,
                                VkSampler* outSampler)
{
    int width = 0, height = 0, channelCount = 0;
    uint8_t* pixels = stbi_load(path, &width, &height, &channelCount, STBI_rgb_alpha);
    if (!pixels || width <= 0 || height <= 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDeviceSize imageSize = (VkDeviceSize)width * (VkDeviceSize)height * 4;

    /* staging buffer */
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkBufferCreateInfo bufferCreateInfo;
    memset(&bufferCreateInfo, 0, sizeof(bufferCreateInfo));
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size  = imageSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    vkCreateBuffer(gCtx_Switcher.vkDevice, &bufferCreateInfo, NULL, &stagingBuffer);

    VkMemoryRequirements memoryRequirements;
    memset(&memoryRequirements, 0, sizeof(memoryRequirements));
    vkGetBufferMemoryRequirements(gCtx_Switcher.vkDevice, stagingBuffer, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo;
    memset(&memoryAllocateInfo, 0, sizeof(memoryAllocateInfo));
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    /* choose HOST_VISIBLE|HOST_COHERENT */
    for (uint32_t i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((memoryRequirements.memoryTypeBits & (1u << i)) != 0 &&
            (gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            memoryAllocateInfo.memoryTypeIndex = i;
            break;
        }
    }
    vkAllocateMemory(gCtx_Switcher.vkDevice, &memoryAllocateInfo, NULL, &stagingMemory);
    vkBindBufferMemory(gCtx_Switcher.vkDevice, stagingBuffer, stagingMemory, 0);

    void* mappedMemory = NULL;
    vkMapMemory(gCtx_Switcher.vkDevice, stagingMemory, 0, imageSize, 0, &mappedMemory);
    memcpy(mappedMemory, pixels, (size_t)imageSize);
    vkUnmapMemory(gCtx_Switcher.vkDevice, stagingMemory);
    stbi_image_free(pixels);

    /* device image */
    VkImageCreateInfo imageCreateInfo;
    memset(&imageCreateInfo, 0, sizeof(imageCreateInfo));
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.extent.width  = (uint32_t)width;
    imageCreateInfo.extent.height = (uint32_t)height;
    imageCreateInfo.extent.depth  = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling  = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage   = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCreateImage(gCtx_Switcher.vkDevice, &imageCreateInfo, NULL, outImg);

    VkMemoryRequirements imageMemoryRequirements;
    memset(&imageMemoryRequirements, 0, sizeof(imageMemoryRequirements));
    vkGetImageMemoryRequirements(gCtx_Switcher.vkDevice, *outImg, &imageMemoryRequirements);

    VkMemoryAllocateInfo imageMemoryAllocateInfo;
    memset(&imageMemoryAllocateInfo, 0, sizeof(imageMemoryAllocateInfo));
    imageMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageMemoryAllocateInfo.allocationSize = imageMemoryRequirements.size;
    for (uint32_t i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((imageMemoryRequirements.memoryTypeBits & (1u << i)) != 0 &&
            (gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            imageMemoryAllocateInfo.memoryTypeIndex = i;
            break;
        }
    }
    vkAllocateMemory(gCtx_Switcher.vkDevice, &imageMemoryAllocateInfo, NULL, outMem);
    vkBindImageMemory(gCtx_Switcher.vkDevice, *outImg, *outMem, 0);

    /* transitions and copy */
    VkCommandBuffer commandBuffer = BeginOneShotCB();
    VkImageMemoryBarrier imageBarrier;
    memset(&imageBarrier, 0, sizeof(imageBarrier));
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = *outImg;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.layerCount = 1;
    imageBarrier.srcAccessMask = 0;
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &imageBarrier);
    EndOneShotCB(commandBuffer);

    commandBuffer = BeginOneShotCB();
    VkBufferImageCopy bufferImageCopyRegion;
    memset(&bufferImageCopyRegion, 0, sizeof(bufferImageCopyRegion));
    bufferImageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopyRegion.imageSubresource.mipLevel   = 0;
    bufferImageCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferImageCopyRegion.imageSubresource.layerCount = 1;
    bufferImageCopyRegion.imageExtent.width  = (uint32_t)width;
    bufferImageCopyRegion.imageExtent.height = (uint32_t)height;
    bufferImageCopyRegion.imageExtent.depth  = 1;
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, *outImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopyRegion);
    EndOneShotCB(commandBuffer);

    commandBuffer = BeginOneShotCB();
    memset(&imageBarrier, 0, sizeof(imageBarrier));
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = *outImg;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.layerCount = 1;
    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, NULL, 0, NULL, 1, &imageBarrier);
    EndOneShotCB(commandBuffer);
    vkFreeMemory(gCtx_Switcher.vkDevice, stagingMemory, NULL);
    vkDestroyBuffer(gCtx_Switcher.vkDevice, stagingBuffer, NULL);

    VkImageViewCreateInfo imageViewCreateInfo;
    memset(&imageViewCreateInfo, 0, sizeof(imageViewCreateInfo));
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = *outImg;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(gCtx_Switcher.vkDevice, &imageViewCreateInfo, NULL, outView);

    VkSamplerCreateInfo samplerCreateInfo;
    memset(&samplerCreateInfo, 0, sizeof(samplerCreateInfo));
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.maxAnisotropy = 16.0f;
    vkCreateSampler(gCtx_Switcher.vkDevice, &samplerCreateInfo, NULL, outSampler);

    return VK_SUCCESS;
}

VkResult CreateCubemap(const char* const faces[6],
                              VkImage* outImg,
                              VkDeviceMemory* outMem,
                              VkImageView* outView,
                              VkSampler* outSampler)
{
    int width = 0, height = 0, channelCount = 0;
    uint8_t* facePixelData[6];
    memset(facePixelData, 0, sizeof(facePixelData));
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        facePixelData[faceIndex] = stbi_load(faces[faceIndex], &width, &height, &channelCount, STBI_rgb_alpha);
        if (!facePixelData[faceIndex])
        {
            for (int i = 0; i < faceIndex; ++i)
            {
                stbi_image_free(facePixelData[i]);
            }
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    VkDeviceSize faceSize = (VkDeviceSize)width * (VkDeviceSize)height * 4;
    VkDeviceSize totalSize    = faceSize * 6;

    VkBuffer stagingBuffer; VkDeviceMemory stagingMemory;
    VkBufferCreateInfo bufferCreateInfo;
    memset(&bufferCreateInfo, 0, sizeof(bufferCreateInfo));
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size  = totalSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    vkCreateBuffer(gCtx_Switcher.vkDevice, &bufferCreateInfo, NULL, &stagingBuffer);
    VkMemoryRequirements memoryRequirements;
    memset(&memoryRequirements, 0, sizeof(memoryRequirements));
    vkGetBufferMemoryRequirements(gCtx_Switcher.vkDevice, stagingBuffer, &memoryRequirements);
    VkMemoryAllocateInfo memoryAllocateInfo;
    memset(&memoryAllocateInfo, 0, sizeof(memoryAllocateInfo));
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    for (uint32_t i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((memoryRequirements.memoryTypeBits & (1u << i)) != 0 &&
            (gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            memoryAllocateInfo.memoryTypeIndex = i;
            break;
        }
    }
    vkAllocateMemory(gCtx_Switcher.vkDevice, &memoryAllocateInfo, NULL, &stagingMemory);
    vkBindBufferMemory(gCtx_Switcher.vkDevice, stagingBuffer, stagingMemory, 0);
    void* mappedMemory = NULL;
    vkMapMemory(gCtx_Switcher.vkDevice, stagingMemory, 0, totalSize, 0, &mappedMemory);
    for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        memcpy((uint8_t*)mappedMemory + (VkDeviceSize)faceIndex * faceSize, facePixelData[faceIndex], (size_t)faceSize);
    }
    vkUnmapMemory(gCtx_Switcher.vkDevice, stagingMemory);
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        stbi_image_free(facePixelData[faceIndex]);
    }

    /* device image (cube) */
    VkImageCreateInfo imageCreateInfo;
    memset(&imageCreateInfo, 0, sizeof(imageCreateInfo));
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.extent.width  = (uint32_t)width;
    imageCreateInfo.extent.height = (uint32_t)height;
    imageCreateInfo.extent.depth  = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 6;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling  = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage   = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCreateImage(gCtx_Switcher.vkDevice, &imageCreateInfo, NULL, outImg);

    VkMemoryRequirements imageMemoryRequirements;
    memset(&imageMemoryRequirements, 0, sizeof(imageMemoryRequirements));
    vkGetImageMemoryRequirements(gCtx_Switcher.vkDevice, *outImg, &imageMemoryRequirements);
    VkMemoryAllocateInfo imageMemoryAllocateInfo;
    memset(&imageMemoryAllocateInfo, 0, sizeof(imageMemoryAllocateInfo));
    imageMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageMemoryAllocateInfo.allocationSize = imageMemoryRequirements.size;
    for (uint32_t i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((imageMemoryRequirements.memoryTypeBits & (1u << i)) != 0 &&
            (gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            imageMemoryAllocateInfo.memoryTypeIndex = i;
            break;
        }
    }
    vkAllocateMemory(gCtx_Switcher.vkDevice, &imageMemoryAllocateInfo, NULL, outMem);
    vkBindImageMemory(gCtx_Switcher.vkDevice, *outImg, *outMem, 0);

    /* to TRANSFER_DST */
    VkCommandBuffer commandBuffer = BeginOneShotCB();
    VkImageMemoryBarrier imageBarrier;
    memset(&imageBarrier, 0, sizeof(imageBarrier));
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = *outImg;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.layerCount = 6;
    imageBarrier.srcAccessMask = 0;
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &imageBarrier);
    EndOneShotCB(commandBuffer);

    /* copy six faces */
    commandBuffer = BeginOneShotCB();
    VkBufferImageCopy regions[6];
    memset(regions, 0, sizeof(regions));
    for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        regions[faceIndex].bufferOffset = (VkDeviceSize)faceIndex * faceSize;
        regions[faceIndex].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[faceIndex].imageSubresource.mipLevel = 0;
        regions[faceIndex].imageSubresource.baseArrayLayer = faceIndex;
        regions[faceIndex].imageSubresource.layerCount = 1;
        regions[faceIndex].imageExtent.width  = (uint32_t)width;
        regions[faceIndex].imageExtent.height = (uint32_t)height;
        regions[faceIndex].imageExtent.depth  = 1;
    }
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, *outImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions);
    EndOneShotCB(commandBuffer);

    /* to SHADER_READ_ONLY */
    commandBuffer = BeginOneShotCB();
    memset(&imageBarrier, 0, sizeof(imageBarrier));
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = *outImg;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.layerCount = 6;
    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, NULL, 0, NULL, 1, &imageBarrier);
    EndOneShotCB(commandBuffer);

    vkFreeMemory(gCtx_Switcher.vkDevice, stagingMemory, NULL);
    vkDestroyBuffer(gCtx_Switcher.vkDevice, stagingBuffer, NULL);

    VkImageViewCreateInfo imageViewCreateInfo;
    memset(&imageViewCreateInfo, 0, sizeof(imageViewCreateInfo));
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = *outImg;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    imageViewCreateInfo.format   = VK_FORMAT_R8G8B8A8_UNORM;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.layerCount = 6;
    vkCreateImageView(gCtx_Switcher.vkDevice, &imageViewCreateInfo, NULL, outView);

    VkSamplerCreateInfo samplerCreateInfo;
    memset(&samplerCreateInfo, 0, sizeof(samplerCreateInfo));
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.maxAnisotropy = 16.0f;
    vkCreateSampler(gCtx_Switcher.vkDevice, &samplerCreateInfo, NULL, outSampler);

    return VK_SUCCESS;
}

static VkResult LoadShaderModuleFromFile(const char* path, VkShaderModule* outModule)
{
    FILE* shaderFile = fopen(path, "rb");
    if (shaderFile == NULL)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (fseek(shaderFile, 0L, SEEK_END) != 0)
    {
        fclose(shaderFile);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    long shaderFileSizeLong = ftell(shaderFile);
    if (shaderFileSizeLong <= 0)
    {
        fclose(shaderFile);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (fseek(shaderFile, 0L, SEEK_SET) != 0)
    {
        fclose(shaderFile);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    size_t shaderFileSize = (size_t)shaderFileSizeLong;
    char* shaderData = (char*)malloc(shaderFileSize);
    if (shaderData == NULL)
    {
        fclose(shaderFile);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    size_t bytesRead = fread(shaderData, shaderFileSize, 1, shaderFile);
    fclose(shaderFile);
    if (bytesRead != 1)
    {
        free(shaderData);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    memset(&shaderModuleCreateInfo, 0, sizeof(shaderModuleCreateInfo));
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = shaderFileSize;
    shaderModuleCreateInfo.pCode = (const uint32_t*)shaderData;

    VkResult result = vkCreateShaderModule(gCtx_Switcher.vkDevice, &shaderModuleCreateInfo, NULL, outModule);
    free(shaderData);
    return result;
}

/* Scene 1 logic lives in Scene1.cpp */
/* ====================== end Scene 1 Integration (state + helpers) ======================= */

//WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
        //local variable declarations
        WNDCLASSEX wndclass;
        HWND hwnd;
        MSG msg;
        TCHAR szAppName[255];
        int bDone = 0;
        VkResult vkResult = VK_SUCCESS;


        //code
        InitializeSceneSwitcherGlobals();

        gCtx_Switcher.gpFile = fopen("SSA_Log.txt", "w+");
        if (gCtx_Switcher.gpFile == NULL)
        {
                MessageBox(NULL, TEXT("Cannot create/open SSA_Log.txt file"), TEXT("FILE IO ERROR"), MB_ICONERROR);
                exit(0);
        }
        else
        {
                fprintf(gCtx_Switcher.gpFile, "WinMain() --> Program started successfully\n");
        }

        InitializeFunctionTable();

        wsprintf(szAppName, TEXT("%s"), gWin32WindowCtx_Switcher.gpszAppName);

        wndclass.cbSize = sizeof(WNDCLASSEX);
        wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wndclass.cbClsExtra = 0;
        wndclass.cbWndExtra = 0;
        wndclass.lpfnWndProc = gWin32FunctionTable_Switcher.WndProc;
	wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hInstance = hInstance;
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = szAppName;
	wndclass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));

	RegisterClassEx(&wndclass);

	hwnd = CreateWindowEx(WS_EX_APPWINDOW,
		szAppName,
		TEXT("SSA_VULKAN"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
		GetSystemMetrics(SM_CXSCREEN)/2 - WIN_WIDTH/2,
		GetSystemMetrics(SM_CYSCREEN)/2 - WIN_HEIGHT/2,
		WIN_WIDTH,
		WIN_HEIGHT,
		NULL, //Parent Window Handle
		NULL, //Menu Handle
		hInstance,
		NULL);

	gWin32WindowCtx_Switcher.ghwnd = hwnd;

    vkResult = gWin32FunctionTable_Switcher.Initialize();
        if(vkResult != VK_SUCCESS)
        {
                fprintf(gCtx_Switcher.gpFile, "WinMain() --> Initialize() is failed\n");
                DestroyWindow(hwnd);
                hwnd = NULL;
        }
        else
        {
                fprintf(gCtx_Switcher.gpFile, "WinMain() --> Initialize() is succedded\n");
        }
        //Error checking of Initialize

	ShowWindow(hwnd, iCmdShow);
    gWin32FunctionTable_Switcher.Update();
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);

	//Game loop
	while (bDone == 0)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				bDone = 1;
                        else
                        {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                        }
		}
		else
		{
			if (gWin32WindowCtx_Switcher.gbActiveWindow == 1)
			{
                                //Here you should call update() for OpenGL rendering
                                gWin32FunctionTable_Switcher.Update();
                                //Here you should call display() for OpenGL rendering
                                if(gWin32WindowCtx_Switcher.gbWindowMinimized == FALSE)
                                {
                                        vkResult = gWin32FunctionTable_Switcher.Display();
                                        if((vkResult != VK_FALSE) && (vkResult != VK_SUCCESS) && (vkResult != VK_ERROR_OUT_OF_DATE_KHR) && (vkResult != VK_SUBOPTIMAL_KHR))
                                        {
                                                fprintf(gCtx_Switcher.gpFile, "WinMain() --> call to Display() failed\n");
                                                bDone = TRUE;
                                        }
                                }
			}
		}
	}

        gWin32FunctionTable_Switcher.Uninitialize();

	return((int)(msg.wParam));
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
        int height = 0;
        int width = 0;
        int length = sizeof(WINDOWPLACEMENT);

        switch (iMsg)
        {

        case WM_CREATE:
                memset(&gWin32WindowCtx_Switcher.wpPrev, 0, length);
                break;

        case WM_SETFOCUS:
                gWin32WindowCtx_Switcher.gbActiveWindow = 1;
                break;

        case WM_KILLFOCUS:
                gWin32WindowCtx_Switcher.gbActiveWindow = 0;
                break;

        case WM_ERASEBKGND:
                return(0);

        case WM_SIZE:
                if(wParam == SIZE_MINIMIZED)
                {
                        gWin32WindowCtx_Switcher.gbWindowMinimized = TRUE;
                }
                else
                {
                        gWin32WindowCtx_Switcher.gbWindowMinimized = FALSE;
                        gWin32FunctionTable_Switcher.Resize(LOWORD(lParam), HIWORD(lParam));
                }

                break;

        case WM_KEYDOWN:
                switch (wParam)
                {
                case VK_ESCAPE:
                        DestroyWindow(hwnd);
                        break;

                case 0x46:
                case 0x66:
                        gWin32FunctionTable_Switcher.ToggleFullscreen();
                        break;

                case VK_F4:
                        Scene1_StartSequence();
                        fprintf(gCtx_Switcher.gpFile, "WndProc() --> Showcase sequence started\n");
                        break;

                case SCENESWITCHER_KEY_SCENE3:
                        Scene1_StopSequence();
                        RequestScenePhase(SEQUENCE_PHASE_SCENE3);
                        fprintf(gCtx_Switcher.gpFile, "WndProc() --> Requested Scene3\n");
                        break;
                case VK_F6:
                        if (gCtx_Switcher.gSequenceActive)
                        {
                            StopSceneSequence();
                        }
                        else
                        {
                            StartSceneSequence();
                        }
                        break;

                case SCENESWITCHER_KEY_SCENE0:
                        Scene1_StopSequence();
                        RequestScenePhase(SEQUENCE_PHASE_SCENE0);
                        fprintf(gCtx_Switcher.gpFile, "WndProc() --> Requested Scene0\n");
                        break;

                case SCENESWITCHER_KEY_SCENE1:
                        Scene1_StopSequence();
                        RequestScenePhase(SEQUENCE_PHASE_SCENE1);
                        fprintf(gCtx_Switcher.gpFile, "WndProc() --> Requested Scene1\n");
                        break;

                case SCENESWITCHER_KEY_SCENE2:
                        Scene1_StopSequence();
                        RequestScenePhase(SEQUENCE_PHASE_SCENE2);
                        fprintf(gCtx_Switcher.gpFile, "WndProc() --> Requested Scene2\n");
                        break;

                default:
                        break;
                }
                break;

        case WM_CHAR:
        {
            /* Scene 1 live controls: Z/X = pan speed −/+, C/V = overlay size −/+ */
            switch ((int)wParam)
            {
            case 'z':
            case 'Z':
            {
                if (gActiveScene == ACTIVE_SCENE_SCENE1)
                {
                    Scene1_AdjustPanSpeed(-10.0f);
                }
            } break;
            case 'x':
            case 'X':
            {
                if (gActiveScene == ACTIVE_SCENE_SCENE1)
                {
                    Scene1_AdjustPanSpeed(+10.0f);
                }
            } break;
            case 'c':
            case 'C':
            {
                if (gActiveScene == ACTIVE_SCENE_SCENE1)
                {
                    Scene1_AdjustOverlaySize(-0.02f);
                }
            } break;
            case 'v':
            case 'V':
            {
                if (gActiveScene == ACTIVE_SCENE_SCENE1)
                {
                    Scene1_AdjustOverlaySize(+0.02f);
                }
            } break;
            default: break;
            }
        } break;

        case WM_CLOSE:
                DestroyWindow(hwnd);
                break;

        case WM_DESTROY:
                gWin32FunctionTable_Switcher.Uninitialize();
                PostQuitMessage(0);
                break;

        default: break;
        }

        return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}


void ToggleFullscreen(void)
{
	//Local variable declaration
	MONITORINFO mi = { sizeof(MONITORINFO) };

	//Code
	if (gWin32WindowCtx_Switcher.gbFullscreen == 0) //If current window is normal(Not Fullscreen)
	{
		gWin32WindowCtx_Switcher.dwStyle = GetWindowLong(gWin32WindowCtx_Switcher.ghwnd, GWL_STYLE); //gWin32WindowCtx_Switcher.dwStyle will get WS_OVERLAPPEDWINDOW information
		if (gWin32WindowCtx_Switcher.dwStyle & WS_OVERLAPPEDWINDOW)
		{
			if (GetWindowPlacement(gWin32WindowCtx_Switcher.ghwnd, &gWin32WindowCtx_Switcher.wpPrev) && GetMonitorInfo(MonitorFromWindow(gWin32WindowCtx_Switcher.ghwnd, MONITORINFOF_PRIMARY), &mi))
			{
				SetWindowLong(gWin32WindowCtx_Switcher.ghwnd, GWL_STYLE, (gWin32WindowCtx_Switcher.dwStyle & ~WS_OVERLAPPEDWINDOW)); //Removing flags of WS_OVERLAPPEDWINDOW
				SetWindowPos(gWin32WindowCtx_Switcher.ghwnd,
					HWND_TOP, //HWND_TOP -> for making it on top order
					mi.rcMonitor.left, // left coordinate of monitor
					mi.rcMonitor.top, // top coordinate of monitor
					mi.rcMonitor.right - mi.rcMonitor.left, // width coordinate of monitor
					mi.rcMonitor.bottom - mi.rcMonitor.top, // height coordinate of monitor
					SWP_NOZORDER | //Window flag --> don't change the Z order
					SWP_FRAMECHANGED); //Window flag --> WM_NCCALCSIZE (Window message calculate Non Client area)
			}
		}
		ShowCursor(FALSE);  //disappear the cursor in full screen or Game mode
		gWin32WindowCtx_Switcher.gbFullscreen = 1;
	}
	else
	{
		SetWindowLong(gWin32WindowCtx_Switcher.ghwnd, GWL_STYLE, (gWin32WindowCtx_Switcher.dwStyle | WS_OVERLAPPEDWINDOW));//Restoring flags of WS_OVERLAPPEDWINDOW
		SetWindowPlacement(gWin32WindowCtx_Switcher.ghwnd, &gWin32WindowCtx_Switcher.wpPrev); //setting placement of window
		SetWindowPos(gWin32WindowCtx_Switcher.ghwnd,
			HWND_TOP,
			0,  //already set in gWin32WindowCtx_Switcher.wpPrev
			0,  //already set in gWin32WindowCtx_Switcher.wpPrev
			0,  //already set in gWin32WindowCtx_Switcher.wpPrev
			0,  //already set in gWin32WindowCtx_Switcher.wpPrev
			SWP_NOMOVE | // do not change x, y cordinates for starting position
			SWP_NOSIZE | // do not change height and width cordinates
			SWP_NOOWNERZORDER | //do not change the position even if its parent window is changed
			SWP_NOZORDER | //Window flag --> don't change the Z order
			SWP_FRAMECHANGED); //Window flag --> WM_NCCALCSIZE (Window message calculate Non Client area)
		ShowCursor(TRUE); //Appear the cursor in full screen or Game mode
		gWin32WindowCtx_Switcher.gbFullscreen = 0;
	}
}



VkResult Initialize(void)
{
//variable delaration
VkResult vkResult = VK_SUCCESS;

//code
vkResult = gFunctionTable_Switcher.createVulkanInstance();
    /*Main points:
        vkEnumerateInstanceExtensionProperties()
        
        struct --> vkApplicationInfo
        struct --> gCtx_Switcher.vkInstanceCreateInfo
        vkCreateInstance()
    */
    
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createVulkanInstance() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createVulkanInstance() is succedded\n");
    }

//CreateVulkan Presntation Surface
vkResult = gFunctionTable_Switcher.getSupportedSurface();
    /* Main points:
        struct --> vkWin32SurfaceCreateInfoKHR
        vkCreateWin32SurfaceKHR()
    */
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> getSupportedSurface() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> getSupportedSurface() is succedded\n");
    }

//Get required Physical Device and its Queue family index
vkResult = gFunctionTable_Switcher.getPhysicalDevice();
    /* Main points:
        vkEnumeratePhysicalDevices()
        vkGetPhysicalDeviceQueueFamilyProperties()
        vkGetPhysicalDeviceSurfaceSupportKHR()
        vkGetPhysicalDeviceMemoryProperties()
        vkGetPhysicalDeviceFeatures()
    */
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> getPhysicalDevice() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> getPhysicalDevice() is succedded\n");
    }
    
//Printing Vulkan Infomration
vkResult = gFunctionTable_Switcher.printVkInfo();
    /* Main points:
        vkGetPhysicalDeviceProperties()
    */
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> printVkInfo() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> printVkInfo() is succedded\n");
    }
    
vkResult = gFunctionTable_Switcher.createVulkanDevice();
    /* Main Points:
        vkEnumerateDeviceExtensionProperties()
        struct --> gCtx_Switcher.vkDeviceQueueCreateInfo
        struct --> gCtx_Switcher.vkDeviceCreateInfo
        vkCreateDevice()
        
    */
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createVulkanDevice() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createVulkanDevice() is succedded\n");
    }
    
//getDeviceQueue
gFunctionTable_Switcher.getDeviceQueue();
    /* Main Points
        vkGetDeviceQueue()
    */

        
//Swapchain
vkResult = gFunctionTable_Switcher.createSwapchain(VK_FALSE);
    /*Main Points:
        vkGetPhysicalDeviceSurfaceFormatsKHR()
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR()
        vkGetPhysicalDeviceSurfacePresentModesKHR()
        
    */
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createSwapchain() is failed %d\n", vkResult);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createSwapchain() is succedded\n");
    }
    
//Create Vulkan Images and ImageViews
vkResult = gFunctionTable_Switcher.createImagesAndImageViews();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createImagesAndImageViews() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createImagesAndImageViews() is succedded\n");
    }
    
//Create Command Pool
vkResult = gFunctionTable_Switcher.createCommandPool();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createCommandPool() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createCommandPool() is succedded\n");
    }
    
//Command Buffers
vkResult = gFunctionTable_Switcher.createCommandBuffers();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createCommandBuffers() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createCommandBuffers() is succedded\n");
    }
    
//create VertexBUffer
vkResult = gFunctionTable_Switcher.createVertexBuffer();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createVertexBuffer() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createVertexBuffer() is succedded\n");
    }
    
//create texture
vkResult = gFunctionTable_Switcher.createTexture("Vijay_Kundali.png");
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createTexture() is failed for Stone texture %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createTexture() is succedded for Stone texture\n");
    }
    
    /* Scene 1: create cubemap + overlay textures */
    vkResult = Scene1_CreateTextures();
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> Scene1_CreateTextures() failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> Scene1_CreateTextures() succeeded\n");
    }

    vkResult = CreateTexture2D("EndCredits.png",
                                &gCtx_Switcher.vkImage_texture_scene2,
                                &gCtx_Switcher.vkDeviceMemory_texture_scene2,
                                &gCtx_Switcher.vkImageView_texture_scene2,
                                &gCtx_Switcher.vkSampler_texture_scene2);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> CreateTexture2D() failed for Scene 2 texture %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> Scene 2 texture created\n");
    }

    vkResult = CreateTexture2D("references_notes.png",
                                &gCtx_Switcher.vkImage_texture_scene3,
                                &gCtx_Switcher.vkDeviceMemory_texture_scene3,
                                &gCtx_Switcher.vkImageView_texture_scene3,
                                &gCtx_Switcher.vkSampler_texture_scene3);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> CreateTexture2D() failed for Scene 3 texture %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> Scene 3 texture created\n");
    }

    /* timers for smooth camera pans */
    srand((unsigned)GetTickCount());
    Scene1_BeginNewPan();

//create Uniform Buffer
vkResult = gFunctionTable_Switcher.createUniformBuffer();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createUniformBuffer() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createUniformBuffer() is succedded\n");
    }
    
//Create RenderPass
vkResult = gFunctionTable_Switcher.createShaders();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createShaders() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createShaders() is succedded\n");
    }
    
    
    
//Create Dewscriptor Set Layout
vkResult = gFunctionTable_Switcher.createDescriptorSetLayout();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createDescriptorSetLayout() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createDescriptorSetLayout() is succedded\n");
    }
    
//Create Descriptor Set Layout
vkResult = gFunctionTable_Switcher.createPipelineLayout();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createPipelineLayout() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createPipelineLayout() is succedded\n");
    }
    
//create Descriptor pool
vkResult = gFunctionTable_Switcher.createDescriptorPool();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createDescriptorPool() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createDescriptorPool() is succedded\n");
    }
    
    //create Descriptor set
    vkResult = gFunctionTable_Switcher.createDescriptorSet();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createDescriptorSet() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createDescriptorPool() is succedded\n");
    }
    
    //Create RenderPass
    vkResult = gFunctionTable_Switcher.createRenderPass();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createRenderPass() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createRenderPass() is succedded\n");
    }
    
    //Create Pipeline
    vkResult = gFunctionTable_Switcher.createPipeline();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createPipeline() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createPipeline() is succedded\n");
    }
    
    
    //CreateBuffer
    vkResult = gFunctionTable_Switcher.createFrameBuffers();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createFrameBuffers() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createFrameBuffers() is succedded\n");
    }

    vkResult = InitializeOffscreenResources();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> InitializeOffscreenResources() failed %d\n", vkResult);
        return vkResult;
    }

    //CreateSemaphores
    vkResult = gFunctionTable_Switcher.createSemaphores();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createSemaphores() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createSemaphores() is succedded\n");
    }
    
    //CreateFences
    vkResult = gFunctionTable_Switcher.createFences();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createFences() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> createFences() is succedded\n");
    }
    
    //Initlaize gCtx_Switcher.vkClearColorValue
    memset((void*)&gCtx_Switcher.vkClearColorValue, 0, sizeof(VkClearColorValue));
    gCtx_Switcher.vkClearColorValue.float32[0] = 0.0f;// R
    gCtx_Switcher.vkClearColorValue.float32[1] = 0.0f;// G
    gCtx_Switcher.vkClearColorValue.float32[2] = 0.0f;// B
    gCtx_Switcher.vkClearColorValue.float32[3] = 1.0f;//Analogous to glClearColor()
    
    memset((void*)&gCtx_Switcher.vkClearDepthStencilValue, 0, sizeof(VkClearDepthStencilValue));
    //setDefaultClearDepth
    gCtx_Switcher.vkClearDepthStencilValue.depth = 1.0f; //flaot value
    //setDeafaultStencilValue
    gCtx_Switcher.vkClearDepthStencilValue.stencil = 0; //uint32_t value
    
    //BuildCommandBuffers
    vkResult = gFunctionTable_Switcher.buildCommandBuffers();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> buildCommandBuffers() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "Initialize() --> buildCommandBuffers() is succedded\n");
    }
    
    //Initialization is completed
    gCtx_Switcher.bInitialized = TRUE;
    fprintf(gCtx_Switcher.gpFile, "Initialize() --> Initialization is completed successfully\n");
    
    return vkResult;
}


VkResult Resize(int width, int height)
{
//variables
VkResult vkResult = VK_SUCCESS;
	//code
	if (height == 0)
		height = 1;
    
    gWin32WindowCtx_Switcher.giHeight = height;
    gWin32WindowCtx_Switcher.giWidth = width;
    

    //check the gCtx_Switcher.bInitialized variable
    if(gCtx_Switcher.bInitialized == FALSE)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> Initialization yet not completed or failed\n");  
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
    
    //As recreation of swapchain is needed we are goring to repeat many steps of initialized again, hence set bINitialized to \False again
    gCtx_Switcher.bInitialized = FALSE;

    //Set gloabl winwidth and winheight variable
    gCtx_Switcher.winWidth = width;
    gCtx_Switcher.winHeight = height;
    
    //wait for device to complete inhand tasks
    vkDeviceWaitIdle(gCtx_Switcher.vkDevice);
    fprintf(gCtx_Switcher.gpFile, "Resize() --> vkDeviceWaitIdle() is done\n");

    DestroyCompositeResources();
    DestroyOffscreenTargets();

    //check presence of swapchain
    if(gCtx_Switcher.vkSwapchainKHR == VK_NULL_HANDLE)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> swapchain is already NULL cannot proceed\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
    }
    
    //Destroy vkframebuffer
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount ; i++)
    {
        vkDestroyFramebuffer(gCtx_Switcher.vkDevice, gCtx_Switcher.vkFrameBuffer_array[i], NULL);
    }
    if(gCtx_Switcher.vkFrameBuffer_array)
    {
        free(gCtx_Switcher.vkFrameBuffer_array);
        gCtx_Switcher.vkFrameBuffer_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "Resize() --> gCtx_Switcher.vkFrameBuffer_array() is done\n");
    }
    
    //Destroy Commandbuffer
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        if(gCtx_Switcher.vkCommandBuffer_scene0_array)
        {
            vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &gCtx_Switcher.vkCommandBuffer_scene0_array[i]);
        }
        if(gCtx_Switcher.vkCommandBuffer_scene1_array)
        {
            vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &gCtx_Switcher.vkCommandBuffer_scene1_array[i]);
        }
        if(gCtx_Switcher.vkCommandBuffer_scene2_array)
        {
            vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &gCtx_Switcher.vkCommandBuffer_scene2_array[i]);
        }
        if(gCtx_Switcher.vkCommandBuffer_scene3_array)
        {
            vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &gCtx_Switcher.vkCommandBuffer_scene3_array[i]);
        }
        fprintf(gCtx_Switcher.gpFile, "Resize() --> vkFreeCommandBuffers() is done\n");
    }

    if(gCtx_Switcher.vkCommandBuffer_scene0_array)
    {
        free(gCtx_Switcher.vkCommandBuffer_scene0_array);
        gCtx_Switcher.vkCommandBuffer_scene0_array = NULL;
    }
    if(gCtx_Switcher.vkCommandBuffer_scene1_array)
    {
        free(gCtx_Switcher.vkCommandBuffer_scene1_array);
        gCtx_Switcher.vkCommandBuffer_scene1_array = NULL;
    }
    if(gCtx_Switcher.vkCommandBuffer_scene2_array)
    {
        free(gCtx_Switcher.vkCommandBuffer_scene2_array);
        gCtx_Switcher.vkCommandBuffer_scene2_array = NULL;
    }
    if(gCtx_Switcher.vkCommandBuffer_scene3_array)
    {
        free(gCtx_Switcher.vkCommandBuffer_scene3_array);
        gCtx_Switcher.vkCommandBuffer_scene3_array = NULL;
    }
    if(gCtx_Switcher.vkCommandBuffer_scene3_array)
    {
        free(gCtx_Switcher.vkCommandBuffer_scene3_array);
        gCtx_Switcher.vkCommandBuffer_scene3_array = NULL;
    }
    fprintf(gCtx_Switcher.gpFile, "Resize() --> command buffers freed\n");
    
    
    //Destroy Pipeline
    if(gCtx_Switcher.vkPipeline_scene0)
    {
        vkDestroyPipeline(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipeline_scene0, NULL);
        gCtx_Switcher.vkPipeline_scene0 = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Resize() --> vkDestroyPipeline() for scene0 is done\n");
    }

    if(gCtx_Switcher.vkPipeline_scene1)
    {
        vkDestroyPipeline(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipeline_scene1, NULL);
        gCtx_Switcher.vkPipeline_scene1 = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Resize() --> vkDestroyPipeline() for scene1 is done\n");
    }
    if(gCtx_Switcher.vkPipeline_scene2)
    {
        vkDestroyPipeline(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipeline_scene2, NULL);
        gCtx_Switcher.vkPipeline_scene2 = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Resize() --> vkDestroyPipeline() for scene2 is done\n");
    }
    if(gCtx_Switcher.vkPipeline_scene3)
    {
        vkDestroyPipeline(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipeline_scene3, NULL);
        gCtx_Switcher.vkPipeline_scene3 = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Resize() --> vkDestroyPipeline() for scene3 is done\n");
    }
    
    //Destroy gCtx_Switcher.vkPipelineLayout
    if(gCtx_Switcher.vkPipelineLayout)
    {
        vkDestroyPipelineLayout(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipelineLayout, NULL);
        gCtx_Switcher.vkPipelineLayout = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Resize() --> gCtx_Switcher.vkPipelineLayout() is done\n");
    }
    
    //Destroy Renderpass
    if(gCtx_Switcher.vkRenderPass)
    {
        vkDestroyRenderPass(gCtx_Switcher.vkDevice, gCtx_Switcher.vkRenderPass, NULL);
        gCtx_Switcher.vkRenderPass = VK_NULL_HANDLE;
    }

    if (gOffscreenRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(gCtx_Switcher.vkDevice, gOffscreenRenderPass, NULL);
        gOffscreenRenderPass = VK_NULL_HANDLE;
    }
    
    //destroy depth image view
    if(gCtx_Switcher.vkImageView_depth)
    {
        vkDestroyImageView(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImageView_depth, NULL);
        gCtx_Switcher.vkImageView_depth = VK_NULL_HANDLE;
    }
    
    //destroy device memory for depth image
    if(gCtx_Switcher.vkDeviceMemory_depth)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vkDeviceMemory_depth, NULL);
        gCtx_Switcher.vkDeviceMemory_depth = VK_NULL_HANDLE;
    }
    
    if(gCtx_Switcher.vkImage_depth)
    {
        vkDestroyImage(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImage_depth, NULL);
        gCtx_Switcher.vkImage_depth = VK_NULL_HANDLE;
    }
    //destory image views
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        vkDestroyImageView(gCtx_Switcher.vkDevice, gCtx_Switcher.swapchainImageView_array[i], NULL);
        fprintf(gCtx_Switcher.gpFile, "Resize() --> vkDestoryImageView() is done\n");
    }
    
    if(gCtx_Switcher.swapchainImageView_array)
    {
        free(gCtx_Switcher.swapchainImageView_array);
        gCtx_Switcher.swapchainImageView_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "Resize() --> gCtx_Switcher.swapchainImageView_array is freed\n");
    }
    
    //free swapchainImages
    // for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    // {
        // vkDestroyImage(gCtx_Switcher.vkDevice, gCtx_Switcher.swapchainImage_array[i], NULL);
        // fprintf(gCtx_Switcher.gpFile, "Resize() --> vkDestroyImage() is done\n");
    // }
    
    
    if(gCtx_Switcher.swapchainImage_array)
    {
        free(gCtx_Switcher.swapchainImage_array);
        gCtx_Switcher.swapchainImage_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "Resize() --> gCtx_Switcher.swapchainImage_array is freed\n");
    }
    
    //Destory Swapchain
    if(gCtx_Switcher.vkSwapchainKHR)
    {
        vkDestroySwapchainKHR(gCtx_Switcher.vkDevice, gCtx_Switcher.vkSwapchainKHR, NULL);
        gCtx_Switcher.vkSwapchainKHR = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Resize() --> vkSwapchainCreateInfoKHR() is done\n");
    }
    
    //RECREATE FOR RESIZE
    
//Swapchain
vkResult = gFunctionTable_Switcher.createSwapchain(VK_FALSE);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> createSwapchain() is failed %d\n", vkResult);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
//Create Vulkan Images and ImageViews
vkResult = gFunctionTable_Switcher.createImagesAndImageViews();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> createImagesAndImageViews() is failed %d\n", vkResult);
    }
    
//Create RenderPass
vkResult = gFunctionTable_Switcher.createRenderPass();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> createRenderPass() is failed %d\n", vkResult);
    }
    
    //Create Descriptor Set Layout
    vkResult = gFunctionTable_Switcher.createPipelineLayout();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> createPipelineLayout() is failed %d\n", vkResult);
    }
    
//Create Pipeline
vkResult = gFunctionTable_Switcher.createPipeline();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> createPipeline() is failed %d\n", vkResult);
    }
      
//CreateBuffer
vkResult = gFunctionTable_Switcher.createFrameBuffers();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> createFrameBuffers() is failed %d\n", vkResult);
    }

vkResult = InitializeOffscreenResources();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> InitializeOffscreenResources() failed %d\n", vkResult);
        return vkResult;
    }

//Command Buffers
vkResult = gFunctionTable_Switcher.createCommandBuffers();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> createCommandBuffers() is failed %d\n", vkResult);
    }
    
//BuildCommandBuffers
vkResult = gFunctionTable_Switcher.buildCommandBuffers();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Resize() --> buildCommandBuffers() is failed %d\n", vkResult);
    }

    gCtx_Switcher.bInitialized = TRUE;

    return vkResult;
}



VkResult Display(void)
{
//variable declarations
VkResult vkResult = VK_SUCCESS;

    //code
    // if control comes here before initlaization is completed, return FALSE
    if(gCtx_Switcher.bInitialized == FALSE)
    {
        fprintf(gCtx_Switcher.gpFile, "Display() --> Initialization yet not completed\n");
        return (VkResult)VK_FALSE;
    }
    
    //acquire index of next swapchin image
    //if timour occurs, then function returns VK_NOT_READY
    vkResult = vkAcquireNextImageKHR(gCtx_Switcher.vkDevice,
                                     gCtx_Switcher.vkSwapchainKHR,
                                     UINT64_MAX, //waiting time in nanaoseconds for swapchain to get the image
                                     gCtx_Switcher.vkSemaphore_backBuffer, //semaphore, waiting for another queue to relaease the image held by another queue demanded by swapchain, (InterQueue semaphore)
                                     VK_NULL_HANDLE, //Fence, when you want to halt host also, for device::: (Use Semaphore and fences exclusively, using both is not recommended(Redbook)
                                     &gCtx_Switcher.currentImageIndex);
    if(vkResult != VK_SUCCESS)
    {
        if((vkResult == VK_ERROR_OUT_OF_DATE_KHR) || (vkResult == VK_SUBOPTIMAL_KHR))
        {
            gWin32FunctionTable_Switcher.Resize(gCtx_Switcher.winWidth, gCtx_Switcher.winHeight);
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "Display() --> vkAcquireNextImageKHR() is failed errorcode = %d\n", vkResult);
            return vkResult;   
        }
    }
    
    //use fence to allow host to wait for completion of execution of previous commandbuffer
    vkResult = vkWaitForFences(gCtx_Switcher.vkDevice,
                               1, //waiting for how many fences
                               &gCtx_Switcher.vkFence_array[gCtx_Switcher.currentImageIndex], //Which fence
                               VK_TRUE, // wait till all fences get signalled(Blocking and unblocking function)
                               UINT64_MAX); //waiting time in nanaoseconds
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Display() --> vkWaitForFences() is failed errorcode = %d\n", vkResult);
        return vkResult;
    }

    //Now make Fences execution of next command buffer
    vkResult = vkResetFences(gCtx_Switcher.vkDevice,
                             1, //How many fences to reset
                             &gCtx_Switcher.vkFence_array[gCtx_Switcher.currentImageIndex]);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Display() --> vkResetFences() is failed errorcode = %d\n", vkResult);
        return vkResult;
    }

    BOOL didWaitForIdle = FALSE;

    if (Scene1_HasPendingOverlay())
    {
        VkResult idleResult = WaitForGpuIdle();
        if (idleResult != VK_SUCCESS)
        {
            return idleResult;
        }
        didWaitForIdle = TRUE;
        VkResult bindResult = Scene1_BindPendingOverlay(gCtx_Switcher.vkDescriptorSet_scene1);
        if (bindResult != VK_SUCCESS)
        {
            return bindResult;
        }
    }

    if (Scene1_IsCommandBufferDirty())
    {
        if (!didWaitForIdle)
        {
            VkResult idleResult = WaitForGpuIdle();
            if (idleResult != VK_SUCCESS)
            {
                return idleResult;
            }
            didWaitForIdle = TRUE;
        }

        VkResult rr = gFunctionTable_Switcher.buildCommandBuffers();
        if (rr != VK_SUCCESS)
        {
            return rr;
        }
        Scene1_ClearCommandBufferDirty();
    }

    //One of the mmeber of vkSubmitinfo structure requires array of pipeline stages, we have only one of the completion of color attachment output, still we need 1 member array
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    //Declare memset and initialize VkSubmitInfo structure
    VkSubmitInfo vkSubmitInfo;
    memset((void*)&vkSubmitInfo, 0, sizeof(VkSubmitInfo));
    
    vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkSubmitInfo.pNext = NULL;
    vkSubmitInfo.pWaitDstStageMask = &waitDstStageMask;
    vkSubmitInfo.waitSemaphoreCount = 1;
    vkSubmitInfo.pWaitSemaphores = &gCtx_Switcher.vkSemaphore_backBuffer;
    VkCommandBuffer submitBuffers[5];
    uint32_t submitCount = 0;

    VkCommandBuffer scene0Cb = gCtx_Switcher.vkCommandBuffer_scene0_array[gCtx_Switcher.currentImageIndex];
    VkCommandBuffer scene1Cb = gCtx_Switcher.vkCommandBuffer_scene1_array[gCtx_Switcher.currentImageIndex];
    VkCommandBuffer scene2Cb = gCtx_Switcher.vkCommandBuffer_scene2_array[gCtx_Switcher.currentImageIndex];
    VkCommandBuffer scene3Cb = gCtx_Switcher.vkCommandBuffer_scene3_array[gCtx_Switcher.currentImageIndex];
    VkCommandBuffer compositeCb = (gCompositeCommandBuffer_array) ?
                                  gCompositeCommandBuffer_array[gCtx_Switcher.currentImageIndex] : VK_NULL_HANDLE;

    if (scene0Cb != VK_NULL_HANDLE) submitBuffers[submitCount++] = scene0Cb;
    if (scene1Cb != VK_NULL_HANDLE) submitBuffers[submitCount++] = scene1Cb;
    if (scene2Cb != VK_NULL_HANDLE) submitBuffers[submitCount++] = scene2Cb;
    if (scene3Cb != VK_NULL_HANDLE) submitBuffers[submitCount++] = scene3Cb;
    if (compositeCb != VK_NULL_HANDLE) submitBuffers[submitCount++] = compositeCb;

    if (submitCount == 0)
    {
        fprintf(gCtx_Switcher.gpFile, "Display() --> no command buffers to submit\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkSubmitInfo.commandBufferCount = submitCount;
    vkSubmitInfo.pCommandBuffers = submitBuffers;
    vkSubmitInfo.signalSemaphoreCount = 1;
    vkSubmitInfo.pSignalSemaphores = &gCtx_Switcher.vkSemaphore_renderComplete;
    
    //Now submit our work to the Queue
    vkResult = vkQueueSubmit(gCtx_Switcher.vkQueue,
                             1,
                             &vkSubmitInfo,
                             gCtx_Switcher.vkFence_array[gCtx_Switcher.currentImageIndex]);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Display() --> vkQueueSubmit() is failed errorcode = %d\n", vkResult);
        return vkResult;
    }
    
    //We are going to present rendered image after declaring and initlaizing VkPresentInfoKHR structure
    VkPresentInfoKHR vkPresentInfoKHR;
    memset((void*)&vkPresentInfoKHR, 0, sizeof(VkPresentInfoKHR));
    
    vkPresentInfoKHR.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    vkPresentInfoKHR.pNext = NULL;
    vkPresentInfoKHR.swapchainCount = 1;
    vkPresentInfoKHR.pSwapchains = &gCtx_Switcher.vkSwapchainKHR;
    vkPresentInfoKHR.pImageIndices = &gCtx_Switcher.currentImageIndex;
    vkPresentInfoKHR.waitSemaphoreCount = 1;
    vkPresentInfoKHR.pWaitSemaphores = &gCtx_Switcher.vkSemaphore_renderComplete;
    
    //Now present the Queue
    vkResult = vkQueuePresentKHR(gCtx_Switcher.vkQueue, &vkPresentInfoKHR);
    if(vkResult != VK_SUCCESS)
    {
        if((vkResult == VK_ERROR_OUT_OF_DATE_KHR) || (vkResult == VK_SUBOPTIMAL_KHR))
        {
            gWin32FunctionTable_Switcher.Resize(gCtx_Switcher.winWidth, gCtx_Switcher.winHeight);
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "Display() --> vkQueuePresentKHR() is failed errorcode = %d\n", vkResult);
            return vkResult;
        }
    }
    
vkResult = gFunctionTable_Switcher.updateUniformBuffer();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Display() --> updateUniformBuffer() is failed errorcode = %d\n", vkResult);
    }
        
    //validation
    vkDeviceWaitIdle(gCtx_Switcher.vkDevice);    
    
    return vkResult;
}
    
void Update(void)
{
    // Update scene sequence timing and transitions
    UpdateSceneSequence();
    
    Scene1_UpdateSequence();

    if (Scene1_IsSequenceActive())
    {
        if (gActiveScene == ACTIVE_SCENE_SCENE1)
        {
            Scene1_UpdateCameraAnim();
        }
        return;
    }

    if (gActiveScene == ACTIVE_SCENE_SCENE1)
    {
        Scene1_UpdateCameraAnim();
    }

    // Only update fade if neither sequence system is active (manual control)
    if (!gCtx_Switcher.gSequenceActive && !Scene1_IsSequenceActive())
    {
        float targetFade = (gActiveScene == ACTIVE_SCENE_NONE) ? 0.0f : 1.0f;
        if (gCtx_Switcher.gFade != targetFade)
        {
            gCtx_Switcher.gFade = targetFade;
        }
    }
    
    Scene1_UpdateBlendFade(gCtx_Switcher.gFade);
}

void Uninitialize(void)
{
        //code
        if (gWin32WindowCtx_Switcher.gbFullscreen == 1)
        {
                gWin32WindowCtx_Switcher.dwStyle = GetWindowLong(gWin32WindowCtx_Switcher.ghwnd, GWL_STYLE);
                SetWindowLong(gWin32WindowCtx_Switcher.ghwnd, GWL_STYLE, (gWin32WindowCtx_Switcher.dwStyle | WS_OVERLAPPEDWINDOW));//Restoring flags of WS_OVERLAPPEDWINDOW
                SetWindowPlacement(gWin32WindowCtx_Switcher.ghwnd, &gWin32WindowCtx_Switcher.wpPrev); //setting placement of window
                SetWindowPos(gWin32WindowCtx_Switcher.ghwnd,
                        HWND_TOP,
                        0,  //already set in gWin32WindowCtx_Switcher.wpPrev
                        0,  //already set in gWin32WindowCtx_Switcher.wpPrev
                        0,  //already set in gWin32WindowCtx_Switcher.wpPrev
                        0,  //already set in gWin32WindowCtx_Switcher.wpPrev
                        SWP_NOMOVE | // do not change x, y cordinates for starting position
                        SWP_NOSIZE | // do not change height and width cordinates
                        SWP_NOOWNERZORDER | //do not change the position even if its parent window is changed
                        SWP_NOZORDER | //Window flag --> don't change the Z order
                        SWP_FRAMECHANGED); //Window flag --> WM_NCCALCSIZE (Window message calculate Non Client area)
                ShowCursor(TRUE); //Appear the cursor in full screen or Game mode
        }

	/**************Shader removal code*******************/


	if (gWin32WindowCtx_Switcher.ghdc)
	{
		ReleaseDC(gWin32WindowCtx_Switcher.ghwnd, gWin32WindowCtx_Switcher.ghdc);
		gWin32WindowCtx_Switcher.ghdc = NULL;
	}

    //synchronisation function
    vkDeviceWaitIdle(gCtx_Switcher.vkDevice);
    fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDeviceWaitIdle() is done\n");

    DestroyCompositeResources();
    DestroyOffscreenTargets();

    //DestroyFences
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        vkDestroyFence(gCtx_Switcher.vkDevice, gCtx_Switcher.vkFence_array[i], NULL);
    }
    
    if(gCtx_Switcher.vkFence_array)
    {
        free(gCtx_Switcher.vkFence_array);
        gCtx_Switcher.vkFence_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyFence() is done\n");
    }
    
    //DestroySemaphore
    if(gCtx_Switcher.vkSemaphore_renderComplete)
    {
        vkDestroySemaphore(gCtx_Switcher.vkDevice, gCtx_Switcher.vkSemaphore_renderComplete, NULL);
        gCtx_Switcher.vkSemaphore_renderComplete = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroySemaphore() for gCtx_Switcher.vkSemaphore_renderComplete is done\n");
    }
    
    if(gCtx_Switcher.vkSemaphore_backBuffer)
    {
        vkDestroySemaphore(gCtx_Switcher.vkDevice, gCtx_Switcher.vkSemaphore_backBuffer, NULL);
        gCtx_Switcher.vkSemaphore_backBuffer = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroySemaphore() for gCtx_Switcher.vkSemaphore_backBuffer is done\n");
    }
    
    //vkframebuffer
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount ; i++)
    {
        vkDestroyFramebuffer(gCtx_Switcher.vkDevice, gCtx_Switcher.vkFrameBuffer_array[i], NULL);
    }
    if(gCtx_Switcher.vkFrameBuffer_array)
    {
        free(gCtx_Switcher.vkFrameBuffer_array);
        gCtx_Switcher.vkFrameBuffer_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> gCtx_Switcher.vkFrameBuffer_array() is done\n");
    }
    
    //Destroy Descriptor Pool
    //When Descriptor pool is destroyed, descriptor set created  by that pool get destroyed implicitly
    if(gCtx_Switcher.vkDescriptorPool)
    {
        vkDestroyDescriptorPool(gCtx_Switcher.vkDevice, gCtx_Switcher.vkDescriptorPool, NULL);
        gCtx_Switcher.vkDescriptorPool = VK_NULL_HANDLE;
        gCtx_Switcher.vkDescriptorSet_scene0 = VK_NULL_HANDLE;
        gCtx_Switcher.vkDescriptorSet_scene1 = VK_NULL_HANDLE;
        gCtx_Switcher.vkDescriptorSet_scene2 = VK_NULL_HANDLE;
        gCtx_Switcher.vkDescriptorSet_scene3 = VK_NULL_HANDLE;
        gCompositeDescriptorSet_array[0] = VK_NULL_HANDLE;
        gCompositeDescriptorSet_array[1] = VK_NULL_HANDLE;
        gCompositeDescriptorSet_array[2] = VK_NULL_HANDLE;
        gCompositeDescriptorSet_array[3] = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> gCtx_Switcher.vkDescriptorPool and DescriptorSet() is done\n");
    }
    
    //VkDescritporSetLayout
    if(gCtx_Switcher.vkPipelineLayout)
    {
        vkDestroyPipelineLayout(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipelineLayout, NULL);
        gCtx_Switcher.vkPipelineLayout = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> gCtx_Switcher.vkPipelineLayout() is done\n");
    }
    
    //VkDescritporSetLayout
    if(gCtx_Switcher.vkDescriptorSetLayout)
    {
        vkDestroyDescriptorSetLayout(gCtx_Switcher.vkDevice, gCtx_Switcher.vkDescriptorSetLayout, NULL);
        gCtx_Switcher.vkDescriptorSetLayout = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> gCtx_Switcher.vkDescriptorSetLayout() is done\n");
    }
    
    if(gCtx_Switcher.vkPipeline_scene0)
    {
        vkDestroyPipeline(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipeline_scene0, NULL);
        gCtx_Switcher.vkPipeline_scene0 = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyPipeline() for scene0 is done\n");
    }

    if(gCtx_Switcher.vkPipeline_scene1)
    {
        vkDestroyPipeline(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipeline_scene1, NULL);
        gCtx_Switcher.vkPipeline_scene1 = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyPipeline() for scene1 is done\n");
    }
    if(gCtx_Switcher.vkPipeline_scene2)
    {
        vkDestroyPipeline(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipeline_scene2, NULL);
        gCtx_Switcher.vkPipeline_scene2 = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyPipeline() for scene2 is done\n");
    }
    if(gCtx_Switcher.vkPipeline_scene3)
    {
        vkDestroyPipeline(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipeline_scene3, NULL);
        gCtx_Switcher.vkPipeline_scene3 = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyPipeline() for scene3 is done\n");
    }
    
    //Renderpass
    if(gCtx_Switcher.vkRenderPass)
    {
        vkDestroyRenderPass(gCtx_Switcher.vkDevice, gCtx_Switcher.vkRenderPass, NULL);
        gCtx_Switcher.vkRenderPass = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyRenderPass() is done\n");
    }

    if (gOffscreenRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(gCtx_Switcher.vkDevice, gOffscreenRenderPass, NULL);
        gOffscreenRenderPass = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> offscreen vkDestroyRenderPass() is done\n");
    }
    
    //destroy shader modules
    if(gCtx_Switcher.vkShaderModule_fragment_shader)
    {
        vkDestroyShaderModule(gCtx_Switcher.vkDevice, gCtx_Switcher.vkShaderModule_fragment_shader, NULL);
        gCtx_Switcher.vkShaderModule_fragment_shader = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyShaderModule() is done\n");
    }
    
    if(gCtx_Switcher.vkShaderModule_vertex_shader)
    {
        vkDestroyShaderModule(gCtx_Switcher.vkDevice, gCtx_Switcher.vkShaderModule_vertex_shader, NULL);
        gCtx_Switcher.vkShaderModule_vertex_shader = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyShaderModule() is done\n");
    }
    
    /* Scene 1 shader modules */
    if (gShaderModule_fragment_scene1)
    {
        vkDestroyShaderModule(gCtx_Switcher.vkDevice, gShaderModule_fragment_scene1, NULL);
        gShaderModule_fragment_scene1 = VK_NULL_HANDLE;
    }
    if (gShaderModule_vertex_scene1)
    {
        vkDestroyShaderModule(gCtx_Switcher.vkDevice, gShaderModule_vertex_scene1, NULL);
        gShaderModule_vertex_scene1 = VK_NULL_HANDLE;
    }
    if (gShaderModule_fragment_scene2)
    {
        vkDestroyShaderModule(gCtx_Switcher.vkDevice, gShaderModule_fragment_scene2, NULL);
        gShaderModule_fragment_scene2 = VK_NULL_HANDLE;
    }
    if (gShaderModule_vertex_scene2)
    {
        vkDestroyShaderModule(gCtx_Switcher.vkDevice, gShaderModule_vertex_scene2, NULL);
        gShaderModule_vertex_scene2 = VK_NULL_HANDLE;
    }
    if (gShaderModule_fragment_scene3)
    {
        vkDestroyShaderModule(gCtx_Switcher.vkDevice, gShaderModule_fragment_scene3, NULL);
        gShaderModule_fragment_scene3 = VK_NULL_HANDLE;
    }
    if (gShaderModule_vertex_scene3)
    {
        vkDestroyShaderModule(gCtx_Switcher.vkDevice, gShaderModule_vertex_scene3, NULL);
        gShaderModule_vertex_scene3 = VK_NULL_HANDLE;
    }
    Scene1_DestroyTextures();

    //Destroy uniform buffers
    if(gCtx_Switcher.uniformData_scene0.vkBuffer)
    {
        vkDestroyBuffer(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene0.vkBuffer, NULL);
        gCtx_Switcher.uniformData_scene0.vkBuffer = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.uniformData_scene0.vkDeviceMemory)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene0.vkDeviceMemory, NULL);
        gCtx_Switcher.uniformData_scene0.vkDeviceMemory = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.uniformData_scene1.vkBuffer)
    {
        vkDestroyBuffer(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene1.vkBuffer, NULL);
        gCtx_Switcher.uniformData_scene1.vkBuffer = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.uniformData_scene1.vkDeviceMemory)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene1.vkDeviceMemory, NULL);
        gCtx_Switcher.uniformData_scene1.vkDeviceMemory = VK_NULL_HANDLE;
    }

    if(gCtx_Switcher.uniformData_scene2.vkBuffer)
    {
        vkDestroyBuffer(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene2.vkBuffer, NULL);
        gCtx_Switcher.uniformData_scene2.vkBuffer = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.uniformData_scene2.vkDeviceMemory)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene2.vkDeviceMemory, NULL);
        gCtx_Switcher.uniformData_scene2.vkDeviceMemory = VK_NULL_HANDLE;
    }

    if(gCtx_Switcher.uniformData_scene3.vkBuffer)
    {
        vkDestroyBuffer(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene3.vkBuffer, NULL);
        gCtx_Switcher.uniformData_scene3.vkBuffer = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.uniformData_scene3.vkDeviceMemory)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene3.vkDeviceMemory, NULL);
        gCtx_Switcher.uniformData_scene3.vkDeviceMemory = VK_NULL_HANDLE;
    }
    
    if(gCtx_Switcher.vkSampler_texture)
    {
        vkDestroySampler(gCtx_Switcher.vkDevice, gCtx_Switcher.vkSampler_texture, NULL);
        gCtx_Switcher.vkSampler_texture = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroySampler() for gCtx_Switcher.vkSampler_texture is done\n");
    }
    
    if(gCtx_Switcher.vkImageView_texture)
    {
        vkDestroyImageView(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImageView_texture, NULL);
        gCtx_Switcher.vkImageView_texture = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroySampler() for gCtx_Switcher.vkImageView_texture is done\n");
    }
    
    if(gCtx_Switcher.vkDeviceMemory_texture)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vkDeviceMemory_texture, NULL);
        gCtx_Switcher.vkDeviceMemory_texture = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkFreeMemory() for gCtx_Switcher.vkDeviceMemory_texture is done\n");
    }
    
    if(gCtx_Switcher.vkImage_texture)
    {
        vkDestroyImage(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImage_texture, NULL);
        gCtx_Switcher.vkImage_texture = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyImage() for gCtx_Switcher.vkImage_texture is done\n");
    }

    if(gCtx_Switcher.vkSampler_texture_scene2)
    {
        vkDestroySampler(gCtx_Switcher.vkDevice, gCtx_Switcher.vkSampler_texture_scene2, NULL);
        gCtx_Switcher.vkSampler_texture_scene2 = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.vkImageView_texture_scene2)
    {
        vkDestroyImageView(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImageView_texture_scene2, NULL);
        gCtx_Switcher.vkImageView_texture_scene2 = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.vkDeviceMemory_texture_scene2)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vkDeviceMemory_texture_scene2, NULL);
        gCtx_Switcher.vkDeviceMemory_texture_scene2 = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.vkImage_texture_scene2)
    {
        vkDestroyImage(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImage_texture_scene2, NULL);
        gCtx_Switcher.vkImage_texture_scene2 = VK_NULL_HANDLE;
    }

    if(gCtx_Switcher.vkSampler_texture_scene3)
    {
        vkDestroySampler(gCtx_Switcher.vkDevice, gCtx_Switcher.vkSampler_texture_scene3, NULL);
        gCtx_Switcher.vkSampler_texture_scene3 = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.vkImageView_texture_scene3)
    {
        vkDestroyImageView(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImageView_texture_scene3, NULL);
        gCtx_Switcher.vkImageView_texture_scene3 = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.vkDeviceMemory_texture_scene3)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vkDeviceMemory_texture_scene3, NULL);
        gCtx_Switcher.vkDeviceMemory_texture_scene3 = VK_NULL_HANDLE;
    }
    if(gCtx_Switcher.vkImage_texture_scene3)
    {
        vkDestroyImage(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImage_texture_scene3, NULL);
        gCtx_Switcher.vkImage_texture_scene3 = VK_NULL_HANDLE;
    }

    
    //Vertex Tecoord buffer
    if(gCtx_Switcher.vertexData_texcoord.vkDeviceMemory)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_texcoord.vkDeviceMemory, NULL);
        gCtx_Switcher.vertexData_texcoord.vkDeviceMemory = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkFreeMemory() is done\n");
    }
    
    if(gCtx_Switcher.vertexData_texcoord.vkBuffer)
    {
        vkDestroyBuffer(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_texcoord.vkBuffer, NULL);
        gCtx_Switcher.vertexData_texcoord.vkBuffer = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyBuffer() is done\n");
    }

    //Vertex position BUffer
    if(gCtx_Switcher.vertexData_position.vkDeviceMemory)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_position.vkDeviceMemory, NULL);
        gCtx_Switcher.vertexData_position.vkDeviceMemory = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkFreeMemory() is done\n");
    }
    
    if(gCtx_Switcher.vertexData_position.vkBuffer)
    {
        vkDestroyBuffer(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_position.vkBuffer, NULL);
        gCtx_Switcher.vertexData_position.vkBuffer = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyBuffer() is done\n");
    }

    
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        if(gCtx_Switcher.vkCommandBuffer_scene0_array)
        {
            vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &gCtx_Switcher.vkCommandBuffer_scene0_array[i]);
        }
        if(gCtx_Switcher.vkCommandBuffer_scene1_array)
        {
            vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &gCtx_Switcher.vkCommandBuffer_scene1_array[i]);
        }
        if(gCtx_Switcher.vkCommandBuffer_scene2_array)
        {
            vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &gCtx_Switcher.vkCommandBuffer_scene2_array[i]);
        }
    }
    fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkFreeCommandBuffers() is done\n");

    if(gCtx_Switcher.vkCommandBuffer_scene0_array)
    {
        free(gCtx_Switcher.vkCommandBuffer_scene0_array);
        gCtx_Switcher.vkCommandBuffer_scene0_array = NULL;
    }
    if(gCtx_Switcher.vkCommandBuffer_scene1_array)
    {
        free(gCtx_Switcher.vkCommandBuffer_scene1_array);
        gCtx_Switcher.vkCommandBuffer_scene1_array = NULL;
    }
    if(gCtx_Switcher.vkCommandBuffer_scene2_array)
    {
        free(gCtx_Switcher.vkCommandBuffer_scene2_array);
        gCtx_Switcher.vkCommandBuffer_scene2_array = NULL;
    }
    fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> command buffer arrays freed\n");
    
    
    if(gCtx_Switcher.vkCommandPool)
    {
        vkDestroyCommandPool(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, NULL);
        gCtx_Switcher.vkCommandPool = NULL;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyCommandPool() is done\n");
    }
    
    //destroy depth image view
    if(gCtx_Switcher.vkImageView_depth)
    {
        vkDestroyImageView(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImageView_depth, NULL);
        gCtx_Switcher.vkImageView_depth = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestoryImageView() is done for depth\n");
    }
    
    //destroy device memory for depth image
    if(gCtx_Switcher.vkDeviceMemory_depth)
    {
        vkFreeMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vkDeviceMemory_depth, NULL);
        gCtx_Switcher.vkDeviceMemory_depth = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkFreeMemory() is done for depth\n");
    }
    
    if(gCtx_Switcher.vkImage_depth)
    {
        vkDestroyImage(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImage_depth, NULL);
        gCtx_Switcher.vkImage_depth = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestoryImage() is done for depth\n");
    }
    
    //destory image views
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        vkDestroyImageView(gCtx_Switcher.vkDevice, gCtx_Switcher.swapchainImageView_array[i], NULL);
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestoryImageView() is done for color\n");
    }
    
    if(gCtx_Switcher.swapchainImageView_array)
    {
        free(gCtx_Switcher.swapchainImageView_array);
        gCtx_Switcher.swapchainImageView_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> gCtx_Switcher.swapchainImageView_array is freed\n");
    }
    
    //free swapchainImages
    // for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    // {
        // vkDestroyImage(gCtx_Switcher.vkDevice, gCtx_Switcher.swapchainImage_array[i], NULL);
        // fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyImage() is done\n");
    // }
    
    if(gCtx_Switcher.swapchainImage_array)
    {
        free(gCtx_Switcher.swapchainImage_array);
        gCtx_Switcher.swapchainImage_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> gCtx_Switcher.swapchainImage_array is freed\n");
    }
    
  
    
    //Destory Swapchain
    if(gCtx_Switcher.vkSwapchainKHR)
    {
        vkDestroySwapchainKHR(gCtx_Switcher.vkDevice, gCtx_Switcher.vkSwapchainKHR, NULL);
        gCtx_Switcher.vkSwapchainKHR = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkSwapchainCreateInfoKHR() is done\n");
    }

    //No need to Destroy/Uninitialize the DeviceQueue

    //Destroy vulkan device
    if(gCtx_Switcher.vkDevice)
    {
        vkDestroyDevice(gCtx_Switcher.vkDevice, NULL);
        gCtx_Switcher.vkDevice = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestoryDevice() is done\n");
    }

    //No need to free slected physical device

    //Destroy gCtx_Switcher.vkSurfaceKHR:This function is generic and not platform specific
    if(gCtx_Switcher.vkSurfaceKHR)
    {
        vkDestroySurfaceKHR(gCtx_Switcher.vkInstance, gCtx_Switcher.vkSurfaceKHR, NULL);
        gCtx_Switcher.vkSurfaceKHR = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroySurfaceKHR() is done\n");
    }
    
    //Validation destroying
    if(gCtx_Switcher.vkDebugReportCallbackEXT && gCtx_Switcher.vkDestroyDebugReportCallbackEXT_fnptr)
    {
        gCtx_Switcher.vkDestroyDebugReportCallbackEXT_fnptr(gCtx_Switcher.vkInstance, 
                                              gCtx_Switcher.vkDebugReportCallbackEXT,
                                              NULL);
        gCtx_Switcher.vkDebugReportCallbackEXT = VK_NULL_HANDLE;
        gCtx_Switcher.vkDestroyDebugReportCallbackEXT_fnptr = NULL;
    }
    
    
    //Destroy Vulkan Instance
    if(gCtx_Switcher.vkInstance)
    {
        vkDestroyInstance(gCtx_Switcher.vkInstance, NULL);
        gCtx_Switcher.vkInstance = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> vkDestroyInstance() is done\n");
    }
    
    if (gCtx_Switcher.gpFile)
	{
		fprintf(gCtx_Switcher.gpFile, "Uninitialize() --> Program terminated successfully\n");
		fclose(gCtx_Switcher.gpFile);
		gCtx_Switcher.gpFile = NULL;
	}
}

/***************Definition of Vulkan functions***********************/
/********************************************************************/


VkResult createVulkanInstance(void)
{
//variable declarations
VkResult vkResult = VK_SUCCESS;

    //code
//Step1: Fill and initialize required extension names and count in global variable
vkResult = gFunctionTable_Switcher.fillInstanceExtensionNames();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> fillInstanceExtensionNames() is failed\n");
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> fillInstanceExtensionNames() is succedded\n");
    }

if(gCtx_Switcher.bValidation == TRUE)
{
//Fill validation Layer Names
vkResult = gFunctionTable_Switcher.fillValidationLayerNames();
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> fillValidationLayerNames() is failed\n");
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> fillValidationLayerNames() is succedded\n");
        }
    }


    //Step1: Initialize struct VkApplicationInfo
    VkApplicationInfo vkApplicationInfo;
    memset((void*)&vkApplicationInfo, 0, sizeof(VkApplicationInfo));

    vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; //type safety, generic names
    vkApplicationInfo.pNext = NULL; //Linked List
    vkApplicationInfo.pApplicationName = gWin32WindowCtx_Switcher.gpszAppName;
    vkApplicationInfo.applicationVersion = 1;
    vkApplicationInfo.pEngineName = gWin32WindowCtx_Switcher.gpszAppName;
    vkApplicationInfo.engineVersion = 1;
    vkApplicationInfo.apiVersion = VK_API_VERSION_1_4;


    //Step3: Initialize struct VkInstanceCreateInfo by using information in Step1 and Step2
    VkInstanceCreateInfo vkInstanceCreateInfo;
    memset((void*)&vkInstanceCreateInfo, 0, sizeof(VkInstanceCreateInfo));
    vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInstanceCreateInfo.pNext = NULL;
    vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
    vkInstanceCreateInfo.enabledExtensionCount = gCtx_Switcher.enabledInstanceExtensionCount;
    vkInstanceCreateInfo.ppEnabledExtensionNames = gCtx_Switcher.enabledInstanceExtensionNames_array;
    
    if(gCtx_Switcher.bValidation == TRUE)
    {
        vkInstanceCreateInfo.enabledLayerCount = gCtx_Switcher.enabledValidationLayerCount;
        vkInstanceCreateInfo.ppEnabledLayerNames = gCtx_Switcher.enabledValidationLayerNames_array;
    }
    else
    {
        vkInstanceCreateInfo.enabledLayerCount = 0;
        vkInstanceCreateInfo.ppEnabledLayerNames = NULL;
    }

    /*
	// Provided by VK_VERSION_1_0
		VkResult vkCreateInstance(
		const VkInstanceCreateInfo* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkInstance* pInstance);

		• pCreateInfo is a pointer to a VkInstanceCreateInfo structure controlling creation of the instance.
		• pAllocator controls host memory allocation as described in the Memory Allocation chapter.
		• pInstance points a VkInstance handle in which the resulting instance is returned.
	 */
    //Step4: Call VkCreateInstance() to get gCtx_Switcher.vkInstance in a global variable and do error checking
    vkResult = vkCreateInstance(&vkInstanceCreateInfo,
                                 NULL,  //no custom Memory allocater
                                 &gCtx_Switcher.vkInstance);
    if(vkResult == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> vkCreateInstance:: vkCreateInstance failed due to incompatible driver %d\n", vkResult);
        return vkResult;
    }
    else if(vkResult == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> vkCreateInstance:: vkCreateInstance failed due to required extension not present %d\n", vkResult);
        return vkResult;
    }
    else if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> vkCreateInstance:: vkCreateInstance failed due to unknown reason %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> vkCreateInstance:: vkCreateInstance succedded\n");
    }
    
    //do for validation callbacks
if(gCtx_Switcher.bValidation == TRUE)
{
vkResult = gFunctionTable_Switcher.createValidationCallbackFunction();
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> createValidationCallbackFunction() is failed\n");
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "createVulkanInstance() --> createValidationCallbackFunction() is succedded\n");
        }
    }


   //Step5: Destroy

    return vkResult;
}


VkResult fillInstanceExtensionNames(void)
{
    //variable declaration
    VkResult vkResult = VK_SUCCESS;

    //Step1: Find how many Instacne Extension are supported by the vulkan driver of this version and keep the count in local variable
    uint32_t instanceExtensionCount = 0;

    memset((void*)gCtx_Switcher.enabledInstanceExtensionNames_array, 0,
           sizeof(gCtx_Switcher.enabledInstanceExtensionNames_array));
    gCtx_Switcher.enabledInstanceExtensionCount = 0;

    /*
    // Provided by VK_VERSION_1_0
        VkResult vkEnumerateInstanceExtensionProperties(
        const char* pLayerName,
        uint32_t* pPropertyCount,
        VkExtensionProperties* pProperties);
    
        • pLayerName is either NULL or a pointer to a null-terminated UTF-8 string naming the layer to
          retrieve extensions from.
        • pPropertyCount is a pointer to an integer related to the number of extension properties available
          or queried, as described below.
        • pProperties is either NULL or a pointer to an array of VkExtensionProperties structures
    */

    vkResult = vkEnumerateInstanceExtensionProperties(NULL, //Which layer's extenion is needed: Mention extension name: For all driver's extension use NULL
                                                      &instanceExtensionCount,
                                                      NULL); // Instance Extensions Properties array: As we dont have count, so its NULL
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> 1st call to vkEnumerateInstanceExtensionProperties() is failed\n");
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> 1st call to vkEnumerateInstanceExtensionProperties() is succedded\n");
    }



    //Step2: Allocate and fill VkExtensionProperties corresponding to above count
    VkExtensionProperties* vkExtensionProperties_array = NULL;
    vkExtensionProperties_array = (VkExtensionProperties*) malloc (sizeof(VkExtensionProperties) * instanceExtensionCount);
    //Should be error checking for malloc: assert() can also be used

    vkResult = vkEnumerateInstanceExtensionProperties(NULL, //Which layer's extenion is needed: Mention extension name: For all driver's extension use NULL
                                                      &instanceExtensionCount,
                                                      vkExtensionProperties_array); // Instance Extensions Properties array: As we have count, so it is value
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> 2nd call to vkEnumerateInstanceExtensionProperties() is failed\n");
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> 2nd call to vkEnumerateInstanceExtensionProperties() is succedded\n");
    }



    //Step3: Fill and Display a local string array of extension names obtained from vkExtensionProperties
    char** instanceExtensionNames_array = NULL;

    instanceExtensionNames_array = (char**)malloc(sizeof(char*) * instanceExtensionCount);
    //Should be error checking for malloc: assert() can also be used
    for(uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        instanceExtensionNames_array[i] = (char*)malloc(sizeof(char) * strlen(vkExtensionProperties_array[i].extensionName) + 1);
        memcpy(instanceExtensionNames_array[i], vkExtensionProperties_array[i].extensionName, strlen(vkExtensionProperties_array[i].extensionName) + 1);
        fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> Vulkan Instance Extension names = %s\n", instanceExtensionNames_array[i]);
    }



   //Step4: As not required henceforth, free the vkExtensionProperties_array;
   free(vkExtensionProperties_array);
   vkExtensionProperties_array = NULL;



   // Step5: Find whether below extension names contains our required two extensions
   //VK_KHR_SURFACE_EXTENSION_NAME
   //VK_KHR_WIN32_SURFACE_EXTENSION_NAME
   VkBool32 vulkanSurfaceExtensionFound = VK_FALSE;
   VkBool32 vulkanWin32SurfaceExtensionFound = VK_FALSE;
   VkBool32 vulkanDebugReportExtensionFound = VK_FALSE;

    for(uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        if(strcmp(instanceExtensionNames_array[i], VK_KHR_SURFACE_EXTENSION_NAME) == 0)
        {
            vulkanSurfaceExtensionFound = VK_TRUE;
            gCtx_Switcher.enabledInstanceExtensionNames_array[gCtx_Switcher.enabledInstanceExtensionCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
        }
        
        if(strcmp(instanceExtensionNames_array[i], VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == 0)
        {
            vulkanWin32SurfaceExtensionFound = VK_TRUE;
            gCtx_Switcher.enabledInstanceExtensionNames_array[gCtx_Switcher.enabledInstanceExtensionCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
        }
        
        if(strcmp(instanceExtensionNames_array[i], VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0)
        {
            vulkanDebugReportExtensionFound = VK_TRUE;
            if(gCtx_Switcher.bValidation == TRUE)
            {
                gCtx_Switcher.enabledInstanceExtensionNames_array[gCtx_Switcher.enabledInstanceExtensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
            }
            else
            {
                //array will not have entry of VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
            }
        }
    }



    //Step 6:
    //As not required henceforth, free the local string array
    for(uint32_t i = 0; i < instanceExtensionCount; i++)
    {
        free(instanceExtensionNames_array[i]);
    }
    free(instanceExtensionNames_array);



    //Step7:Print whether our vulkan driver supports our required extension names or not
    if(vulkanSurfaceExtensionFound == VK_FALSE)
    {
        // return hardcoded failure
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> VK_KHR_SURFACE_EXTENSION_NAME not found\n");
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> VK_KHR_SURFACE_EXTENSION_NAME found\n");
    }

    if(vulkanWin32SurfaceExtensionFound == VK_FALSE)
    {
        // return hardcoded failure
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> VK_KHR_WIN32_SURFACE_EXTENSION_NAME not found\n");
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> VK_KHR_WIN32_SURFACE_EXTENSION_NAME found\n");
    }
    
    if(vulkanDebugReportExtensionFound == VK_FALSE)
    {
        if(gCtx_Switcher.bValidation == TRUE)
        {
            // return hardcoded failure
            vkResult = VK_ERROR_INITIALIZATION_FAILED;
            fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> VK_EXT_DEBUG_REPORT_EXTENSION_NAME not found:: Validation is ON But required VK_EXT_DEBUG_REPORT_EXTENSION_NAME is not supported\n");
            return vkResult;
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> VK_EXT_DEBUG_REPORT_EXTENSION_NAME not found:: Validation is OFF But required VK_EXT_DEBUG_REPORT_EXTENSION_NAME is not supported\n");
        }
    }
    else
    {
        if(gCtx_Switcher.bValidation == TRUE)
        {
            fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> VK_EXT_DEBUG_REPORT_EXTENSION_NAME found:: Validation is ON and required VK_EXT_DEBUG_REPORT_EXTENSION_NAME is supported\n");
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> VK_EXT_DEBUG_REPORT_EXTENSION_NAME found:: Validation is OFF and required VK_EXT_DEBUG_REPORT_EXTENSION_NAME is supported\n");
        }
    }


    //Step8: Print only Enabled Extension Names 
    for(uint32_t i = 0; i < gCtx_Switcher.enabledInstanceExtensionCount; i++)
    {
         fprintf(gCtx_Switcher.gpFile, "fillInstanceExtensionNames() --> Enabled vulkan Instance extension Names = %s\n", gCtx_Switcher.enabledInstanceExtensionNames_array[i]);
    }

    return vkResult;
}


VkResult fillValidationLayerNames(void)
{
    //code
    //variables
    VkResult vkResult = VK_SUCCESS;
    uint32_t validationLayerCount = 0;

    memset((void*)gCtx_Switcher.enabledValidationLayerNames_array, 0,
           sizeof(gCtx_Switcher.enabledValidationLayerNames_array));
    gCtx_Switcher.enabledValidationLayerCount = 0;
    
    vkResult = vkEnumerateInstanceLayerProperties(&validationLayerCount,
                                                  NULL); // Instance Validation Properties array: As we dont have count, so its NULL
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "fillValidationLayerNames() --> 1st call to vkEnumerateInstanceLayerProperties() is failed: error code %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "fillValidationLayerNames() --> 1st call to vkEnumerateInstanceLayerProperties() is succedded\n");
    }
    
    VkLayerProperties* vkLayerProperties_array = NULL;
    vkLayerProperties_array = (VkLayerProperties*) malloc (sizeof(VkLayerProperties) * validationLayerCount);
    //Should be error checking for malloc: assert() can also be used

    vkResult = vkEnumerateInstanceLayerProperties(&validationLayerCount,
                                                  vkLayerProperties_array); // Instance Validation Properties array: As we dont have count, so its NULL
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "fillValidationLayerNames() --> 2nd call to vkEnumerateInstanceLayerProperties() is failed: error code %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "fillValidationLayerNames() --> 2nd call to vkEnumerateInstanceLayerProperties() is succedded\n");
    }
    
    char** validationLayerNames_array = NULL;
    validationLayerNames_array = (char**) malloc(sizeof(char*) * validationLayerCount);
    //Should be error checking for malloc: assert() can also be used
    for(uint32_t i = 0; i < validationLayerCount; i++)
    {
        validationLayerNames_array[i] = (char*)malloc(sizeof(char) * strlen(vkLayerProperties_array[i].layerName) + 1);
        //Should be error checking for malloc: assert() can also be used
        memcpy(validationLayerNames_array[i], vkLayerProperties_array[i].layerName, strlen(vkLayerProperties_array[i].layerName) + 1);
        fprintf(gCtx_Switcher.gpFile, "fillValidationLayerNames() --> Vulkan Validation Layer names = %s\n", vkLayerProperties_array[i].layerName);
    }
    
    if(vkLayerProperties_array) 
        free(vkLayerProperties_array);
    vkLayerProperties_array = NULL;

    // Step5: Find whether below layer names contains our required two extensions
    //VK_KHR_SURFACE_EXTENSION_NAME
    VkBool32 vulkanValidationLayerFound = VK_FALSE;
    for(uint32_t i = 0; i < validationLayerCount; i++)
    {
        if(strcmp(validationLayerNames_array[i], "VK_LAYER_KHRONOS_validation") == 0)
        {
            vulkanValidationLayerFound = VK_TRUE;
            gCtx_Switcher.enabledValidationLayerNames_array[gCtx_Switcher.enabledValidationLayerCount++] = "VK_LAYER_KHRONOS_validation";
        }
    }
    
     //As not required henceforth, free the local string array
    for(uint32_t i = 0; i < validationLayerCount; i++)
    {
        free(validationLayerNames_array[i]);
    }
    free(validationLayerNames_array);
    
    if(gCtx_Switcher.bValidation == TRUE)
    {
        //Step7:Print whether our vulkan driver supports our required extension names or not
        if(vulkanValidationLayerFound == VK_FALSE)
        {
            // return hardcoded failure
            vkResult = VK_ERROR_INITIALIZATION_FAILED;
            fprintf(gCtx_Switcher.gpFile, "fillValidationLayerNames() --> VK_LAYER_KHRONOS_validation not supported\n");
            return vkResult;
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "fillValidationLayerNames() --> VK_LAYER_KHRONOS_validation is supported\n");
        }
    }
    
    //Step8: Print only Enabled validation layer Names
    for(uint32_t i = 0; i < gCtx_Switcher.enabledValidationLayerCount; i++)
    {
         fprintf(gCtx_Switcher.gpFile, "fillValidationLayerNames() --> Enabled vulkan validation layer Names = %s\n", gCtx_Switcher.enabledValidationLayerNames_array[i]);
    }
    
    return (vkResult);    
}


VkResult createValidationCallbackFunction(void)
{
    //code
    //function declarations
    VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(VkDebugReportFlagsEXT, 
                                                       VkDebugReportObjectTypeEXT,
                                                       uint64_t,
                                                       size_t,
                                                       int32_t,
                                                       const char*,
                                                       const char*,
                                                       void*);
    
    //variables
    VkResult vkResult = VK_SUCCESS;
    
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT_fnptr = NULL;
    
    //Get the required function pointers
    vkCreateDebugReportCallbackEXT_fnptr = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(gCtx_Switcher.vkInstance, "vkCreateDebugReportCallbackEXT");
    if(vkCreateDebugReportCallbackEXT_fnptr == NULL)
    {
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gCtx_Switcher.gpFile, "createValidationCallbackFunction() --> vkGetInstanceProcAddr() is failed to get function pointer for vkCreateDebugReportCallbackEXT \n");
        return (vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createValidationCallbackFunction() --> vkGetInstanceProcAddr() is succedded to get function pointer for vkCreateDebugReportCallbackEXT \n");
    }
    
    
   // Assign to the global function pointer (no local shadowing)
    gCtx_Switcher.vkDestroyDebugReportCallbackEXT_fnptr = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(gCtx_Switcher.vkInstance, "vkDestroyDebugReportCallbackEXT");
    if(gCtx_Switcher.vkDestroyDebugReportCallbackEXT_fnptr == NULL)
    {
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gCtx_Switcher.gpFile, "createValidationCallbackFunction() --> vkGetInstanceProcAddr() is failed to get function pointer for vkDestroyDebugReportCallbackEXT \n");
        return (vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createValidationCallbackFunction() --> vkGetInstanceProcAddr() is succedded to get function pointer for vkDestroyDebugReportCallbackEXT \n");
    }
    
    
    //Get the vulkanDebugReportCallback object
    VkDebugReportCallbackCreateInfoEXT vkDebugReportCallbackCreateInfoEXT;
    memset((void*)&vkDebugReportCallbackCreateInfoEXT, 0, sizeof(VkDebugReportCallbackCreateInfoEXT));
    vkDebugReportCallbackCreateInfoEXT.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    vkDebugReportCallbackCreateInfoEXT.pNext = NULL;
    vkDebugReportCallbackCreateInfoEXT.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    vkDebugReportCallbackCreateInfoEXT.pfnCallback = gFunctionTable_Switcher.debugReportCallback;
    vkDebugReportCallbackCreateInfoEXT.pUserData = NULL;
    
    vkResult = vkCreateDebugReportCallbackEXT_fnptr(gCtx_Switcher.vkInstance,
                                                    &vkDebugReportCallbackCreateInfoEXT,
                                                    NULL,
                                                    &gCtx_Switcher.vkDebugReportCallbackEXT);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createValidationCallbackFunction() --> vkCreateDebugReportCallbackEXT_fnptr() is failed: error code %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createValidationCallbackFunction() --> vkCreateDebugReportCallbackEXT_fnptr() is succedded\n");
    }
    
    return (vkResult);
    
}


//Create Vulkan Presentation Surface
VkResult getSupportedSurface(void)
{
    //local variable declaration
    VkResult vkResult = VK_SUCCESS;

    //Step2
    VkWin32SurfaceCreateInfoKHR vkWin32SurfaceCreateInfoKHR;
    memset((void*)&vkWin32SurfaceCreateInfoKHR, 0, sizeof(VkWin32SurfaceCreateInfoKHR));
    vkWin32SurfaceCreateInfoKHR.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    vkWin32SurfaceCreateInfoKHR.pNext = NULL;
    vkWin32SurfaceCreateInfoKHR.flags = 0;
    //one way
    // vkWin32SurfaceCreateInfoKHR.hinstance = (HINSTANCE)GetModuleHandle(NULL);
    //another way for 64bit
    vkWin32SurfaceCreateInfoKHR.hinstance = (HINSTANCE)GetWindowLongPtr(gWin32WindowCtx_Switcher.ghwnd, GWLP_HINSTANCE);
    vkWin32SurfaceCreateInfoKHR.hwnd = gWin32WindowCtx_Switcher.ghwnd;

    //Step3:
    vkResult = vkCreateWin32SurfaceKHR(gCtx_Switcher.vkInstance,
                                       &vkWin32SurfaceCreateInfoKHR,
                                       NULL, //Memory mamnagement function is default
                                       &gCtx_Switcher.vkSurfaceKHR);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "getSupportedSurface() --> vkCreateWin32SurfaceKHR() is failed %d\n", vkResult);
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "getSupportedSurface() --> vkCreateWin32SurfaceKHR() is succedded\n");
    }

    return vkResult;
}


VkResult getPhysicalDevice(void)
{
    //local variable declaration
    VkResult vkResult = VK_SUCCESS;
    

    //code
    vkResult = vkEnumeratePhysicalDevices(gCtx_Switcher.vkInstance,
                                          &gCtx_Switcher.physicalDeviceCount,
                                          NULL);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() --> 1st call to vkEnumeratePhysicalDevices() is failed %d\n", vkResult);
        return vkResult;
    }
    else if(gCtx_Switcher.physicalDeviceCount == 0)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() --> 1st call to vkEnumeratePhysicalDevices() resulted in zero devices\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() --> 1st call to vkEnumeratePhysicalDevices() is succedded\n");
    }

    gCtx_Switcher.vkPhysicalDevice_array = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gCtx_Switcher.physicalDeviceCount);
    //error checking to be done
    
    vkResult = vkEnumeratePhysicalDevices(gCtx_Switcher.vkInstance, 
                                          &gCtx_Switcher.physicalDeviceCount,
                                          gCtx_Switcher.vkPhysicalDevice_array);
     if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() --> 2nd call to vkEnumeratePhysicalDevices() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() --> 2nd call to vkEnumeratePhysicalDevices() is succedded\n");
    }

    VkBool32 bFound = VK_FALSE;
    for(uint32_t i = 0; i < gCtx_Switcher.physicalDeviceCount; i++)
    {
        uint32_t qCount = UINT32_MAX;
        
        //If physical device is present then it must support at least 1 queue family
        vkGetPhysicalDeviceQueueFamilyProperties(gCtx_Switcher.vkPhysicalDevice_array[i], 
                                               &qCount, 
                                               NULL);
        VkQueueFamilyProperties *vkQueueFamilyProperties_array = NULL;
        vkQueueFamilyProperties_array = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * qCount);
        //error checking to be done
        
        vkGetPhysicalDeviceQueueFamilyProperties(gCtx_Switcher.vkPhysicalDevice_array[i], 
                                               &qCount, 
                                               vkQueueFamilyProperties_array);
        
        VkBool32* isQueueSurfaceSupported_array = NULL;
        isQueueSurfaceSupported_array = (VkBool32*)malloc(sizeof(VkBool32) * qCount);
        //error checking to be done
        
        for(uint32_t j = 0; j < qCount; j++)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(gCtx_Switcher.vkPhysicalDevice_array[i], 
                                                 j,
                                                 gCtx_Switcher.vkSurfaceKHR,
                                                 &isQueueSurfaceSupported_array[j]);
        }
        
        for(uint32_t j = 0; j < qCount; j++)
        {
            if(vkQueueFamilyProperties_array[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                if(isQueueSurfaceSupported_array[j] == VK_TRUE)
                {
                    gCtx_Switcher.vkPhysicalDevice_selected = gCtx_Switcher.vkPhysicalDevice_array[i];
                    gCtx_Switcher.graphicsQueueFamilyIndex_selected = j;
                    bFound = VK_TRUE;
                    break;
                }
            }
        }
        
        if(isQueueSurfaceSupported_array)
        {
            free(isQueueSurfaceSupported_array);
            isQueueSurfaceSupported_array = NULL;
            fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() --> isQueueSurfaceSupported_array succedded to free\n");
        }
        
        if(vkQueueFamilyProperties_array)
        {
            free(vkQueueFamilyProperties_array);
            vkQueueFamilyProperties_array = NULL;
            fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() --> vkQueueFamilyProperties_array succedded to free\n");
        }
        
        if(bFound == VK_TRUE)
        {
            break;
        }
    }
    
    if(bFound == VK_TRUE)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() -->is succedded to select the required device with graphics enabled\n");
    }
    else
    {
        if(gCtx_Switcher.vkPhysicalDevice_array)
        {
            free(gCtx_Switcher.vkPhysicalDevice_array);
            gCtx_Switcher.vkPhysicalDevice_array = NULL;
            fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() --> gCtx_Switcher.vkPhysicalDevice_array succedded to free\n");
        }
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() -->is failed to select the required device with graphics enabled\n");
    }
    
    
    memset((void*)&gCtx_Switcher.vkPhysicalDeviceMemoryProperties, 0, sizeof(VkPhysicalDeviceMemoryProperties));
    vkGetPhysicalDeviceMemoryProperties(gCtx_Switcher.vkPhysicalDevice_selected, 
                                        &gCtx_Switcher.vkPhysicalDeviceMemoryProperties);
                                        
    VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures;
    memset((void*)&vkPhysicalDeviceFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
    
    vkGetPhysicalDeviceFeatures(gCtx_Switcher.vkPhysicalDevice_selected, 
                                &vkPhysicalDeviceFeatures);
                                
    if(vkPhysicalDeviceFeatures.tessellationShader)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() -->selected device supports tessellationShader\n");
    }
    else
    {
          fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() -->selected device not supports tessellationShader\n");
    }
    
    if(vkPhysicalDeviceFeatures.geometryShader)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() -->selected device supports geometryShader\n");
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevice() -->selected device not supports geometryShader\n");
    }
    
    return vkResult;
}


VkResult printVkInfo(void)
{
    //local variable declaration
    VkResult vkResult = VK_SUCCESS;
    
    //code
    fprintf(gCtx_Switcher.gpFile, "*******************VULKAN INFORMATION*********************\n");
    for(uint32_t i = 0; i < gCtx_Switcher.physicalDeviceCount; i++)    
    {
        fprintf(gCtx_Switcher.gpFile, "Infomration of Device = %d\n", i);
        
        VkPhysicalDeviceProperties vkPhysicalDeviceProperties;
        memset((void*)&vkPhysicalDeviceProperties, 0, sizeof(VkPhysicalDeviceProperties));
        
        vkGetPhysicalDeviceProperties(gCtx_Switcher.vkPhysicalDevice_array[i], &vkPhysicalDeviceProperties);
        
        uint32_t majorVersion = VK_API_VERSION_MAJOR(vkPhysicalDeviceProperties.apiVersion);
        uint32_t minorVersion = VK_API_VERSION_MINOR(vkPhysicalDeviceProperties.apiVersion);;
        uint32_t patchVersion = VK_API_VERSION_PATCH(vkPhysicalDeviceProperties.apiVersion);;
        //API Version
        fprintf(gCtx_Switcher.gpFile, "apiVersion = %d.%d.%d\n", majorVersion, minorVersion, patchVersion);
        
        //Device Name
        fprintf(gCtx_Switcher.gpFile, "DeviceName = %s\n", vkPhysicalDeviceProperties.deviceName);
        
        //DeviceType
        switch(vkPhysicalDeviceProperties.deviceType)
        {
            case(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU):
                fprintf(gCtx_Switcher.gpFile, "DeviceType = Integrated GPU(iGPU)\n");
                break;
            
            case(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU):
                fprintf(gCtx_Switcher.gpFile, "DeviceType = Discrete GPU(dGPU)\n");
                break;
                
            case(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU):
                fprintf(gCtx_Switcher.gpFile, "DeviceType = Virtual GPU(vGPU)\n");
                break;
                
            case(VK_PHYSICAL_DEVICE_TYPE_CPU):
                fprintf(gCtx_Switcher.gpFile, "DeviceType = CPU\n");
                break;    
                
            case(VK_PHYSICAL_DEVICE_TYPE_OTHER):
                fprintf(gCtx_Switcher.gpFile, "DeviceType = Other\n");
                break; 
                
            default: 
                fprintf(gCtx_Switcher.gpFile, "DeviceType = UNKNOWN\n");                
        }
        
        //Vendor Id
        fprintf(gCtx_Switcher.gpFile, "VendorId = 0x%04x\n", vkPhysicalDeviceProperties.vendorID);
        
        //DeviceId
        fprintf(gCtx_Switcher.gpFile, "DeviceId = 0x%04x\n\n", vkPhysicalDeviceProperties.deviceID);
   }
   
   fprintf(gCtx_Switcher.gpFile, "****************END OF VULKAN INFORMATION********************\n");
   
    //Freephysical device array
    if(gCtx_Switcher.vkPhysicalDevice_array)
    {
        free(gCtx_Switcher.vkPhysicalDevice_array);
        gCtx_Switcher.vkPhysicalDevice_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "printVkInfo() --> gCtx_Switcher.vkPhysicalDevice_array succedded to free\n");
    }
    
    return vkResult;
}


VkResult fillDeviceExtensionNames(void)
{
    //variable declaration
    VkResult vkResult = VK_SUCCESS;

    //Step1: Find how many Device Extension are supported by the vulkan driver of this version and keep the count in local variable
    uint32_t deviceExtensionCount = 0;

    memset((void*)gCtx_Switcher.enabledDeviceExtensionNames_array, 0,
           sizeof(gCtx_Switcher.enabledDeviceExtensionNames_array));
    gCtx_Switcher.enabledDeviceExtensionCount = 0;

    vkResult = vkEnumerateDeviceExtensionProperties(gCtx_Switcher.vkPhysicalDevice_selected,
                                                    NULL,  //Layer name: All layers
                                                    &deviceExtensionCount,
                                                    NULL); // Device Extensions Properties array: As we dont have count, so its NULL
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "fillDeviceExtensionNames() --> 1st call to vkEnumerateDeviceExtensionProperties() is failed: %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "fillDeviceExtensionNames() --> 1st call to vkEnumerateDeviceExtensionProperties() is succedded\n");
        fprintf(gCtx_Switcher.gpFile, "deviceExtensionCount is %u\n", deviceExtensionCount);
    }


    //Step2: Allocate and fill VkExtensionProperties corresponding to above count
    VkExtensionProperties* vkExtensionProperties_array = NULL;
    vkExtensionProperties_array = (VkExtensionProperties*) malloc (sizeof(VkExtensionProperties) * deviceExtensionCount);
    //Should be error checking for malloc: assert() can also be used

    vkResult = vkEnumerateDeviceExtensionProperties(gCtx_Switcher.vkPhysicalDevice_selected,
                                                    NULL, //Which layer's extenion is needed: Mention extension name: For all driver's extension use NULL
                                                    &deviceExtensionCount,
                                                    vkExtensionProperties_array); // Instance Extensions Properties array: As we have count, so it is value
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "fillDeviceExtensionNames() --> 2nd call to vkEnumerateDeviceExtensionProperties() is failed: %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "fillDeviceExtensionNames() --> 2nd call to vkEnumerateDeviceExtensionProperties() is succedded\n");
    }



    //Step3: Fill and Display a local string array of extension names obtained from vkExtensionProperties
    char** deviceExtensionNames_array = NULL;

    deviceExtensionNames_array = (char**)malloc(sizeof(char*) * deviceExtensionCount);
    //Should be error checking for malloc: assert() can also be used
    for(uint32_t i = 0; i < deviceExtensionCount; i++)
    {
        deviceExtensionNames_array[i] = (char*)malloc(sizeof(char) * strlen(vkExtensionProperties_array[i].extensionName) + 1);
        memcpy(deviceExtensionNames_array[i], vkExtensionProperties_array[i].extensionName, strlen(vkExtensionProperties_array[i].extensionName) + 1);
        fprintf(gCtx_Switcher.gpFile, "fillDeviceExtensionNames() --> Vulkan Device Extension names = %s\n", deviceExtensionNames_array[i]);
    }



   //Step4: As not required henceforth, free the vkExtensionProperties_array;
   free(vkExtensionProperties_array);
   vkExtensionProperties_array = NULL;



   // Step5: Find whether below extension names contains our required two extensions
   //VK_KHR_SWAPCHAIN_EXTENSION_NAME
   VkBool32 vulkanSwapChainExtensionFound = VK_FALSE;
  
    for(uint32_t i = 0; i < deviceExtensionCount; i++)
    {
        if(strcmp(deviceExtensionNames_array[i], VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            vulkanSwapChainExtensionFound = VK_TRUE;
            gCtx_Switcher.enabledDeviceExtensionNames_array[gCtx_Switcher.enabledDeviceExtensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        }
    }



    //Step 6:
    //As not required henceforth, free the local string array
    for(uint32_t i = 0; i < deviceExtensionCount; i++)
    {
        free(deviceExtensionNames_array[i]);
    }
    free(deviceExtensionNames_array);



    //Step7:Print whether our vulkan driver supports our required extension names or not
    if(vulkanSwapChainExtensionFound == VK_FALSE)
    {
        // return hardcoded failure
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        fprintf(gCtx_Switcher.gpFile, "fillDeviceExtensionNames() --> VK_KHR_SWAPCHAIN_EXTENSION_NAME not found\n");
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "fillDeviceExtensionNames() --> VK_KHR_SWAPCHAIN_EXTENSION_NAME found\n");
    }


    //Step8: Print only Enabled Extension Names
    for(uint32_t i = 0; i < gCtx_Switcher.enabledDeviceExtensionCount; i++)
    {
         fprintf(gCtx_Switcher.gpFile, "fillDeviceExtensionNames() --> Enabled vulkan Device extension Names = %s\n", gCtx_Switcher.enabledDeviceExtensionNames_array[i]);
    }

    return vkResult;
}



VkResult createVulkanDevice(void)
{
//variable declaration
VkResult vkResult = VK_SUCCESS;
    
//Fill Device Extensions
vkResult = gFunctionTable_Switcher.fillDeviceExtensionNames();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanDevice() --> fillDeviceExtensionNames() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanDevice() --> fillDeviceExtensionNames() is succedded\n");
    }
    
    /////Newly added code//////
    
    float QueuePriorities[] = {1.0};
    VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo;
    memset((void*)&vkDeviceQueueCreateInfo, 0, sizeof(VkDeviceQueueCreateInfo));
    vkDeviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    vkDeviceQueueCreateInfo.pNext = NULL;
    vkDeviceQueueCreateInfo.flags = 0;
    vkDeviceQueueCreateInfo.queueFamilyIndex = gCtx_Switcher.graphicsQueueFamilyIndex_selected;
    vkDeviceQueueCreateInfo.queueCount = 1;
    vkDeviceQueueCreateInfo.pQueuePriorities = QueuePriorities;
    
    //Initialize VkDeviceCreateinfo structure
    VkDeviceCreateInfo vkDeviceCreateInfo;
    memset((void*)&vkDeviceCreateInfo, 0, sizeof(VkDeviceCreateInfo));

    vkDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    vkDeviceCreateInfo.pNext = NULL;
    vkDeviceCreateInfo.flags = 0;
    vkDeviceCreateInfo.enabledExtensionCount = gCtx_Switcher.enabledDeviceExtensionCount;
    vkDeviceCreateInfo.ppEnabledExtensionNames = gCtx_Switcher.enabledDeviceExtensionNames_array;
    vkDeviceCreateInfo.enabledLayerCount = 0;  // Deprecated
    vkDeviceCreateInfo.ppEnabledLayerNames = NULL;  // Deprecated
    vkDeviceCreateInfo.pEnabledFeatures = NULL;
    vkDeviceCreateInfo.queueCreateInfoCount = 1;
    vkDeviceCreateInfo.pQueueCreateInfos = &vkDeviceQueueCreateInfo;
        
    vkResult = vkCreateDevice(gCtx_Switcher.vkPhysicalDevice_selected,
                              &vkDeviceCreateInfo,
                              NULL,
                              &gCtx_Switcher.vkDevice);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanDevice() --> vkCreateDevice() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVulkanDevice() --> vkCreateDevice() is succedded\n");
    }                     
    
    return vkResult;
}


void getDeviceQueue(void)
{
    //code
    vkGetDeviceQueue(gCtx_Switcher.vkDevice, 
                     gCtx_Switcher.graphicsQueueFamilyIndex_selected,
                     0, //0th Queue index in that family queue
                     &gCtx_Switcher.vkQueue);
    if(gCtx_Switcher.vkQueue == VK_NULL_HANDLE) //rarest possibility
    {
        fprintf(gCtx_Switcher.gpFile, "getDeviceQueue() --> vkGetDeviceQueue() returned NULL for gCtx_Switcher.vkQueue\n");
        return;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "getDeviceQueue() --> vkGetDeviceQueue() is succedded\n");
    }
}


VkResult getPhysicalDeviceSurfaceFormatAndColorSpace(void)
{
    //variable declarations
    VkResult vkResult = VK_SUCCESS;   
    uint32_t formatCount = 0;
    
    //code
    //get the count of supported SurfaceColorFormats
    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(gCtx_Switcher.vkPhysicalDevice_selected,
                                                    gCtx_Switcher.vkSurfaceKHR,
                                                    &formatCount,
                                                    NULL);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDeviceSurfaceFormatAndColorSpace() --> 1st call to vkGetPhysicalDeviceSurfaceFormatsKHR() is failed %d\n", vkResult);
        return vkResult;
    }
    else if(formatCount == 0)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDeviceSurfaceFormatAndColorSpace() --> 1st call to vkGetPhysicalDeviceSurfaceFormatsKHR() is failed as formatCount is zero:: %d\n", vkResult);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDeviceSurfaceFormatAndColorSpace() --> 1st call to vkGetPhysicalDeviceSurfaceFormatsKHR() is succedded\n");
    }
    
    fprintf(gCtx_Switcher.gpFile, "getPhysicalDeviceSurfaceFormatAndColorSpace() --> total formatCount are:: %d\n", formatCount);
    
    VkSurfaceFormatKHR* vkSurfaceFormatKHR_array = (VkSurfaceFormatKHR*) malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    //Malloc error checking
    
    //Fillig the array
    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(gCtx_Switcher.vkPhysicalDevice_selected,
                                                    gCtx_Switcher.vkSurfaceKHR,
                                                    &formatCount,
                                                    vkSurfaceFormatKHR_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDeviceSurfaceFormatAndColorSpace() --> 2nd call to vkGetPhysicalDeviceSurfaceFormatsKHR() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDeviceSurfaceFormatAndColorSpace() --> 2nd call to vkGetPhysicalDeviceSurfaceFormatsKHR() is succedded\n");
    }
    
    if(formatCount == 1 && vkSurfaceFormatKHR_array[0].format == VK_FORMAT_UNDEFINED) //bydefault it is not there
    {
        gCtx_Switcher.vkFormat_color = VK_FORMAT_R8G8B8A8_UNORM;
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDeviceSurfaceFormatAndColorSpace() --> gCtx_Switcher.vkFormat_color is VK_FORMAT_B8G8R8A8_UNORM\n");
    }
    else
    {
        gCtx_Switcher.vkFormat_color = vkSurfaceFormatKHR_array[0].format;
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDeviceSurfaceFormatAndColorSpace() --> gCtx_Switcher.vkFormat_color is %d\n", gCtx_Switcher.vkFormat_color);
    }
    
    //Decide the ColorSpace
    gCtx_Switcher.vkColorSpaceKHR = vkSurfaceFormatKHR_array[0].colorSpace;
    
    if(vkSurfaceFormatKHR_array)
    {
        free(vkSurfaceFormatKHR_array);
        vkSurfaceFormatKHR_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDeviceSurfaceFormatAndColorSpace() --> vkSurfaceFormatKHR_array is freed\n");
    }
    
    return vkResult;
}


VkResult getPhysicalDevicePresentMode(void)
{
    //variable declarations
    VkResult vkResult = VK_SUCCESS;   
    
    uint32_t presentModeCount = 0;
    
    //code
    vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(gCtx_Switcher.vkPhysicalDevice_selected,
                                                         gCtx_Switcher.vkSurfaceKHR,
                                                         &presentModeCount,
                                                         NULL);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevicePresentMode() --> 1st call to vkGetPhysicalDeviceSurfacePresentModesKHR() is failed %d\n", vkResult);
        return vkResult;
    }
    else if(presentModeCount == 0)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevicePresentMode() --> 1st call to vkGetPhysicalDeviceSurfacePresentModesKHR() is failed as formatCount is zero:: %d\n", vkResult);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevicePresentMode() --> 1st call to vkGetPhysicalDeviceSurfacePresentModesKHR() is succedded\n");
    }   

    fprintf(gCtx_Switcher.gpFile, "getPhysicalDevicePresentMode() --> total presentModeCount are:: %d\n", presentModeCount);

    VkPresentModeKHR* vkPresentModeKHR_array = (VkPresentModeKHR*) malloc(presentModeCount * sizeof(VkPresentModeKHR));
    //Malloc error checking
    
    vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(gCtx_Switcher.vkPhysicalDevice_selected,
                                                         gCtx_Switcher.vkSurfaceKHR,
                                                         &presentModeCount,
                                                         vkPresentModeKHR_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevicePresentMode() --> 2nd call to vkGetPhysicalDeviceSurfacePresentModesKHR() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevicePresentMode() --> 2nd call to vkGetPhysicalDeviceSurfacePresentModesKHR() is succedded\n");
    }
    
    //Decide Presentation mode
    for(uint32_t i = 0; i < presentModeCount; i++)
    {
        if(vkPresentModeKHR_array[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            gCtx_Switcher.vkPresentModeKHR = VK_PRESENT_MODE_MAILBOX_KHR;
            fprintf(gCtx_Switcher.gpFile, "getPhysicalDevicePresentMode() --> gCtx_Switcher.vkPresentModeKHR is VK_PRESENT_MODE_MAILBOX_KHR\n");
            break;
        }
    }
    
    if(gCtx_Switcher.vkPresentModeKHR != VK_PRESENT_MODE_MAILBOX_KHR)
    {
        gCtx_Switcher.vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevicePresentMode() --> gCtx_Switcher.vkPresentModeKHR is VK_PRESENT_MODE_FIFO_KHR\n");
    }
  
    
    if(vkPresentModeKHR_array)
    {
        free(vkPresentModeKHR_array);
        vkPresentModeKHR_array = NULL;
        fprintf(gCtx_Switcher.gpFile, "getPhysicalDevicePresentMode() --> vkPresentModeKHR_array is freed\n");
    }
    
    return vkResult;
  
}



VkResult createSwapchain(VkBool32 vsync)  // vertical sync
{
//variables
VkResult vkResult = VK_SUCCESS;
        
    //code
//Color Format and ColorSpace
vkResult = gFunctionTable_Switcher.getPhysicalDeviceSurfaceFormatAndColorSpace();
    /*Main Points
        vkGetPhysicalDeviceSurfaceFormatsKHR()
    */
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> getPhysicalDeviceSurfaceFormatAndColorSpace() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> getPhysicalDeviceSurfaceFormatAndColorSpace() is succedded\n");
    }
   
   
    //Step 2:
    VkSurfaceCapabilitiesKHR vkSurfaceCapabilitiesKHR;
    memset((void*)&vkSurfaceCapabilitiesKHR, 0, sizeof(VkSurfaceCapabilitiesKHR));
    
    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gCtx_Switcher.vkPhysicalDevice_selected,
                                                         gCtx_Switcher.vkSurfaceKHR,
                                                         &vkSurfaceCapabilitiesKHR);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> vkGetPhysicalDeviceSurfaceCapabilitiesKHR() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> vkGetPhysicalDeviceSurfaceCapabilitiesKHR() is succedded\n");
    }
   
    //Step3: Find out desired swapchain image count
    uint32_t testingNumberOfSwapchainImages = vkSurfaceCapabilitiesKHR.minImageCount + 1;
    uint32_t desiredNumbeOfSwapchainImages = 0;
    
    
    if(vkSurfaceCapabilitiesKHR.maxImageCount > 0 && vkSurfaceCapabilitiesKHR.maxImageCount < testingNumberOfSwapchainImages)
    {
        desiredNumbeOfSwapchainImages = vkSurfaceCapabilitiesKHR.maxImageCount;
    }
    else
    {
        desiredNumbeOfSwapchainImages = vkSurfaceCapabilitiesKHR.minImageCount;
    }
    
    //Step4: Choose size of swapchain image
    memset((void*)&gCtx_Switcher.vkExtent2D_swapchain, 0, sizeof(VkExtent2D));
    if(vkSurfaceCapabilitiesKHR.currentExtent.width != UINT32_MAX)
    {
        gCtx_Switcher.vkExtent2D_swapchain.width = vkSurfaceCapabilitiesKHR.currentExtent.width;
        gCtx_Switcher.vkExtent2D_swapchain.height = vkSurfaceCapabilitiesKHR.currentExtent.height;
        
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> Swapchain image width X height = %d X %d \n", gCtx_Switcher.vkExtent2D_swapchain.width, gCtx_Switcher.vkExtent2D_swapchain.height);
    }
    else
    {
        // if surface is already defined then swapchain image size must match with it
        VkExtent2D vkExtent2D;
        memset((void*)&vkExtent2D, 0 , sizeof(VkExtent2D));
        vkExtent2D.width = (uint32_t)gCtx_Switcher.winWidth;
        vkExtent2D.height = (uint32_t)gCtx_Switcher.winHeight;
        
        gCtx_Switcher.vkExtent2D_swapchain.width = glm::max(vkSurfaceCapabilitiesKHR.minImageExtent.width, glm::min(vkSurfaceCapabilitiesKHR.maxImageExtent.width, vkExtent2D.width));
        gCtx_Switcher.vkExtent2D_swapchain.height = glm::max(vkSurfaceCapabilitiesKHR.minImageExtent.height, glm::min(vkSurfaceCapabilitiesKHR.maxImageExtent.height, vkExtent2D.height));
        
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> Swapchain image width X height = %d X %d \n", gCtx_Switcher.vkExtent2D_swapchain.width, gCtx_Switcher.vkExtent2D_swapchain.height);
    }
    
    //step5: Set SwapchainImageUsageFlag
    VkImageUsageFlags vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT/*Texture, Compute, FBO*/; 
    
    //enum
    VkSurfaceTransformFlagBitsKHR vkSurfaceTransformFlagBitsKHR;
    if(vkSurfaceCapabilitiesKHR.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        vkSurfaceTransformFlagBitsKHR = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        vkSurfaceTransformFlagBitsKHR = vkSurfaceCapabilitiesKHR.currentTransform;
    }
   
    
//Step 7: Presentation mode
vkResult = gFunctionTable_Switcher.getPhysicalDevicePresentMode();
    /*Main Points
        
    */
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> getPhysicalDevicePresentMode() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> getPhysicalDevicePresentMode() is succedded\n");
    }
    
    //Step 8: Initialie vkCreateSwapchinCreateInfoStructure
    VkSwapchainCreateInfoKHR vkSwapchainCreateInfoKHR;
    memset((void*)&vkSwapchainCreateInfoKHR, 0 , sizeof(VkSwapchainCreateInfoKHR));
    vkSwapchainCreateInfoKHR.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    vkSwapchainCreateInfoKHR.pNext = NULL;
    vkSwapchainCreateInfoKHR.flags = 0;
    vkSwapchainCreateInfoKHR.surface = gCtx_Switcher.vkSurfaceKHR;
    vkSwapchainCreateInfoKHR.minImageCount = desiredNumbeOfSwapchainImages;
    vkSwapchainCreateInfoKHR.imageFormat = gCtx_Switcher.vkFormat_color;
    vkSwapchainCreateInfoKHR.imageColorSpace = gCtx_Switcher.vkColorSpaceKHR;
    vkSwapchainCreateInfoKHR.imageExtent.width = gCtx_Switcher.vkExtent2D_swapchain.width;
    vkSwapchainCreateInfoKHR.imageExtent.height = gCtx_Switcher.vkExtent2D_swapchain.height;
    vkSwapchainCreateInfoKHR.imageUsage = vkImageUsageFlags;
    vkSwapchainCreateInfoKHR.preTransform = vkSurfaceTransformFlagBitsKHR;
    vkSwapchainCreateInfoKHR.imageArrayLayers = 1;
    vkSwapchainCreateInfoKHR.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkSwapchainCreateInfoKHR.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    vkSwapchainCreateInfoKHR.presentMode = gCtx_Switcher.vkPresentModeKHR;
    vkSwapchainCreateInfoKHR.clipped = VK_TRUE;
    
    //Step9:
    vkResult = vkCreateSwapchainKHR(gCtx_Switcher.vkDevice,
                                    &vkSwapchainCreateInfoKHR,
                                    NULL,
                                    &gCtx_Switcher.vkSwapchainKHR);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> vkCreateSwapchainKHR() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createSwapchain() --> vkCreateSwapchainKHR() is succedded\n");
    }
    
    return vkResult;
}

VkBool32 HasStencilComponent(VkFormat fmt)
{
    return (fmt == VK_FORMAT_D32_SFLOAT_S8_UINT) ||
           (fmt == VK_FORMAT_D24_UNORM_S8_UINT)  ||
           (fmt == VK_FORMAT_D16_UNORM_S8_UINT);
}


VkResult createImagesAndImageViews(void)
{
//variables
    VkResult vkResult = VK_SUCCESS;
    
    //step1: Get desired SwapchainImage count
    vkResult = vkGetSwapchainImagesKHR(gCtx_Switcher.vkDevice, 
                                       gCtx_Switcher.vkSwapchainKHR,
                                       &gCtx_Switcher.swapchainImageCount,
                                       NULL);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> 1st call to vkGetSwapchainImagesKHR() is failed %d\n", vkResult);
        return vkResult;
    }
    else if(0 == gCtx_Switcher.swapchainImageCount)
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> 1st call to vkGetSwapchainImagesKHR() is failed %d\n", vkResult);
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> gives swapchainImagecount = %d\n", gCtx_Switcher.swapchainImageCount);
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkGetSwapchainImagesKHR() is succedded\n");
    }
    
    
    //step2: Allocate the swapchain Image array
    gCtx_Switcher.swapchainImage_array = (VkImage*)malloc(sizeof(VkImage) * gCtx_Switcher.swapchainImageCount);
    //malloc check to be done

    //step3: fill this array by swapchain imagesize
    vkResult = vkGetSwapchainImagesKHR(gCtx_Switcher.vkDevice, 
                                       gCtx_Switcher.vkSwapchainKHR,
                                       &gCtx_Switcher.swapchainImageCount,
                                       gCtx_Switcher.swapchainImage_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> 2nd call to vkGetSwapchainImagesKHR() is failed %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> 2nd call to vkGetSwapchainImagesKHR() is succedded\n");
    }
    
    //step4: allocate array of swapchainImageViews   
    gCtx_Switcher.swapchainImageView_array = (VkImageView*)malloc(sizeof(VkImageView) * gCtx_Switcher.swapchainImageCount);
    //malloc check to be done
    
    //step5: Initialize vkImageViewCreateInfo structure
    VkImageViewCreateInfo vkImageViewCreateInfo;
    memset((void*)&vkImageViewCreateInfo, 0, sizeof(VkImageViewCreateInfo));

    vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vkImageViewCreateInfo.pNext = NULL;
    vkImageViewCreateInfo.flags = 0;
    vkImageViewCreateInfo.format = gCtx_Switcher.vkFormat_color;
    vkImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    vkImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    vkImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    vkImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    vkImageViewCreateInfo.subresourceRange.levelCount = 1;
    vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    vkImageViewCreateInfo.subresourceRange.layerCount = 1;
    vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    
    //Step6: Fill Imageview Array by using above struct
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        vkImageViewCreateInfo.image = gCtx_Switcher.swapchainImage_array[i];
        
        vkResult = vkCreateImageView(gCtx_Switcher.vkDevice,
                                 &vkImageViewCreateInfo,
                                 NULL,
                                 &gCtx_Switcher.swapchainImageView_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkCreateImageViews() is failed for iteration %d and error code is %d\n", i, vkResult);
            return vkResult;
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkCreateImageViews() is succedded for iteration for %d\n", i);
        }
    }
    
    //for depth image
    vkResult = gFunctionTable_Switcher.GetSupportedDepthFormat();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> GetSupportedDepthFormat() is failed error code is %d\n",vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> GetSupportedDepthFormat() is succedded\n");
    }
    
    //for depth image Initialize VkImageCreateInfo
    VkImageCreateInfo vkImageCreateInfo;
    memset((void*)&vkImageCreateInfo, 0, sizeof(VkImageCreateInfo));
    vkImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    vkImageCreateInfo.pNext = NULL;
    vkImageCreateInfo.flags = 0;
    vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    vkImageCreateInfo.format = gCtx_Switcher.vkFormat_depth;
    vkImageCreateInfo.extent.width = gCtx_Switcher.winWidth;
    vkImageCreateInfo.extent.height = gCtx_Switcher.winHeight;
    vkImageCreateInfo.extent.depth = 1;
    vkImageCreateInfo.mipLevels = 1;
    vkImageCreateInfo.arrayLayers = 1;
    vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    vkImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    vkResult = vkCreateImage(gCtx_Switcher.vkDevice, &vkImageCreateInfo, NULL, &gCtx_Switcher.vkImage_depth);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkCreateImage() is failed error code is %d\n",vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkCreateImage() is succedded\n");
    }
    
    //Memory requirement for depth image
    
    VkMemoryRequirements vkMemoryRequirements;
    memset((void*)&vkMemoryRequirements, 0, sizeof(VkMemoryRequirements));
   
    vkGetImageMemoryRequirements(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImage_depth, &vkMemoryRequirements);
    
    //8" Allocate
    VkMemoryAllocateInfo vkMemoryAllocateInfo;
    memset((void*)&vkMemoryAllocateInfo , 0, sizeof(VkMemoryAllocateInfo));
    vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vkMemoryAllocateInfo.pNext = NULL; 
    vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
    //initial value before entering inloop
    vkMemoryAllocateInfo.memoryTypeIndex = 0;
   
    for(uint32_t i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
    {
        if((vkMemoryRequirements.memoryTypeBits & 1) == 1)
        {
            if(gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            {
                vkMemoryAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
        vkMemoryRequirements.memoryTypeBits >>= 1;
    }
    
    //#9 vkAllocateMemory
    vkResult = vkAllocateMemory(gCtx_Switcher.vkDevice, &vkMemoryAllocateInfo, NULL, &gCtx_Switcher.vkDeviceMemory_depth);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkAllocateMemory() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkAllocateMemory() is succedded\n");
    }
    
    //#10: Binds vulkan device memory object handle with vulkan buffer object handle
    vkResult = vkBindImageMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImage_depth, gCtx_Switcher.vkDeviceMemory_depth, 0);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkBindBufferMemory() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkBindBufferMemory() is succedded\n");
    }
    
    
    //create image view for above depth image
    memset((void*)&vkImageViewCreateInfo, 0, sizeof(VkImageViewCreateInfo));

    vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vkImageViewCreateInfo.pNext = NULL;
    vkImageViewCreateInfo.flags = 0;
    vkImageViewCreateInfo.format = gCtx_Switcher.vkFormat_depth;
    vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (gFunctionTable_Switcher.HasStencilComponent(gCtx_Switcher.vkFormat_depth))
    {
        vkImageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    vkImageViewCreateInfo.subresourceRange.levelCount = 1;
    vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    vkImageViewCreateInfo.subresourceRange.layerCount = 1;
    vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vkImageViewCreateInfo.image = gCtx_Switcher.vkImage_depth;
    
    vkResult = vkCreateImageView(gCtx_Switcher.vkDevice,
                                 &vkImageViewCreateInfo,
                                 NULL,
                                 &gCtx_Switcher.vkImageView_depth);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkCreateImageView() is failed error code is %d\n",vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createImagesAndImageViews() --> vkCreateImageView() is succedded\n");
    }
    
    
    return vkResult;
}


VkResult GetSupportedDepthFormat(void)
{
    //code
    //variables
    VkResult vkResult = VK_SUCCESS;
    VkFormat vkFormat_depth_array[] = { VK_FORMAT_D32_SFLOAT_S8_UINT,
                                       VK_FORMAT_D32_SFLOAT,
                                       VK_FORMAT_D24_UNORM_S8_UINT,
                                       VK_FORMAT_D16_UNORM_S8_UINT,
                                       VK_FORMAT_D16_UNORM };

    for(uint32_t i = 0; i < (sizeof(vkFormat_depth_array)/sizeof(vkFormat_depth_array[0])); i++)
    {
        VkFormatProperties vkFormatProperties;
        memset((void*)&vkFormatProperties, 0, sizeof(VkFormatProperties));

        vkGetPhysicalDeviceFormatProperties(gCtx_Switcher.vkPhysicalDevice_selected, vkFormat_depth_array[i], &vkFormatProperties);

        if(vkFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
           gCtx_Switcher.vkFormat_depth = vkFormat_depth_array[i];
           vkResult = VK_SUCCESS;
           break;
        }
    }
    
   return vkResult;
}


VkResult createCommandPool(void)
{
    //variables
   VkResult vkResult = VK_SUCCESS;
    
   //code
   VkCommandPoolCreateInfo vkCommandPoolCreateInfo;
   memset((void*)&vkCommandPoolCreateInfo, 0, sizeof(VkCommandPoolCreateInfo));

   vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   vkCommandPoolCreateInfo.pNext = NULL;
   vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; //such command buffers will be resetted and resatrted, and these command buffers are long lived
   vkCommandPoolCreateInfo.queueFamilyIndex = gCtx_Switcher.graphicsQueueFamilyIndex_selected;

   vkResult = vkCreateCommandPool(gCtx_Switcher.vkDevice,
                                  &vkCommandPoolCreateInfo,
                                  NULL,
                                  &gCtx_Switcher.vkCommandPool);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createCommandPool() --> vkCreateCommandPool() is failed and error code is %d\n",vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createCommandPool() --> vkCreateCommandPool() is succedded \n");
    }

   return vkResult;   
}


VkResult createCommandBuffers(void)
{
   //variables
   VkResult vkResult = VK_SUCCESS;
   
   //code
   //vkCommandBuffer allocate info structure initliazation
   VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo; 
   memset((void*)&vkCommandBufferAllocateInfo, 0, sizeof(VkCommandBufferAllocateInfo));
   
   vkCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   vkCommandBufferAllocateInfo.pNext = NULL;
   vkCommandBufferAllocateInfo.commandPool = gCtx_Switcher.vkCommandPool;
   vkCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   vkCommandBufferAllocateInfo.commandBufferCount = 1;
   
   gCtx_Switcher.vkCommandBuffer_scene0_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * gCtx_Switcher.swapchainImageCount);
   gCtx_Switcher.vkCommandBuffer_scene1_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * gCtx_Switcher.swapchainImageCount);
   gCtx_Switcher.vkCommandBuffer_scene2_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * gCtx_Switcher.swapchainImageCount);
   gCtx_Switcher.vkCommandBuffer_scene3_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * gCtx_Switcher.swapchainImageCount);

   for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        vkResult = vkAllocateCommandBuffers(gCtx_Switcher.vkDevice, &vkCommandBufferAllocateInfo, &gCtx_Switcher.vkCommandBuffer_scene0_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile, "createCommandBuffers() --> scene0 vkAllocateCommandBuffers() failed for %d iteration and error code is %d\n",i, vkResult);
            return vkResult;
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "createCommandBuffers() --> scene0 vkAllocateCommandBuffers() succeeded for iteration %d\n", i);
        }
    }

   for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        vkResult = vkAllocateCommandBuffers(gCtx_Switcher.vkDevice, &vkCommandBufferAllocateInfo, &gCtx_Switcher.vkCommandBuffer_scene1_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile, "createCommandBuffers() --> scene1 vkAllocateCommandBuffers() failed for %d iteration and error code is %d\n",i, vkResult);
            return vkResult;
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "createCommandBuffers() --> scene1 vkAllocateCommandBuffers() succeeded for iteration %d\n", i);
        }
    }

   for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        vkResult = vkAllocateCommandBuffers(gCtx_Switcher.vkDevice, &vkCommandBufferAllocateInfo, &gCtx_Switcher.vkCommandBuffer_scene2_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile, "createCommandBuffers() --> scene2 vkAllocateCommandBuffers() failed for %d iteration and error code is %d\n",i, vkResult);
            return vkResult;
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "createCommandBuffers() --> scene2 vkAllocateCommandBuffers() succeeded for iteration %d\n", i);
        }
    }

   for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        vkResult = vkAllocateCommandBuffers(gCtx_Switcher.vkDevice, &vkCommandBufferAllocateInfo, &gCtx_Switcher.vkCommandBuffer_scene3_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile, "createCommandBuffers() --> scene3 vkAllocateCommandBuffers() failed for %d iteration and error code is %d\n",i, vkResult);
            return vkResult;
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "createCommandBuffers() --> scene3 vkAllocateCommandBuffers() succeeded for iteration %d\n", i);
        }
    }

   return vkResult;
}

VkResult createVertexBuffer(void)
{
    //variable
    VkResult vkResult = VK_SUCCESS; 
    
    // CUBE
    // position
    float cubePosition[] =
    {
        // front
        // triangle one
         1.0f,  1.0f,  1.0f, // top-right of front
        -1.0f,  1.0f,  1.0f, // top-left of front
         1.0f, -1.0f,  1.0f, // bottom-right of front
        
        // triangle two
         1.0f, -1.0f,  1.0f, // bottom-right of front
        -1.0f,  1.0f,  1.0f, // top-left of front
        -1.0f, -1.0f,  1.0f, // bottom-left of front

        // right
        // triangle one
         1.0f,  1.0f, -1.0f, // top-right of right
         1.0f,  1.0f,  1.0f, // top-left of right
         1.0f, -1.0f, -1.0f, // bottom-right of right
         
        // triangle two
         1.0f, -1.0f, -1.0f, // bottom-right of right
         1.0f,  1.0f,  1.0f, // top-left of right
         1.0f, -1.0f,  1.0f, // bottom-left of right

        // back
        // triangle one
         1.0f,  1.0f, -1.0f, // top-right of back
        -1.0f,  1.0f, -1.0f, // top-left of back
         1.0f, -1.0f, -1.0f, // bottom-right of back
        
        // triangle two
         1.0f, -1.0f, -1.0f, // bottom-right of back
        -1.0f,  1.0f, -1.0f, // top-left of back
        -1.0f, -1.0f, -1.0f, // bottom-left of back

        // left
        // triangle one
        -1.0f,  1.0f,  1.0f, // top-right of left
        -1.0f,  1.0f, -1.0f, // top-left of left
        -1.0f, -1.0f,  1.0f, // bottom-right of left
        
        // triangle two
        -1.0f, -1.0f,  1.0f, // bottom-right of left
        -1.0f,  1.0f, -1.0f, // top-left of left
        -1.0f, -1.0f, -1.0f, // bottom-left of left

        // top
        // triangle one
         1.0f,  1.0f, -1.0f, // top-right of top
        -1.0f,  1.0f, -1.0f, // top-left of top
         1.0f,  1.0f,  1.0f, // bottom-right of top

        // triangle two
         1.0f,  1.0f,  1.0f, // bottom-right of top
        -1.0f,  1.0f, -1.0f, // top-left of top
        -1.0f,  1.0f,  1.0f, // bottom-left of top

        // bottom
        // triangle one
         1.0f, -1.0f,  1.0f, // top-right of bottom
        -1.0f, -1.0f,  1.0f, // top-left of bottom
         1.0f, -1.0f, -1.0f, // bottom-right of bottom
        
        // triangle two
         1.0f, -1.0f, -1.0f, // bottom-right of bottom
        -1.0f, -1.0f,  1.0f, // top-left of bottom
        -1.0f, -1.0f, -1.0f, // bottom-left of bottom
    };

    float cubeTexcoords[] =
        {
		// front
		// triangle one
		1.0f, 1.0f, // top-right of front
		0.0f, 1.0f, // top-left of front
		1.0f, 0.0f, // bottom-right of front

		// triangle two
		1.0f, 0.0f, // bottom-right of front
		0.0f, 1.0f, // top-left of front
		0.0f, 0.0f, // bottom-left of front

		// right
		// triangle one
		1.0f, 1.0f, // top-right of right
		0.0f, 1.0f, // top-left of right
		1.0f, 0.0f, // bottom-right of right
	
		// triangle two
		1.0f, 0.0f, // bottom-right of right
		0.0f, 1.0f, // top-left of right
		0.0f, 0.0f, // bottom-left of right

		// back
		// triangle one
		1.0f, 1.0f, // top-right of back
		0.0f, 1.0f, // top-left of back
		1.0f, 0.0f, // bottom-right of back

		// triangle two
		1.0f, 0.0f, // bottom-right of back
		0.0f, 1.0f, // top-left of back
		0.0f, 0.0f, // bottom-left of back

		// left
		// triangle one
		1.0f, 1.0f, // top-right of left
		0.0f, 1.0f, // top-left of left
		1.0f, 0.0f, // bottom-right of left
	
		// triangle two
		1.0f, 0.0f, // bottom-right of left
		0.0f, 1.0f, // top-left of left
		0.0f, 0.0f, // bottom-left of left

		// top
		// triangle one
		1.0f, 1.0f, // top-right of top
		0.0f, 1.0f, // top-left of top
		1.0f, 0.0f, // bottom-right of top

		// triangle two
		1.0f, 0.0f, // bottom-right of top
		0.0f, 1.0f, // top-left of top
		0.0f, 0.0f, // bottom-left of top

		// bottom
		// triangle one
		1.0f, 1.0f, // top-right of bottom
		0.0f, 1.0f, // top-left of bottom
		1.0f, 0.0f, // bottom-right of bottom

		// triangle two
		1.0f, 0.0f, // bottom-right of bottom
		0.0f, 1.0f, // top-left of bottom
		0.0f, 0.0f, // bottom-left of bottom
        };

    //VERTEX POSITION BUFFER
    //#4 memset the global strucure variable
    memset((void*)&gCtx_Switcher.vertexData_position, 0, sizeof(GlobalContext_VertexData));
    
    //#5 VkBufferCreateInfo structure filling
    VkBufferCreateInfo vkBufferCreateInfo;
    memset((void*)&vkBufferCreateInfo , 0, sizeof(VkBufferCreateInfo));
    vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vkBufferCreateInfo.pNext = NULL;
    //valid flags are used in scatterred/sparse buffer
    vkBufferCreateInfo.flags = 0;
    vkBufferCreateInfo.size = sizeof(cubePosition);
    vkBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    
    ///#6
    vkResult = vkCreateBuffer(gCtx_Switcher.vkDevice, 
                              &vkBufferCreateInfo,
                              NULL,
                              &gCtx_Switcher.vertexData_position.vkBuffer);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkCreateBuffer() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkCreateBuffer() is succedded\n");
    }
    
    VkMemoryRequirements vkMemoryRequirements;
    memset((void*)&vkMemoryRequirements, 0, sizeof(VkMemoryRequirements));
   
    vkGetBufferMemoryRequirements(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_position.vkBuffer, &vkMemoryRequirements);
    
    //8" Allocate
    VkMemoryAllocateInfo vkMemoryAllocateInfo;
    memset((void*)&vkMemoryAllocateInfo , 0, sizeof(VkMemoryAllocateInfo));
    vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vkMemoryAllocateInfo.pNext = NULL; 
    vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
    //initial value before entering inloop
    vkMemoryAllocateInfo.memoryTypeIndex = 0;
   
    for(int i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
    {
        if((vkMemoryRequirements.memoryTypeBits & 1) == 1)
        {
            if(gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            {
                vkMemoryAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
        vkMemoryRequirements.memoryTypeBits >>= 1;
    }
    
    //#9 vkAllocateMemory
    vkResult = vkAllocateMemory(gCtx_Switcher.vkDevice, &vkMemoryAllocateInfo, NULL, &gCtx_Switcher.vertexData_position.vkDeviceMemory);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkAllocateMemory() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkAllocateMemory() is succedded\n");
    }
    
    //#10: Binds vulkan device memory object handle with vulkan buffer object handle
    vkResult = vkBindBufferMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_position.vkBuffer, gCtx_Switcher.vertexData_position.vkDeviceMemory, 0);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkBindBufferMemory() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkBindBufferMemory() is succedded\n");
    }
    
    //#11
    void* data = NULL;
    vkResult = vkMapMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_position.vkDeviceMemory, 0, vkMemoryAllocateInfo.allocationSize, 0, &data);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkMapMemory() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkMapMemory() is succedded\n");
    }
   
    //#12
    memcpy(data, cubePosition, sizeof(cubePosition));
    
    vkUnmapMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_position.vkDeviceMemory);
   
   
   
   //VERTEX COLOR BUFFER 
    //#4 memset the global strucure variable
    memset((void*)&gCtx_Switcher.vertexData_texcoord, 0, sizeof(GlobalContext_VertexData));
    
    //#5 VkBufferCreateInfo structure filling
    memset((void*)&vkBufferCreateInfo , 0, sizeof(VkBufferCreateInfo));
    vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vkBufferCreateInfo.pNext = NULL;
    //valid flags are used in scatterred/sparse buffer
    vkBufferCreateInfo.flags = 0;
    vkBufferCreateInfo.size = sizeof(cubeTexcoords);
    vkBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    
    ///#6
    vkResult = vkCreateBuffer(gCtx_Switcher.vkDevice, 
                              &vkBufferCreateInfo,
                              NULL,
                              &gCtx_Switcher.vertexData_texcoord.vkBuffer);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkCreateBuffer() is failed for texcoord buffer and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkCreateBuffer() is succedded for texcoord buffer \n");
    }
    
    memset((void*)&vkMemoryRequirements, 0, sizeof(VkMemoryRequirements));
   
    vkGetBufferMemoryRequirements(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_texcoord.vkBuffer, &vkMemoryRequirements);
    
    //8" Allocate
    memset((void*)&vkMemoryAllocateInfo , 0, sizeof(VkMemoryAllocateInfo));
    vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vkMemoryAllocateInfo.pNext = NULL; 
    vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
    //initial value before entering inloop
    vkMemoryAllocateInfo.memoryTypeIndex = 0;
   
    for(int i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
    {
        if((vkMemoryRequirements.memoryTypeBits & 1) == 1)
        {
            if(gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            {
                vkMemoryAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
        vkMemoryRequirements.memoryTypeBits >>= 1;
    }
    
    //#9 vkAllocateMemory
    vkResult = vkAllocateMemory(gCtx_Switcher.vkDevice, &vkMemoryAllocateInfo, NULL, &gCtx_Switcher.vertexData_texcoord.vkDeviceMemory);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkAllocateMemory() is failed for texcoord buffer and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkAllocateMemory() is succedded for texcoord buffer\n");
    }
    
    //#10: Binds vulkan device memory object handle with vulkan buffer object handle
    vkResult = vkBindBufferMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_texcoord.vkBuffer, gCtx_Switcher.vertexData_texcoord.vkDeviceMemory, 0);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkBindBufferMemory() is failed for texcoord buffer and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkBindBufferMemory() is succedded for texcoord buffer\n");
    }
    
    //#11
    data = NULL;
    vkResult = vkMapMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_texcoord.vkDeviceMemory, 0, vkMemoryAllocateInfo.allocationSize, 0, &data);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkMapMemory() is failed for texcoord buffer and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createVertexBuffer() --> vkMapMemory() is succedded for texcoord buffer\n");
    }
   
    //#12
    memcpy(data, cubeTexcoords, sizeof(cubeTexcoords));

    vkUnmapMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vertexData_texcoord.vkDeviceMemory);

    return (vkResult);
}


VkResult createTexture(const char* textureFileName)
{
    //variable
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //step 1
    FILE* fp = NULL;
    fp = fopen(textureFileName, "rb");
    if(fp == NULL)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> fOpen() failed to open Stone.png texture file\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    
    uint8_t* imageData = NULL;
    int texture_width, texture_height, texture_channels;

    imageData = stbi_load_from_file(fp, &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);
    if(imageData == NULL || texture_width <= 0 || texture_height <= 0 || texture_channels <= 0)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> stbi_load_from_file() failed to read Stone.png texture file\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    
    /*uint64_t :: VkDeviceSize*/
    VkDeviceSize image_size = texture_width * texture_height * 4 /*RCBA*/ ;
    
    //step 2
    VkBuffer vkBuffer_stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vkDeviceMemory_stagingBuffer = VK_NULL_HANDLE;
    
    VkBufferCreateInfo vkBufferCreateInfo_stagingBuffer;
    memset((void*)&vkBufferCreateInfo_stagingBuffer, 0, sizeof(VkBufferCreateInfo));    
    vkBufferCreateInfo_stagingBuffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vkBufferCreateInfo_stagingBuffer.pNext = NULL;
    //valid flags are used in scatterred/sparse buffer
    vkBufferCreateInfo_stagingBuffer.flags = 0;
    vkBufferCreateInfo_stagingBuffer.size = image_size;
    vkBufferCreateInfo_stagingBuffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; // this buffer is source
    vkBufferCreateInfo_stagingBuffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // buffer can be used for concurrent usage, for multithreading
    
    vkResult = vkCreateBuffer(gCtx_Switcher.vkDevice, 
                              &vkBufferCreateInfo_stagingBuffer,
                              NULL,
                              &vkBuffer_stagingBuffer);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkCreateBuffer() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkCreateBuffer() is succedded\n");
    }
    
    VkMemoryRequirements vkMemoryRequirements_stagingBuffer;
    memset((void*)&vkMemoryRequirements_stagingBuffer, 0, sizeof(VkMemoryRequirements));
    
    vkGetBufferMemoryRequirements(gCtx_Switcher.vkDevice, vkBuffer_stagingBuffer, &vkMemoryRequirements_stagingBuffer);
    
    //8" Allocate
    VkMemoryAllocateInfo vkMemoryAllocateInfo_stagingBuffer;
    memset((void*)&vkMemoryAllocateInfo_stagingBuffer , 0, sizeof(VkMemoryAllocateInfo));
    
    vkMemoryAllocateInfo_stagingBuffer.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vkMemoryAllocateInfo_stagingBuffer.pNext = NULL; 
    vkMemoryAllocateInfo_stagingBuffer.allocationSize = vkMemoryRequirements_stagingBuffer.size;
    //initial value before entering inloop
    vkMemoryAllocateInfo_stagingBuffer.memoryTypeIndex = 0;
    
    for(uint32_t i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
    {
        if((vkMemoryRequirements_stagingBuffer.memoryTypeBits & 1) == 1)
        {
            if(gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) //VK_MEMORY_PROPERTY_HOST_COHERENT_BIT--> No need to manage vulkan cache mechanism for flushing and mapping as we order vulkan to maintain coherency
            {
                vkMemoryAllocateInfo_stagingBuffer.memoryTypeIndex = i;
                break;
            }
        }
        vkMemoryRequirements_stagingBuffer.memoryTypeBits >>= 1;
    }
    
    //#9 vkAllocateMemory
    vkResult = vkAllocateMemory(gCtx_Switcher.vkDevice, &vkMemoryAllocateInfo_stagingBuffer, NULL, &vkDeviceMemory_stagingBuffer);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateMemory() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateMemory() is succedded\n");
    }
    
    //#10: Binds vulkan device memory object handle with vulkan buffer object handle
    vkResult = vkBindBufferMemory(gCtx_Switcher.vkDevice, vkBuffer_stagingBuffer, vkDeviceMemory_stagingBuffer, 0);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBindBufferMemory() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBindBufferMemory() is succedded\n");
    }
    
    void* data = NULL;
    vkResult = vkMapMemory(gCtx_Switcher.vkDevice, vkDeviceMemory_stagingBuffer, 0, image_size, 0, &data);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkMapMemory() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkMapMemory() is succedded\n");
    }
    
    //#12
    memcpy(data, imageData, image_size);
    
    vkUnmapMemory(gCtx_Switcher.vkDevice, vkDeviceMemory_stagingBuffer);
    
    //As copying of image data into the staging buffer is completed, we can Free the actual image data given by stb 
    stbi_image_free(imageData);
    imageData = NULL;
    fprintf(gCtx_Switcher.gpFile, "createTexture() --> stbi_image_free() Freeing of image data is succedded\n");
    
    
    /*
    Step# 3. 
    Create "Device only visible", empty, but enough sized Image equal to size of image(image width * image Height).
    */
    
    VkImageCreateInfo vkImageCreateInfo;
    memset((void*)&vkImageCreateInfo, 0, sizeof(VkImageCreateInfo));
    vkImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    vkImageCreateInfo.pNext = NULL;
    vkImageCreateInfo.flags = 0;
    vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    vkImageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;  //SRGB can ber found in other tutorials
    vkImageCreateInfo.extent.width = texture_width;
    vkImageCreateInfo.extent.height = texture_height;
    vkImageCreateInfo.extent.depth = 1;
    vkImageCreateInfo.mipLevels = 1;
    vkImageCreateInfo.arrayLayers = 1;
    vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    vkImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vkImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vkResult = vkCreateImage(gCtx_Switcher.vkDevice, &vkImageCreateInfo, NULL, &gCtx_Switcher.vkImage_texture);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkCreateImage() is failed error code is %d\n",vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkCreateImage() is succedded\n");
        fflush(gCtx_Switcher.gpFile);
    }
    
    //Memory requirement for texture image
    VkMemoryRequirements vkMemoryRequirements_image;
    memset((void*)&vkMemoryRequirements_image, 0, sizeof(VkMemoryRequirements));
    vkGetImageMemoryRequirements(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImage_texture, &vkMemoryRequirements_image);
    
    //8" Allocate
    VkMemoryAllocateInfo vkMemoryAllocateInfo_image;
    memset((void*)&vkMemoryAllocateInfo_image , 0, sizeof(VkMemoryAllocateInfo));
    vkMemoryAllocateInfo_image.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vkMemoryAllocateInfo_image.pNext = NULL; 
    vkMemoryAllocateInfo_image.allocationSize = vkMemoryRequirements_image.size;
    //initial value before entering inloop
    vkMemoryAllocateInfo_image.memoryTypeIndex = 0;
   
    for(uint32_t i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
    {
        if((vkMemoryRequirements_image.memoryTypeBits & 1) == 1)
        {
            if(gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            {
                vkMemoryAllocateInfo_image.memoryTypeIndex = i;
                break;
            }
        }
        vkMemoryRequirements_image.memoryTypeBits >>= 1;
    }
    
    //#9 vkAllocateMemory
    vkResult = vkAllocateMemory(gCtx_Switcher.vkDevice, &vkMemoryAllocateInfo_image, NULL, &gCtx_Switcher.vkDeviceMemory_texture);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateMemory() is failed and error code is %d\n", vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateMemory() is succedded\n");
        fflush(gCtx_Switcher.gpFile);
    }
    
    //#10: Binds vulkan device memory object handle with vulkan buffer object handle
    vkResult = vkBindImageMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.vkImage_texture, gCtx_Switcher.vkDeviceMemory_texture, 0);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBindImageMemory() is failed and error code is %d\n", vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBindImageMemory() is succedded\n");
        fflush(gCtx_Switcher.gpFile);
    }
    
    /*
    Step#4. 
    Send "image transition layout" to the Vulkan / GPU, before the actual staging buffer from step 2 to empty vkImage of Step 3, using Pipeline Barrier.
    */
    
    /*
    Steps of Staging buffer, when pushing data to GPU in initialize
    
    Command pool must be allocated before these steps
    
    AllocateCmd buffer
    begin cmd buffer
    vkCMD
    end CMD nbuffer
    summbmit queue
    wait idleQueue
    Free comamnd buffer
    */
    
    //#4.1
    VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo_transition_image_layout; 
    memset((void*)&vkCommandBufferAllocateInfo_transition_image_layout, 0, sizeof(VkCommandBufferAllocateInfo));
    
    vkCommandBufferAllocateInfo_transition_image_layout.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    vkCommandBufferAllocateInfo_transition_image_layout.pNext = NULL;
    vkCommandBufferAllocateInfo_transition_image_layout.commandPool = gCtx_Switcher.vkCommandPool;
    vkCommandBufferAllocateInfo_transition_image_layout.commandBufferCount = 1;
    vkCommandBufferAllocateInfo_transition_image_layout.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   
    VkCommandBuffer vkCommandBuffer_transition_image_layout = VK_NULL_HANDLE;
    vkResult = vkAllocateCommandBuffers(gCtx_Switcher.vkDevice, &vkCommandBufferAllocateInfo_transition_image_layout, &vkCommandBuffer_transition_image_layout);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateCommandBuffers() is failed and error code is %d\n", vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateCommandBuffers() is succedded\n");
        fflush(gCtx_Switcher.gpFile);
    }

    //#4.2
    VkCommandBufferBeginInfo vkCommandBufferBeginInfo_transition_image_layout;
    memset((void*)&vkCommandBufferBeginInfo_transition_image_layout, 0, sizeof(VkCommandBufferBeginInfo));
    
    vkCommandBufferBeginInfo_transition_image_layout.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkCommandBufferBeginInfo_transition_image_layout.pNext = NULL;
    vkCommandBufferBeginInfo_transition_image_layout.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  //1. we will use only primary command buffers, 2. we are not going to use this command buffer simultaneoulsy between multipple threads
    
    vkResult = vkBeginCommandBuffer(vkCommandBuffer_transition_image_layout, &vkCommandBufferBeginInfo_transition_image_layout);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBeginCommandBuffer() is failed and error code is %d\n", vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBeginCommandBuffer() is succedded \n");
        fflush(gCtx_Switcher.gpFile);
    }

    //#4.3: Setting Barrier
    VkPipelineStageFlags vkPipelineStageFlags_source = 0;
    VkPipelineStageFlags vkPipelineStageFlags_destination = 0;
    VkImageMemoryBarrier vkImageMemoryBarrier;
    memset((void*)&vkImageMemoryBarrier, 0, sizeof(VkImageMemoryBarrier));
    vkImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImageMemoryBarrier.pNext = NULL;
    vkImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkImageMemoryBarrier.image = gCtx_Switcher.vkImage_texture;
    vkImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    vkImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    vkImageMemoryBarrier.subresourceRange.layerCount = 1;
    vkImageMemoryBarrier.subresourceRange.levelCount = 1;
    
    if(vkImageMemoryBarrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkImageMemoryBarrier.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        vkImageMemoryBarrier.srcAccessMask = 0;
        vkImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkPipelineStageFlags_source = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        vkPipelineStageFlags_destination = VK_PIPELINE_STAGE_TRANSFER_BIT;
        
        //Vulkan Pipeline
        //1. Top Stage
        //2. Drawindirect
        //3. Vertex input stage
        //4. Vertex shader stage
        //5. TSC shader stage
        //6. TSE shader stage
        //7. Geometry Shader
        //8. Fragment shader stage
        //9. Early pixel test stage(implementation dependent) (some of these post processing tests, pixel ownership, scissor, stencil, alpha, dither, blend , depth, logic op)
        //10. Late Pixel Stage(implementation dependent)
        //11. Color attachment output stage
        //12. Compute Shader stage
        //13. Transfer stage
        //14. Bottom stage
        //15. Host stage
        //16. All graphic stage
        //17. All command stage
        
        
    }
    else if(vkImageMemoryBarrier.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && vkImageMemoryBarrier.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        vkImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkPipelineStageFlags_source = VK_PIPELINE_STAGE_TRANSFER_BIT;
        vkPipelineStageFlags_destination = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> Unsupported texture layout transition()\n");
        fflush(gCtx_Switcher.gpFile);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    vkCmdPipelineBarrier(vkCommandBuffer_transition_image_layout, vkPipelineStageFlags_source, vkPipelineStageFlags_destination, 0, 0, NULL, 0, NULL, 1, &vkImageMemoryBarrier);
    
    //#4.4: End Command Buffer
    vkResult = vkEndCommandBuffer(vkCommandBuffer_transition_image_layout);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkEndCommandBuffer() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkEndCommandBuffer() is succedded\n");
    }
    
    //#4.5: Submitting Queue
    VkSubmitInfo  vkSubmitInfo_transition_image_layout;
    memset((void*)&vkSubmitInfo_transition_image_layout, 0, sizeof(VkSubmitInfo));
    
    vkSubmitInfo_transition_image_layout.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkSubmitInfo_transition_image_layout.pNext = NULL;
    vkSubmitInfo_transition_image_layout.commandBufferCount = 1;
    vkSubmitInfo_transition_image_layout.pCommandBuffers = &vkCommandBuffer_transition_image_layout;
    // As there is no need of synchrnization for waitDstStageMask and Semaphore is not needed
    
    //Now submit our work to the Queue
    vkResult = vkQueueSubmit(gCtx_Switcher.vkQueue,
                             1,
                             &vkSubmitInfo_transition_image_layout,
                             VK_NULL_HANDLE);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueSubmit() is failed errorcode = %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueSubmit() succeded\n", vkResult);
    }

    //#4.6: Waiting
    vkResult = vkQueueWaitIdle(gCtx_Switcher.vkQueue);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueWaitIdle() is failed errorcode = %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueWaitIdle() succeded\n", vkResult);
    }
    
    //#4.7: Freeing
    vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &vkCommandBuffer_transition_image_layout);
    vkCommandBuffer_transition_image_layout = VK_NULL_HANDLE;
    
    /*
    Step #5 
    Now actually copy the image data from staging buffer to the empty vkImage.
    */
    
    VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo_buffer_to_image_copy; 
    memset((void*)&vkCommandBufferAllocateInfo_buffer_to_image_copy, 0, sizeof(VkCommandBufferAllocateInfo));
    
    vkCommandBufferAllocateInfo_buffer_to_image_copy.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    vkCommandBufferAllocateInfo_buffer_to_image_copy.pNext = NULL;
    vkCommandBufferAllocateInfo_buffer_to_image_copy.commandPool = gCtx_Switcher.vkCommandPool;
    vkCommandBufferAllocateInfo_buffer_to_image_copy.commandBufferCount = 1;
    vkCommandBufferAllocateInfo_buffer_to_image_copy.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   
    VkCommandBuffer vkCommandBuffer_buffer_to_image_copy = VK_NULL_HANDLE;
    vkResult = vkAllocateCommandBuffers(gCtx_Switcher.vkDevice, &vkCommandBufferAllocateInfo_buffer_to_image_copy, &vkCommandBuffer_buffer_to_image_copy);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateCommandBuffers() is failed for buffer_to_image_copy and error code is %d \n", vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateCommandBuffers() is succedded for buffer_to_image_copy\n");
        fflush(gCtx_Switcher.gpFile);
    }

    //#5.2
    VkCommandBufferBeginInfo vkCommandBufferBeginInfo_buffer_to_image_copy;
    memset((void*)&vkCommandBufferBeginInfo_buffer_to_image_copy, 0, sizeof(VkCommandBufferBeginInfo));
    
    vkCommandBufferBeginInfo_buffer_to_image_copy.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkCommandBufferBeginInfo_buffer_to_image_copy.pNext = NULL;
    vkCommandBufferBeginInfo_buffer_to_image_copy.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  //1. we will use only primary command buffers, 2. we are not going to use this command buffer simultaneoulsy between multipple threads
    
    vkResult = vkBeginCommandBuffer(vkCommandBuffer_buffer_to_image_copy, &vkCommandBufferBeginInfo_buffer_to_image_copy);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBeginCommandBuffer() is failed for buffer_to_image_copy and error code is %d\n", vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBeginCommandBuffer() is succedded for buffer_to_image_copy \n");
        fflush(gCtx_Switcher.gpFile);
    }
    
    //5.3
    VkBufferImageCopy vkBufferImageCopy;
    memset((void*)&vkBufferImageCopy, 0, sizeof(VkBufferImageCopy));
    vkBufferImageCopy.bufferOffset = 0; 
    vkBufferImageCopy.bufferRowLength = 0;
    vkBufferImageCopy.bufferImageHeight = 0;
    vkBufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkBufferImageCopy.imageSubresource.mipLevel = 0;
    vkBufferImageCopy.imageSubresource.baseArrayLayer = 0;
    vkBufferImageCopy.imageSubresource.layerCount = 1;
    // vkBufferImageCopy.imageSubresource.levelCount = 1;
    vkBufferImageCopy.imageOffset.x = 0;
    vkBufferImageCopy.imageOffset.y = 0;
    vkBufferImageCopy.imageOffset.z = 0;
    vkBufferImageCopy.imageExtent.width = texture_width;
    vkBufferImageCopy.imageExtent.height = texture_height;
    vkBufferImageCopy.imageExtent.depth = 1;
    
    vkCmdCopyBufferToImage(vkCommandBuffer_buffer_to_image_copy, 
                           vkBuffer_stagingBuffer, 
                           gCtx_Switcher.vkImage_texture, 
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                           1, 
                           &vkBufferImageCopy);
    
    //#5.4: End Command Buffer
    vkResult = vkEndCommandBuffer(vkCommandBuffer_buffer_to_image_copy);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkEndCommandBuffer() is failed for buffer_to_image_copy and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkEndCommandBuffer() is succedded for buffer_to_image_copy\n");
    }
    
    //#5.5: Submitting Queue
    VkSubmitInfo  vkSubmitInfo_buffer_to_image_copy;
    memset((void*)&vkSubmitInfo_buffer_to_image_copy, 0, sizeof(VkSubmitInfo));
    
    vkSubmitInfo_buffer_to_image_copy.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkSubmitInfo_buffer_to_image_copy.pNext = NULL;
    vkSubmitInfo_buffer_to_image_copy.commandBufferCount = 1;
    vkSubmitInfo_buffer_to_image_copy.pCommandBuffers = &vkCommandBuffer_buffer_to_image_copy;
    // As there is no need of synchrnization for waitDstStageMask and Semaphore is not needed
    
    //Now submit our work to the Queue
    vkResult = vkQueueSubmit(gCtx_Switcher.vkQueue,
                             1,
                             &vkSubmitInfo_buffer_to_image_copy,
                             VK_NULL_HANDLE);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueSubmit() is failed for buffer_to_image_copy errorcode = %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueSubmit() is succedded for buffer_to_image_copy");
    }

    //#5.6: Waiting
    vkResult = vkQueueWaitIdle(gCtx_Switcher.vkQueue);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueWaitIdle() is failed for buffer_to_image_copy errorcode = %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueWaitIdle() succeded for buffer_to_image_copy\n");
    }
    
    //#5.7: Freeing
    vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &vkCommandBuffer_buffer_to_image_copy);
    vkCommandBuffer_buffer_to_image_copy = VK_NULL_HANDLE;
    
    
    /*
    Step 6. 
    Now again do image layout transition similar to the step 4, for the corrext reading/writing of object data by shaders.
    */
    
    //#6.1
    memset((void*)&vkCommandBufferAllocateInfo_transition_image_layout, 0, sizeof(VkCommandBufferAllocateInfo));
    
    vkCommandBufferAllocateInfo_transition_image_layout.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    vkCommandBufferAllocateInfo_transition_image_layout.pNext = NULL;
    vkCommandBufferAllocateInfo_transition_image_layout.commandPool = gCtx_Switcher.vkCommandPool;
    vkCommandBufferAllocateInfo_transition_image_layout.commandBufferCount = 1;
    vkCommandBufferAllocateInfo_transition_image_layout.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   
    vkCommandBuffer_transition_image_layout = VK_NULL_HANDLE;
    vkResult = vkAllocateCommandBuffers(gCtx_Switcher.vkDevice, &vkCommandBufferAllocateInfo_transition_image_layout, &vkCommandBuffer_transition_image_layout);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateCommandBuffers() is failed and error code is %d\n", vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkAllocateCommandBuffers() is succedded\n");
        fflush(gCtx_Switcher.gpFile);
    }

    //#6.2
    memset((void*)&vkCommandBufferBeginInfo_transition_image_layout, 0, sizeof(VkCommandBufferBeginInfo));
    
    vkCommandBufferBeginInfo_transition_image_layout.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkCommandBufferBeginInfo_transition_image_layout.pNext = NULL;
    vkCommandBufferBeginInfo_transition_image_layout.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  //1. we will use only primary command buffers, 2. we are not going to use this command buffer simultaneoulsy between multipple threads
    
    vkResult = vkBeginCommandBuffer(vkCommandBuffer_transition_image_layout, &vkCommandBufferBeginInfo_transition_image_layout);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBeginCommandBuffer() is failed and error code is %d\n", vkResult);
        fflush(gCtx_Switcher.gpFile);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkBeginCommandBuffer() is succedded \n");
        fflush(gCtx_Switcher.gpFile);
    }

    //#6.3: Setting Barrier
    vkPipelineStageFlags_source = 0;
    vkPipelineStageFlags_destination = 0;

    memset((void*)&vkImageMemoryBarrier, 0, sizeof(VkImageMemoryBarrier));
    vkImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkImageMemoryBarrier.pNext = NULL;
    vkImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkImageMemoryBarrier.image = gCtx_Switcher.vkImage_texture;
    vkImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    vkImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    vkImageMemoryBarrier.subresourceRange.layerCount = 1;
    vkImageMemoryBarrier.subresourceRange.levelCount = 1;
    
    if(vkImageMemoryBarrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkImageMemoryBarrier.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        vkImageMemoryBarrier.srcAccessMask = 0;
        vkImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkPipelineStageFlags_source = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        vkPipelineStageFlags_destination = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if(vkImageMemoryBarrier.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && vkImageMemoryBarrier.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        vkImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkPipelineStageFlags_source = VK_PIPELINE_STAGE_TRANSFER_BIT;
        vkPipelineStageFlags_destination = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> Unsupported texture layout transition for 2nd time in step 6\n");
        fflush(gCtx_Switcher.gpFile);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    vkCmdPipelineBarrier(vkCommandBuffer_transition_image_layout, vkPipelineStageFlags_source, vkPipelineStageFlags_destination, 0, 0, NULL, 0, NULL, 1, &vkImageMemoryBarrier);
    
    //#6.4: End Command Buffer
    vkResult = vkEndCommandBuffer(vkCommandBuffer_transition_image_layout);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkEndCommandBuffer() is failed for 2nd time in step 6 and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkEndCommandBuffer() is succedded\n");
    }
    
    //#6.5: Submitting Queue
    memset((void*)&vkSubmitInfo_transition_image_layout, 0, sizeof(VkSubmitInfo));
    
    vkSubmitInfo_transition_image_layout.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkSubmitInfo_transition_image_layout.pNext = NULL;
    vkSubmitInfo_transition_image_layout.commandBufferCount = 1;
    vkSubmitInfo_transition_image_layout.pCommandBuffers = &vkCommandBuffer_transition_image_layout;
    // As there is no need of synchrnization for waitDstStageMask and Semaphore is not needed
    
    //Now submit our work to the Queue
    vkResult = vkQueueSubmit(gCtx_Switcher.vkQueue,
                             1,
                             &vkSubmitInfo_transition_image_layout,
                             VK_NULL_HANDLE);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueSubmit() is failed  for 2nd time in step 6 errorcode = %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueSubmit() succeded  for 2nd time in step 6 \n", vkResult);
    }

    //#6.6: Waiting
    vkResult = vkQueueWaitIdle(gCtx_Switcher.vkQueue);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueWaitIdle() is failed  for 2nd time in step 6 errorcode = %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkQueueWaitIdle() succeded for 2nd time in step 6 \n", vkResult);
    }
    
    //#6.7: Freeing
    vkFreeCommandBuffers(gCtx_Switcher.vkDevice, gCtx_Switcher.vkCommandPool, 1, &vkCommandBuffer_transition_image_layout);
    vkCommandBuffer_transition_image_layout = VK_NULL_HANDLE;
    
    /*
    Step #7. 
    Now staging buffer is not needed, hence release its memory and itself
    */

    if(vkBuffer_stagingBuffer)
    {
        vkDestroyBuffer(gCtx_Switcher.vkDevice, vkBuffer_stagingBuffer, NULL);
        vkBuffer_stagingBuffer = VK_NULL_HANDLE;
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkDestroyBuffer() is done for vkBuffer_stagingBuffer of setp 7\n");
    }
    
    if(vkDeviceMemory_stagingBuffer)
    {
       vkFreeMemory(gCtx_Switcher.vkDevice, vkDeviceMemory_stagingBuffer, NULL);
       vkDeviceMemory_stagingBuffer = VK_NULL_HANDLE;
       fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkFreeMemory() is done for vkBuffer_stagingBuffer of setp 7\n");
    }
    
    /* Step8. 
       Create imageview of above image
    */
    // Initialize vkImageViewCreateInfo structure
    VkImageViewCreateInfo vkImageViewCreateInfo;
    //create image view for above depth image
    memset((void*)&vkImageViewCreateInfo, 0, sizeof(VkImageViewCreateInfo));

    vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vkImageViewCreateInfo.pNext = NULL;
    vkImageViewCreateInfo.flags = 0;
    vkImageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    vkImageViewCreateInfo.subresourceRange.levelCount = 1;
    vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    vkImageViewCreateInfo.subresourceRange.layerCount = 1;
    vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vkImageViewCreateInfo.image = gCtx_Switcher.vkImage_texture;
    
    vkResult = vkCreateImageView(gCtx_Switcher.vkDevice,
                                 &vkImageViewCreateInfo,
                                 NULL,
                                 &gCtx_Switcher.vkImageView_texture);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkCreateImageView() is failed error code is %d\n",vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkCreateImageView() is succedded\n");
    }
    
    
    
    /*Step 9. 
      Create texture sampler of above image
     */
     
    VkSamplerCreateInfo vkSamplerCreateInfo;
    memset((void*)&vkSamplerCreateInfo, 0, sizeof(VkSamplerCreateInfo));
    vkSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    vkSamplerCreateInfo.pNext = NULL;
    vkSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    vkSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    vkSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkSamplerCreateInfo.anisotropyEnable = VK_FALSE;
    vkSamplerCreateInfo.maxAnisotropy = 16;
    vkSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    vkSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    vkSamplerCreateInfo.compareEnable = VK_FALSE;
    vkSamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    
    vkResult = vkCreateSampler(gCtx_Switcher.vkDevice, &vkSamplerCreateInfo, NULL, &gCtx_Switcher.vkSampler_texture);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkCreateSampler() is failed error code is %d\n",vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createTexture() --> vkCreateSampler() is succedded\n");
    }
     
    return vkResult; 
}



static VkResult CreateUniformBufferForScene(GlobalContext_UniformData* uniformData)
{
    VkResult vkResult;

    if (uniformData == NULL)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    memset((void*)uniformData, 0, sizeof(GlobalContext_UniformData));

    VkBufferCreateInfo vkBufferCreateInfo;
    memset((void*)&vkBufferCreateInfo, 0, sizeof(VkBufferCreateInfo));
    vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vkBufferCreateInfo.pNext = NULL;
    vkBufferCreateInfo.flags = 0;
    vkBufferCreateInfo.size = sizeof(GlobalContext_MyUniformData);
    vkBufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    vkResult = vkCreateBuffer(gCtx_Switcher.vkDevice,
                              &vkBufferCreateInfo,
                              NULL,
                              &uniformData->vkBuffer);
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    VkMemoryRequirements vkMemoryRequirements;
    memset((void*)&vkMemoryRequirements, 0, sizeof(VkMemoryRequirements));
    vkGetBufferMemoryRequirements(gCtx_Switcher.vkDevice, uniformData->vkBuffer, &vkMemoryRequirements);

    VkMemoryAllocateInfo vkMemoryAllocateInfo;
    memset((void*)&vkMemoryAllocateInfo, 0, sizeof(VkMemoryAllocateInfo));
    vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vkMemoryAllocateInfo.pNext = NULL;
    vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
    vkMemoryAllocateInfo.memoryTypeIndex = 0;

    for (uint32_t i = 0; i < gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((vkMemoryRequirements.memoryTypeBits & 1U) == 1U)
        {
            if (gCtx_Switcher.vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            {
                vkMemoryAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
        vkMemoryRequirements.memoryTypeBits >>= 1;
    }

    vkResult = vkAllocateMemory(gCtx_Switcher.vkDevice,
                                 &vkMemoryAllocateInfo,
                                 NULL,
                                 &uniformData->vkDeviceMemory);
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    vkResult = vkBindBufferMemory(gCtx_Switcher.vkDevice,
                                  uniformData->vkBuffer,
                                  uniformData->vkDeviceMemory,
                                  0);
    return vkResult;
}

VkResult createUniformBuffer(void)
{
    VkResult vkResult = CreateUniformBufferForScene(&gCtx_Switcher.uniformData_scene0);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> failed to create scene0 uniform buffer %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> created scene0 uniform buffer\n");
    }

    vkResult = CreateUniformBufferForScene(&gCtx_Switcher.uniformData_scene1);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> failed to create scene1 uniform buffer %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> created scene1 uniform buffer\n");
    }

    vkResult = CreateUniformBufferForScene(&gCtx_Switcher.uniformData_scene2);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> failed to create scene2 uniform buffer %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> created scene2 uniform buffer\n");
    }

    vkResult = CreateUniformBufferForScene(&gCtx_Switcher.uniformData_scene3);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> failed to create scene3 uniform buffer %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> created scene3 uniform buffer\n");
    }

    vkResult = gFunctionTable_Switcher.updateUniformBuffer();
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> updateUniformBuffer() is failed and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createUniformBuffer() --> updateUniformBuffer() is succedded\n");
    }

    return vkResult;
}

VkResult updateUniformBuffer(void)
{
    VkResult vkResult = Scene0_UpdateUniformBuffer();
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    vkResult = Scene1_UpdateUniformBuffer();
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    vkResult = Scene2_UpdateUniformBuffer();
    if (vkResult != VK_SUCCESS)
    {
        return vkResult;
    }

    return Scene3_UpdateUniformBuffer();
}

VkResult createShaders(void)
{
    //variable
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //for vertex shader
    const char* szFileName = "Shader_Scene0.vert.spv";
    FILE* fp = NULL;
    size_t size;
    
    //#6a
    fp = fopen(szFileName, "rb"); //open for reading in binary format
    if(fp == NULL)
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> fopen() failed to open shader_Scene0.vert.spv\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> fopen() succedded to open shader_Scene0.vert.spv\n");
    }
    
    //#6b
    fseek(fp, 0L, SEEK_END);
    
    //#6c
    size = ftell(fp);
    if(size == 0)
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> ftell() failed to provide size of shader_Scene0.vert.spv\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
   
    //#6d
    fseek(fp, 0L, SEEK_SET); //reset to start
    
    //#6e
    char* shaderData = (char*)malloc(sizeof(char) * size);
    size_t retVal = fread(shaderData, size, 1, fp);
    if(retVal != 1)
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> fread() failed to read shader_Scene0.vert.spv\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> fread() succedded to read shader_Scene0.vert.spv\n");
    }
    
    //#6f
    fclose(fp);
    
    //#7
    VkShaderModuleCreateInfo vkShaderModuleCreateInfo;
    memset((void*)&vkShaderModuleCreateInfo, 0, sizeof(VkShaderModuleCreateInfo));
    vkShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vkShaderModuleCreateInfo.pNext = NULL;
    vkShaderModuleCreateInfo.flags = 0; // reserved, hence must be zero
    vkShaderModuleCreateInfo.codeSize = size;
    vkShaderModuleCreateInfo.pCode = (uint32_t*)shaderData;

    //8
    vkResult = vkCreateShaderModule(gCtx_Switcher.vkDevice, &vkShaderModuleCreateInfo, NULL, &gCtx_Switcher.vkShaderModule_vertex_shader);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> vkCreateShaderModule() is failed & error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> vkCreateShaderModule() is succedded\n");
    }
    
    //#9
    if(shaderData)
    {
        free(shaderData);
        shaderData = NULL;
    }
    
    fprintf(gCtx_Switcher.gpFile, "createShaders() --> vertex Shader module successfully created\n");
    
    
    //for fragment shader
    szFileName = "Shader_Scene0.frag.spv";
    fp = NULL;
    size = 0;
    
    //#6a
    fp = fopen(szFileName, "rb"); //open for reading in binary format
    if(fp == NULL)
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> fopen() failed to open shader_Scene0.frag.spv\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> fopen() succedded to open shader_Scene0.frag.spv\n");
    }
    
    //#6b
    fseek(fp, 0L, SEEK_END);
    
    //#6c
    size = ftell(fp);
    if(size == 0)
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> ftell() failed to provide size of shader_Scene0.frag.spv\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
   
    //#6d
    fseek(fp, 0L, SEEK_SET); //reset to start
    
    //#6e
    shaderData = (char*)malloc(sizeof(char) * size);
    retVal = fread(shaderData, size, 1, fp);
    if(retVal != 1)
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> fread() failed to read shader_Scene0.frag.spv\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> fread() succedded to read shader_Scene0.frag.spv\n");
    }
    
    //#6f
    fclose(fp);
    
    //#7
    memset((void*)&vkShaderModuleCreateInfo, 0, sizeof(VkShaderModuleCreateInfo));
    vkShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vkShaderModuleCreateInfo.pNext = NULL;
    vkShaderModuleCreateInfo.flags = 0; // reserved, hence must be zero
    vkShaderModuleCreateInfo.codeSize = size;
    vkShaderModuleCreateInfo.pCode = (uint32_t*)shaderData;

    //8
    vkResult = vkCreateShaderModule(gCtx_Switcher.vkDevice, &vkShaderModuleCreateInfo, NULL, &gCtx_Switcher.vkShaderModule_fragment_shader);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> vkCreateShaderModule() is failed & error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createShaders() --> vkCreateShaderModule() is succedded\n");
    }
    
    //#9
    if(shaderData)
    {
        free(shaderData);
        shaderData = NULL;
    }
    
    fprintf(gCtx_Switcher.gpFile, "createShaders() --> fragment Shader module successfully created\n");
     
    /* ===== Scene 1 shader modules ===== */
    vkResult = LoadShaderModuleFromFile("Shader_Scene1.vert.spv", &gShaderModule_vertex_scene1);
    if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = LoadShaderModuleFromFile("Shader_Scene1.frag.spv", &gShaderModule_fragment_scene1);
    if (vkResult != VK_SUCCESS) return vkResult;
    fprintf(gCtx_Switcher.gpFile, "createShaders() --> Scene1 shader modules created\n");

    /* ===== Scene 2 shader modules ===== */
    vkResult = LoadShaderModuleFromFile("Shader_Scene2.vert.spv", &gShaderModule_vertex_scene2);
    if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = LoadShaderModuleFromFile("Shader_Scene2.frag.spv", &gShaderModule_fragment_scene2);
    if (vkResult != VK_SUCCESS) return vkResult;
    fprintf(gCtx_Switcher.gpFile, "createShaders() --> Scene2 shader modules created\n");

    /* ===== Scene 3 shader modules ===== */
    vkResult = LoadShaderModuleFromFile("Shader_Scene3.vert.spv", &gShaderModule_vertex_scene3);
    if (vkResult != VK_SUCCESS) return vkResult;
    vkResult = LoadShaderModuleFromFile("Shader_Scene3.frag.spv", &gShaderModule_fragment_scene3);
    if (vkResult != VK_SUCCESS) return vkResult;
    fprintf(gCtx_Switcher.gpFile, "createShaders() --> Scene3 shader modules created\n");

    return (vkResult);
}

VkResult createDescriptorSetLayout(void)
{
    //variable
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //Descriptor set binding
    //0 -> uniform, 1 -> scene texture/cubemap, 2 -> overlay (Scene 1)
    VkDescriptorSetLayoutBinding vkDescriptorSetLayoutBinding_array[3];
    memset((void*)vkDescriptorSetLayoutBinding_array, 0, sizeof(VkDescriptorSetLayoutBinding) * _ARRAYSIZE(vkDescriptorSetLayoutBinding_array));
    
    // for MVP Uniform
    vkDescriptorSetLayoutBinding_array[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vkDescriptorSetLayoutBinding_array[0].binding = 0;  // this zero related with zero binding in vertex shader
    vkDescriptorSetLayoutBinding_array[0].descriptorCount = 1;
    vkDescriptorSetLayoutBinding_array[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;  //shader stage
    vkDescriptorSetLayoutBinding_array[0].pImmutableSamplers = NULL;
    
    // for texture image and sampler
    vkDescriptorSetLayoutBinding_array[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    vkDescriptorSetLayoutBinding_array[1].binding = 1;  // this one related with 1 binding in fragment shader
    vkDescriptorSetLayoutBinding_array[1].descriptorCount = 1;
    vkDescriptorSetLayoutBinding_array[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;  //shader stage
    vkDescriptorSetLayoutBinding_array[1].pImmutableSamplers = NULL;

    /* binding 2: overlay for Scene 1 */
    vkDescriptorSetLayoutBinding_array[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    vkDescriptorSetLayoutBinding_array[2].binding = 2;
    vkDescriptorSetLayoutBinding_array[2].descriptorCount = 1;
    vkDescriptorSetLayoutBinding_array[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    vkDescriptorSetLayoutBinding_array[2].pImmutableSamplers = NULL;

    
    
    VkDescriptorSetLayoutCreateInfo vkDescriptorSetLayoutCreateInfo;
    memset((void*)&vkDescriptorSetLayoutCreateInfo, 0, sizeof(VkDescriptorSetLayoutCreateInfo));
    vkDescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    vkDescriptorSetLayoutCreateInfo.pNext = NULL;
    vkDescriptorSetLayoutCreateInfo.flags = 0; //reserved
    vkDescriptorSetLayoutCreateInfo.bindingCount = _ARRAYSIZE(vkDescriptorSetLayoutBinding_array); // one DescriptorSet available
    vkDescriptorSetLayoutCreateInfo.pBindings = vkDescriptorSetLayoutBinding_array;

    vkResult = vkCreateDescriptorSetLayout(gCtx_Switcher.vkDevice, &vkDescriptorSetLayoutCreateInfo, NULL, &gCtx_Switcher.vkDescriptorSetLayout);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createDescriptorSetLayout() --> vkCreateDescriptorSetLayour() is failed & error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createDescriptorSetLayout() --> vkCreateDescriptorSetLayour() is succedded\n");
    }
    
    return (vkResult);
}

VkResult createPipelineLayout(void)
{
    //variable
    VkResult vkResult = VK_SUCCESS;
    
    //code
    VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo;
    memset((void*)&vkPipelineLayoutCreateInfo, 0, sizeof(VkPipelineLayoutCreateInfo));
    vkPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vkPipelineLayoutCreateInfo.pNext = NULL;
    vkPipelineLayoutCreateInfo.flags = 0; //reserved
    vkPipelineLayoutCreateInfo.setLayoutCount = 1;
    vkPipelineLayoutCreateInfo.pSetLayouts = &gCtx_Switcher.vkDescriptorSetLayout;
    vkPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    vkPipelineLayoutCreateInfo.pPushConstantRanges = NULL;

    vkResult = vkCreatePipelineLayout(gCtx_Switcher.vkDevice, &vkPipelineLayoutCreateInfo, NULL, &gCtx_Switcher.vkPipelineLayout);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createPipelineLayout() --> vkCreatePipelineLayout() is failed & error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createPipelineLayout() --> vkCreatePipelineLayout() is succedded\n");
    }
    
    return (vkResult);
}

VkResult createDescriptorPool(void)
{
    //variable
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //before creating actual descriptor pool, vulkan expects descriptor pool size
    //0th index --> uniform, 1st index --> combined samplers (scene0 + scene1 sky + scene1 overlay)
    VkDescriptorPoolSize vkDescriptorPoolSize_array[2];
    memset((void*)vkDescriptorPoolSize_array, 0, sizeof(VkDescriptorPoolSize) * _ARRAYSIZE(vkDescriptorPoolSize_array));
    
    //for MVP uniform
    vkDescriptorPoolSize_array[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vkDescriptorPoolSize_array[0].descriptorCount = 8;

    //for Texture and sampler uniform
    vkDescriptorPoolSize_array[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    vkDescriptorPoolSize_array[1].descriptorCount = 16;
    
    
    //Create the pool
    VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo;
    memset((void*)&vkDescriptorPoolCreateInfo, 0, sizeof(VkDescriptorPoolCreateInfo));
    vkDescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    vkDescriptorPoolCreateInfo.pNext = NULL;
    vkDescriptorPoolCreateInfo.flags = 0;
    vkDescriptorPoolCreateInfo.poolSizeCount = _ARRAYSIZE(vkDescriptorPoolSize_array);
    vkDescriptorPoolCreateInfo.pPoolSizes = vkDescriptorPoolSize_array;
    vkDescriptorPoolCreateInfo.maxSets = 8;

    vkResult = vkCreateDescriptorPool(gCtx_Switcher.vkDevice, &vkDescriptorPoolCreateInfo, NULL, &gCtx_Switcher.vkDescriptorPool);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createDescriptorPool() --> vkCreateDescriptorPool() is failed & error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createDescriptorPool() --> vkCreateDescriptorPool() is succedded\n");
    }
    
    return (vkResult);
}


VkResult createDescriptorSet(void)
{
    //variable
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //Initialize descriptorset allocation info
    VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo;
    memset((void*)&vkDescriptorSetAllocateInfo, 0, sizeof(VkDescriptorSetAllocateInfo));
    vkDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    vkDescriptorSetAllocateInfo.pNext = NULL;
    vkDescriptorSetAllocateInfo.descriptorPool = gCtx_Switcher.vkDescriptorPool;
    vkDescriptorSetAllocateInfo.descriptorSetCount = 4;
    VkDescriptorSetLayout vkDescriptorSetLayout_array[4] =
    {
        gCtx_Switcher.vkDescriptorSetLayout,
        gCtx_Switcher.vkDescriptorSetLayout,
        gCtx_Switcher.vkDescriptorSetLayout,
        gCtx_Switcher.vkDescriptorSetLayout
    };
    vkDescriptorSetAllocateInfo.pSetLayouts = vkDescriptorSetLayout_array;

    VkDescriptorSet vkDescriptorSet_array[4];
    vkResult = vkAllocateDescriptorSets(gCtx_Switcher.vkDevice, &vkDescriptorSetAllocateInfo, vkDescriptorSet_array);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createDescriptorSet() --> vkAllocateDescriptorSets() is failed & error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createDescriptorSet() --> vkAllocateDescriptorSets() is succedded\n");
    }

    gCtx_Switcher.vkDescriptorSet_scene0 = vkDescriptorSet_array[0];
    gCtx_Switcher.vkDescriptorSet_scene1 = vkDescriptorSet_array[1];
    gCtx_Switcher.vkDescriptorSet_scene2 = vkDescriptorSet_array[2];
    gCtx_Switcher.vkDescriptorSet_scene3 = vkDescriptorSet_array[3];

    VkDescriptorBufferInfo vkDescriptorBufferInfo_scene0;
    memset((void*)&vkDescriptorBufferInfo_scene0, 0, sizeof(VkDescriptorBufferInfo));
    vkDescriptorBufferInfo_scene0.buffer = gCtx_Switcher.uniformData_scene0.vkBuffer;
    vkDescriptorBufferInfo_scene0.offset = 0;
    vkDescriptorBufferInfo_scene0.range = sizeof(GlobalContext_MyUniformData);

    VkDescriptorBufferInfo vkDescriptorBufferInfo_scene1;
    memset((void*)&vkDescriptorBufferInfo_scene1, 0, sizeof(VkDescriptorBufferInfo));
    vkDescriptorBufferInfo_scene1.buffer = gCtx_Switcher.uniformData_scene1.vkBuffer;
    vkDescriptorBufferInfo_scene1.offset = 0;
    vkDescriptorBufferInfo_scene1.range = sizeof(GlobalContext_MyUniformData);

    VkDescriptorBufferInfo vkDescriptorBufferInfo_scene2;
    memset((void*)&vkDescriptorBufferInfo_scene2, 0, sizeof(VkDescriptorBufferInfo));
    vkDescriptorBufferInfo_scene2.buffer = gCtx_Switcher.uniformData_scene2.vkBuffer;
    vkDescriptorBufferInfo_scene2.offset = 0;
    vkDescriptorBufferInfo_scene2.range = sizeof(GlobalContext_MyUniformData);

    VkDescriptorBufferInfo vkDescriptorBufferInfo_scene3;
    memset((void*)&vkDescriptorBufferInfo_scene3, 0, sizeof(VkDescriptorBufferInfo));
    vkDescriptorBufferInfo_scene3.buffer = gCtx_Switcher.uniformData_scene3.vkBuffer;
    vkDescriptorBufferInfo_scene3.offset = 0;
    vkDescriptorBufferInfo_scene3.range = sizeof(GlobalContext_MyUniformData);

    VkDescriptorImageInfo scene0Texture;
    memset((void*)&scene0Texture, 0, sizeof(scene0Texture));
    scene0Texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    scene0Texture.imageView = gCtx_Switcher.vkImageView_texture;
    scene0Texture.sampler = gCtx_Switcher.vkSampler_texture;

    VkDescriptorImageInfo scene1Sky;
    Scene1_GetSkyDescriptor(&scene1Sky);

    VkDescriptorImageInfo scene1Overlay;
    Scene1_GetOverlayDescriptor(&scene1Overlay);

    VkDescriptorImageInfo scene2Texture;
    memset(&scene2Texture, 0, sizeof(scene2Texture));
    scene2Texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    scene2Texture.imageView   = gCtx_Switcher.vkImageView_texture_scene2;
    scene2Texture.sampler     = gCtx_Switcher.vkSampler_texture_scene2;

    VkDescriptorImageInfo scene3Texture;
    memset(&scene3Texture, 0, sizeof(scene3Texture));
    scene3Texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    scene3Texture.imageView   = gCtx_Switcher.vkImageView_texture_scene3;
    scene3Texture.sampler     = gCtx_Switcher.vkSampler_texture_scene3;

    VkWriteDescriptorSet writes[9];
    memset((void*)writes, 0, sizeof(writes));

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = gCtx_Switcher.vkDescriptorSet_scene0;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &vkDescriptorBufferInfo_scene0;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = gCtx_Switcher.vkDescriptorSet_scene0;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &scene0Texture;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = gCtx_Switcher.vkDescriptorSet_scene1;
    writes[2].dstBinding = 0;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &vkDescriptorBufferInfo_scene1;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = gCtx_Switcher.vkDescriptorSet_scene1;
    writes[3].dstBinding = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo = &scene1Sky;

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = gCtx_Switcher.vkDescriptorSet_scene1;
    writes[4].dstBinding = 2;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].descriptorCount = 1;
    writes[4].pImageInfo = &scene1Overlay;

    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = gCtx_Switcher.vkDescriptorSet_scene2;
    writes[5].dstBinding = 0;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[5].descriptorCount = 1;
    writes[5].pBufferInfo = &vkDescriptorBufferInfo_scene2;

    writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[6].dstSet = gCtx_Switcher.vkDescriptorSet_scene2;
    writes[6].dstBinding = 1;
    writes[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[6].descriptorCount = 1;
    writes[6].pImageInfo = &scene2Texture;

    writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[7].dstSet = gCtx_Switcher.vkDescriptorSet_scene3;
    writes[7].dstBinding = 0;
    writes[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[7].descriptorCount = 1;
    writes[7].pBufferInfo = &vkDescriptorBufferInfo_scene3;

    writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[8].dstSet = gCtx_Switcher.vkDescriptorSet_scene3;
    writes[8].dstBinding = 1;
    writes[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[8].descriptorCount = 1;
    writes[8].pImageInfo = &scene3Texture;

    vkUpdateDescriptorSets(gCtx_Switcher.vkDevice, (uint32_t)_ARRAYSIZE(writes), writes, 0, NULL);
    fprintf(gCtx_Switcher.gpFile, "createDescriptorSet() --> vkUpdateDescriptorSets() is succedded\n");
    
    return (vkResult);
}

VkResult createRenderPass(void)
{
    //variable
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //step1:
    VkAttachmentDescription vkAttachmentDescription_array[2];  //for both color and depth
    memset((void*)vkAttachmentDescription_array, 0, sizeof(VkAttachmentDescription) * _ARRAYSIZE(vkAttachmentDescription_array));
    
    //for color
    vkAttachmentDescription_array[0].flags = 0; //For embedded devices
    vkAttachmentDescription_array[0].format = gCtx_Switcher.vkFormat_color;
    vkAttachmentDescription_array[0].samples = VK_SAMPLE_COUNT_1_BIT;
    vkAttachmentDescription_array[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    vkAttachmentDescription_array[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    vkAttachmentDescription_array[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    vkAttachmentDescription_array[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    vkAttachmentDescription_array[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkAttachmentDescription_array[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    //for depth
    vkAttachmentDescription_array[1].flags = 0; //For embedded devices
    vkAttachmentDescription_array[1].format = gCtx_Switcher.vkFormat_depth;
    vkAttachmentDescription_array[1].samples = VK_SAMPLE_COUNT_1_BIT;
    vkAttachmentDescription_array[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    vkAttachmentDescription_array[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    vkAttachmentDescription_array[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    vkAttachmentDescription_array[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    vkAttachmentDescription_array[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkAttachmentDescription_array[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    

    //Step2:
    //for color attachment
    VkAttachmentReference vkAttachmentReference_color;
    memset((void*)&vkAttachmentReference_color, 0, sizeof(VkAttachmentReference));
    
    vkAttachmentReference_color.attachment = 0;  //index number
    vkAttachmentReference_color.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //how to keep/desires/use the layout of image
    
    //for depth attachment
    VkAttachmentReference vkAttachmentReference_depth;
    memset((void*)&vkAttachmentReference_depth, 0, sizeof(VkAttachmentReference));
    
    vkAttachmentReference_depth.attachment = 1;  //index number
    vkAttachmentReference_depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; //how to keep/desires/use the layout of image
    
    
    //step3:
    VkSubpassDescription vkSubpassDescription;
    memset((void*)&vkSubpassDescription, 0, sizeof(VkSubpassDescription));
    
    vkSubpassDescription.flags = 0;
    vkSubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkSubpassDescription.inputAttachmentCount = 0;
    vkSubpassDescription.pInputAttachments = NULL;
    // vkSubpassDescription.colorAttachmentCount = _ARRAYSIZE(vkAttachmentDescription_array);  // earlier code
    vkSubpassDescription.colorAttachmentCount = 1;  //Recommended change for Depth:: This count should be of count vkAttachmentReference of color count
    vkSubpassDescription.pColorAttachments = &vkAttachmentReference_color;
    vkSubpassDescription.pResolveAttachments = NULL;
    vkSubpassDescription.pDepthStencilAttachment = &vkAttachmentReference_depth;
    vkSubpassDescription.preserveAttachmentCount = 0;
    vkSubpassDescription.pPreserveAttachments = NULL;
    
    //step4:
    VkRenderPassCreateInfo vkRenderPassCreateInfo;
    memset((void*)&vkRenderPassCreateInfo, 0, sizeof(VkRenderPassCreateInfo));

    vkRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    vkRenderPassCreateInfo.pNext = NULL;
    vkRenderPassCreateInfo.flags = 0;
    vkRenderPassCreateInfo.attachmentCount = _ARRAYSIZE(vkAttachmentDescription_array);
    vkRenderPassCreateInfo.pAttachments = vkAttachmentDescription_array;
    vkRenderPassCreateInfo.subpassCount = 1;
    vkRenderPassCreateInfo.pSubpasses = &vkSubpassDescription;
    vkRenderPassCreateInfo.dependencyCount = 0;
    vkRenderPassCreateInfo.pDependencies = NULL;
    
    
    //step5:
    vkResult = vkCreateRenderPass(gCtx_Switcher.vkDevice,
                                  &vkRenderPassCreateInfo,
                                  NULL,
                                  &gCtx_Switcher.vkRenderPass);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createRenderPass() --> vkCreateRenderPass() is failed & error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createRenderPass() --> vkCreateRenderPass() is succedded\n");
    }

    VkAttachmentDescription offscreenAttachments[2];
    memset(offscreenAttachments, 0, sizeof(offscreenAttachments));
    offscreenAttachments[0].format = gCtx_Switcher.vkFormat_color;
    offscreenAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    offscreenAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    offscreenAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    offscreenAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    offscreenAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    offscreenAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    offscreenAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    offscreenAttachments[1].format = gCtx_Switcher.vkFormat_depth;
    offscreenAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    offscreenAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    offscreenAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    offscreenAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    offscreenAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    offscreenAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    offscreenAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference offscreenColor;
    memset(&offscreenColor, 0, sizeof(offscreenColor));
    offscreenColor.attachment = 0;
    offscreenColor.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference offscreenDepth;
    memset(&offscreenDepth, 0, sizeof(offscreenDepth));
    offscreenDepth.attachment = 1;
    offscreenDepth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription offscreenSubpass;
    memset(&offscreenSubpass, 0, sizeof(offscreenSubpass));
    offscreenSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    offscreenSubpass.colorAttachmentCount = 1;
    offscreenSubpass.pColorAttachments = &offscreenColor;
    offscreenSubpass.pDepthStencilAttachment = &offscreenDepth;

    VkRenderPassCreateInfo offscreenInfo;
    memset(&offscreenInfo, 0, sizeof(offscreenInfo));
    offscreenInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    offscreenInfo.attachmentCount = _ARRAYSIZE(offscreenAttachments);
    offscreenInfo.pAttachments = offscreenAttachments;
    offscreenInfo.subpassCount = 1;
    offscreenInfo.pSubpasses = &offscreenSubpass;

    if (gOffscreenRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(gCtx_Switcher.vkDevice, gOffscreenRenderPass, NULL);
        gOffscreenRenderPass = VK_NULL_HANDLE;
    }

    vkResult = vkCreateRenderPass(gCtx_Switcher.vkDevice,
                                  &offscreenInfo,
                                  NULL,
                                  &gOffscreenRenderPass);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createRenderPass() --> offscreen vkCreateRenderPass() failed %d\n", vkResult);
        return vkResult;
    }

    return (vkResult);
}

VkResult createPipeline(void)
{
    //variables
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //#1: vertex input state
    VkVertexInputBindingDescription vkVertexInputBindingDescription_array[2]; 
    memset((void*)vkVertexInputBindingDescription_array, 0, sizeof(VkVertexInputBindingDescription) * _ARRAYSIZE(vkVertexInputBindingDescription_array));
    
    //for position
    vkVertexInputBindingDescription_array[0].binding = 0;//corresponding to location 0 in vertex shader
    vkVertexInputBindingDescription_array[0].stride = sizeof(float) * 3;
    vkVertexInputBindingDescription_array[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    //for texcoord
    vkVertexInputBindingDescription_array[1].binding = 1; //corresponding to location 1 in vertex shader
    vkVertexInputBindingDescription_array[1].stride = sizeof(float) * 2;
    vkVertexInputBindingDescription_array[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  
    
    VkVertexInputAttributeDescription vkVertexInputAttributeDescription_array[2]; 
    memset((void*)vkVertexInputAttributeDescription_array, 0, sizeof(VkVertexInputAttributeDescription) * _ARRAYSIZE(vkVertexInputAttributeDescription_array));
    
    //for position
    vkVertexInputAttributeDescription_array[0].binding = 0;
    vkVertexInputAttributeDescription_array[0].location = 0;
    vkVertexInputAttributeDescription_array[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vkVertexInputAttributeDescription_array[0].offset = 0;
    
    //for texcoord
    vkVertexInputAttributeDescription_array[1].binding = 1;
    vkVertexInputAttributeDescription_array[1].location = 1;
    vkVertexInputAttributeDescription_array[1].format = VK_FORMAT_R32G32_SFLOAT;
    vkVertexInputAttributeDescription_array[1].offset = 0;
    
    
    VkPipelineVertexInputStateCreateInfo vkPipelineVertexInputStateCreateInfo;
    memset((void*)&vkPipelineVertexInputStateCreateInfo, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
    vkPipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vkPipelineVertexInputStateCreateInfo.pNext = NULL;
    vkPipelineVertexInputStateCreateInfo.flags = 0;
    vkPipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = _ARRAYSIZE(vkVertexInputBindingDescription_array);
    vkPipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vkVertexInputBindingDescription_array;
    vkPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = _ARRAYSIZE(vkVertexInputAttributeDescription_array);
    vkPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vkVertexInputAttributeDescription_array;
    
    
    //#2: Input assembly State
    VkPipelineInputAssemblyStateCreateInfo vkPipelineInputAssemblyStateCreateInfo;
    memset((void*)&vkPipelineInputAssemblyStateCreateInfo, 0, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    vkPipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    vkPipelineInputAssemblyStateCreateInfo.pNext = NULL;
    vkPipelineInputAssemblyStateCreateInfo.flags = 0;
    vkPipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    vkPipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = 0;
    
    
    //#3: Rasterizer state
    VkPipelineRasterizationStateCreateInfo vkPipelineRasterizationStateCreateInfo;
    memset((void*)&vkPipelineRasterizationStateCreateInfo, 0, sizeof(VkPipelineRasterizationStateCreateInfo));
    vkPipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    vkPipelineRasterizationStateCreateInfo.pNext = NULL;
    vkPipelineRasterizationStateCreateInfo.flags = 0;
    vkPipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    vkPipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    vkPipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    vkPipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
    
    //#4: Color Blend State
    VkPipelineColorBlendAttachmentState vkPipelineColorBlendAttachmentState_array[1];
    memset((void*)vkPipelineColorBlendAttachmentState_array, 0, sizeof(VkPipelineColorBlendAttachmentState) * _ARRAYSIZE(vkPipelineColorBlendAttachmentState_array));
    vkPipelineColorBlendAttachmentState_array[0].blendEnable = VK_TRUE;
	
	// Pre-multiplied alpha: final = src + dst*(1 - src.a)
        vkPipelineColorBlendAttachmentState_array[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        vkPipelineColorBlendAttachmentState_array[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        vkPipelineColorBlendAttachmentState_array[0].colorBlendOp        = VK_BLEND_OP_ADD;

        vkPipelineColorBlendAttachmentState_array[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        vkPipelineColorBlendAttachmentState_array[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        vkPipelineColorBlendAttachmentState_array[0].alphaBlendOp        = VK_BLEND_OP_ADD;
	
    vkPipelineColorBlendAttachmentState_array[0].colorWriteMask = 0xF;
    
    VkPipelineColorBlendStateCreateInfo vkPipelineColorBlendStateCreateInfo;
    memset((void*)&vkPipelineColorBlendStateCreateInfo, 0, sizeof(VkPipelineColorBlendStateCreateInfo));
    vkPipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    vkPipelineColorBlendStateCreateInfo.pNext = NULL;
    vkPipelineColorBlendStateCreateInfo.flags = 0;
    vkPipelineColorBlendStateCreateInfo.attachmentCount = _ARRAYSIZE(vkPipelineColorBlendAttachmentState_array);
    vkPipelineColorBlendStateCreateInfo.pAttachments = vkPipelineColorBlendAttachmentState_array;

    VkDynamicState dynamicStates[1] = { VK_DYNAMIC_STATE_BLEND_CONSTANTS };
    VkPipelineDynamicStateCreateInfo vkPipelineDynamicStateCreateInfo;
    memset(&vkPipelineDynamicStateCreateInfo, 0, sizeof(vkPipelineDynamicStateCreateInfo));
    vkPipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    vkPipelineDynamicStateCreateInfo.dynamicStateCount = _ARRAYSIZE(dynamicStates);
    vkPipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates;

    //#5: viewport sciessor state
    VkPipelineViewportStateCreateInfo vkPipelineViewportStateCreateInfo;
    memset((void*)&vkPipelineViewportStateCreateInfo, 0, sizeof(VkPipelineViewportStateCreateInfo));
    vkPipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vkPipelineViewportStateCreateInfo.pNext = NULL;
    vkPipelineViewportStateCreateInfo.flags = 0;
    vkPipelineViewportStateCreateInfo.viewportCount = 1; //Means we can specify multiple viewport
    
    memset((void*)&gCtx_Switcher.vkViewport, 0, sizeof(VkViewport));
    gCtx_Switcher.vkViewport.x = 0;
    gCtx_Switcher.vkViewport.y = 0;
    gCtx_Switcher.vkViewport.width = (float)gCtx_Switcher.vkExtent2D_swapchain.width;
    gCtx_Switcher.vkViewport.height = (float)gCtx_Switcher.vkExtent2D_swapchain.height;
    gCtx_Switcher.vkViewport.minDepth = 0.0f;
    gCtx_Switcher.vkViewport.maxDepth = 1.0f;
    
    vkPipelineViewportStateCreateInfo.pViewports = &gCtx_Switcher.vkViewport;
    vkPipelineViewportStateCreateInfo.scissorCount = 1;
    
    memset((void*)&gCtx_Switcher.vkRect2D_scissor, 0, sizeof(VkRect2D));
    gCtx_Switcher.vkRect2D_scissor.offset.x = 0;
    gCtx_Switcher.vkRect2D_scissor.offset.y = 0;
    gCtx_Switcher.vkRect2D_scissor.extent.width = (float)gCtx_Switcher.vkExtent2D_swapchain.width;
    gCtx_Switcher.vkRect2D_scissor.extent.height = (float)gCtx_Switcher.vkExtent2D_swapchain.height;
    
    vkPipelineViewportStateCreateInfo.pScissors = &gCtx_Switcher.vkRect2D_scissor;
    
    //#6: Depth Stencil state
    //THIS STATE CAN BE OMMITTED AS WE DONT HAVE THE DEPTH
    VkPipelineDepthStencilStateCreateInfo vkPipelineDepthStencilStateCreateInfo;
    memset((void*)&vkPipelineDepthStencilStateCreateInfo, 0, sizeof(VkPipelineDepthStencilStateCreateInfo));
    vkPipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    vkPipelineDepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    vkPipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
    vkPipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    vkPipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    vkPipelineDepthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
    vkPipelineDepthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
    vkPipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
    vkPipelineDepthStencilStateCreateInfo.front = vkPipelineDepthStencilStateCreateInfo.back;
    vkPipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    
    //#7: Dynamic State
    //THIS STATE CAN BE OMMITTED AS WE DONT HAVE ANY DYNAMIC STATE
    
    //#8: Multi Sample State(needed for fragment shader)
    VkPipelineMultisampleStateCreateInfo vkPipelineMultisampleStateCreateInfo;
    memset((void*)&vkPipelineMultisampleStateCreateInfo, 0, sizeof(VkPipelineMultisampleStateCreateInfo));
    vkPipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    vkPipelineMultisampleStateCreateInfo.pNext = NULL;
    vkPipelineMultisampleStateCreateInfo.flags = 0;
    vkPipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    //#9: Shader State
    VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfo_array[2];
    memset((void*)vkPipelineShaderStageCreateInfo_array, 0, sizeof(VkPipelineShaderStageCreateInfo) * _ARRAYSIZE(vkPipelineShaderStageCreateInfo_array));
    //Vertex Shader
    vkPipelineShaderStageCreateInfo_array[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vkPipelineShaderStageCreateInfo_array[0].pNext = NULL;
    vkPipelineShaderStageCreateInfo_array[0].flags = 0;
    vkPipelineShaderStageCreateInfo_array[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    vkPipelineShaderStageCreateInfo_array[0].module = gCtx_Switcher.vkShaderModule_vertex_shader;
    vkPipelineShaderStageCreateInfo_array[0].pName = "main";
    vkPipelineShaderStageCreateInfo_array[0].pSpecializationInfo = NULL;
    
    //fragment Shader
    vkPipelineShaderStageCreateInfo_array[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vkPipelineShaderStageCreateInfo_array[1].pNext = NULL;
    vkPipelineShaderStageCreateInfo_array[1].flags = 0;
    vkPipelineShaderStageCreateInfo_array[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    vkPipelineShaderStageCreateInfo_array[1].module = gCtx_Switcher.vkShaderModule_fragment_shader;
    vkPipelineShaderStageCreateInfo_array[1].pName = "main";
    vkPipelineShaderStageCreateInfo_array[1].pSpecializationInfo = NULL;
    
    //#10: Tessellation State
    //THIS STATE CAN BE OMMITTED AS WE DONT HAVE ANY TESSELLATION SHADER
    
    
    //As pipeline are created from pipeline cache, we will create the pipeline cache object
    VkPipelineCacheCreateInfo vkPipelineCacheCreateInfo;
    memset((void*)&vkPipelineCacheCreateInfo, 0, sizeof(VkPipelineCacheCreateInfo));
    vkPipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vkPipelineCacheCreateInfo.pNext = NULL;
    vkPipelineCacheCreateInfo.flags = 0;
    
    
    vkResult = vkCreatePipelineCache(gCtx_Switcher.vkDevice, &vkPipelineCacheCreateInfo, NULL, &gCtx_Switcher.vkPipelineCache);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createPipeline() --> vkCreatePipelineCache() is failed error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createPipeline() --> vkCreatePipelineCache() is succedded\n");
    }
    
    //create the actual graphics pipeline
    VkGraphicsPipelineCreateInfo vkGraphicsPipelineCreateInfo;
    memset((void*)&vkGraphicsPipelineCreateInfo, 0, sizeof(VkGraphicsPipelineCreateInfo));
    vkGraphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    vkGraphicsPipelineCreateInfo.pNext = NULL;
    vkGraphicsPipelineCreateInfo.flags = 0;
    vkGraphicsPipelineCreateInfo.pVertexInputState = &vkPipelineVertexInputStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pInputAssemblyState = &vkPipelineInputAssemblyStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pRasterizationState = &vkPipelineRasterizationStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pColorBlendState = &vkPipelineColorBlendStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pViewportState = &vkPipelineViewportStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pDepthStencilState = &vkPipelineDepthStencilStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pDynamicState = &vkPipelineDynamicStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pMultisampleState = &vkPipelineMultisampleStateCreateInfo;
    vkGraphicsPipelineCreateInfo.stageCount = _ARRAYSIZE(vkPipelineShaderStageCreateInfo_array);
    vkGraphicsPipelineCreateInfo.pStages = vkPipelineShaderStageCreateInfo_array;
    vkGraphicsPipelineCreateInfo.pTessellationState = NULL;
    vkGraphicsPipelineCreateInfo.layout = gCtx_Switcher.vkPipelineLayout;
    vkGraphicsPipelineCreateInfo.renderPass = gCtx_Switcher.vkRenderPass;
    vkGraphicsPipelineCreateInfo.subpass = 0; //as we have only one renderpass
    vkGraphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    vkGraphicsPipelineCreateInfo.basePipelineIndex = 0;
    
    //Now create the pipeline
    vkResult = vkCreateGraphicsPipelines(gCtx_Switcher.vkDevice,
                                         gCtx_Switcher.vkPipelineCache,
                                         1,
                                         &vkGraphicsPipelineCreateInfo,
                                         NULL,
                                         &gCtx_Switcher.vkPipeline_scene0);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createPipeline() --> vkCreateGraphicsPipelines() is failed error code is %d\n", vkResult);
        vkDestroyPipelineCache(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipelineCache, NULL);
        gCtx_Switcher.vkPipelineCache = VK_NULL_HANDLE;
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createPipeline() --> Scene0 pipeline created\n");
    }
    
    //we ar done with pipeline cache so destroy it
    vkDestroyPipelineCache(gCtx_Switcher.vkDevice, gCtx_Switcher.vkPipelineCache, NULL);
    gCtx_Switcher.vkPipelineCache = VK_NULL_HANDLE;
    
    /* ===== Scene 1 pipeline (uses Scene 1 shaders) ===== */
    VkPipelineCacheCreateInfo scene1PipelineCacheInfo;
    memset(&scene1PipelineCacheInfo, 0, sizeof(scene1PipelineCacheInfo));
    scene1PipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkPipelineCache scene1Cache = VK_NULL_HANDLE;
    vkCreatePipelineCache(gCtx_Switcher.vkDevice, &scene1PipelineCacheInfo, NULL, &scene1Cache);

    VkPipelineShaderStageCreateInfo scene1Stages[2];
    memset(scene1Stages, 0, sizeof(scene1Stages));
    scene1Stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    scene1Stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    scene1Stages[0].module = gShaderModule_vertex_scene1;
    scene1Stages[0].pName  = "main";
    scene1Stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    scene1Stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    scene1Stages[1].module = gShaderModule_fragment_scene1;
    scene1Stages[1].pName  = "main";

    VkPipelineColorBlendAttachmentState scene1BlendAttachment;
    memset(&scene1BlendAttachment, 0, sizeof(scene1BlendAttachment));
    scene1BlendAttachment.colorWriteMask = 0xF;
    scene1BlendAttachment.blendEnable    = VK_TRUE;
    scene1BlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
    scene1BlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    scene1BlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    scene1BlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
    scene1BlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    scene1BlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo scene1BlendState;
    memset(&scene1BlendState, 0, sizeof(scene1BlendState));
    scene1BlendState.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    scene1BlendState.attachmentCount = 1;
    scene1BlendState.pAttachments    = &scene1BlendAttachment;

    VkGraphicsPipelineCreateInfo scene1PipelineInfo = vkGraphicsPipelineCreateInfo;
    scene1PipelineInfo.pStages          = scene1Stages;
    scene1PipelineInfo.stageCount       = 2;
    scene1PipelineInfo.pColorBlendState = &scene1BlendState;

    vkResult = vkCreateGraphicsPipelines(gCtx_Switcher.vkDevice,
                                         scene1Cache,
                                         1,
                                         &scene1PipelineInfo,
                                         NULL,
                                         &gCtx_Switcher.vkPipeline_scene1);
    vkDestroyPipelineCache(gCtx_Switcher.vkDevice, scene1Cache, NULL);
    if (vkResult != VK_SUCCESS) return vkResult;

    fprintf(gCtx_Switcher.gpFile, "createPipeline() --> Scene1 pipeline created\n");

    /* ===== Scene 2 pipeline ===== */
    VkPipelineCacheCreateInfo scene2PipelineCacheInfo;
    memset(&scene2PipelineCacheInfo, 0, sizeof(scene2PipelineCacheInfo));
    scene2PipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkPipelineCache scene2Cache = VK_NULL_HANDLE;
    vkCreatePipelineCache(gCtx_Switcher.vkDevice, &scene2PipelineCacheInfo, NULL, &scene2Cache);

    VkPipelineShaderStageCreateInfo scene2Stages[2];
    memset(scene2Stages, 0, sizeof(scene2Stages));
    scene2Stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    scene2Stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    scene2Stages[0].module = gShaderModule_vertex_scene2;
    scene2Stages[0].pName  = "main";
    scene2Stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    scene2Stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    scene2Stages[1].module = gShaderModule_fragment_scene2;
    scene2Stages[1].pName  = "main";

    VkPipelineColorBlendAttachmentState scene2BlendAttachment;
    memset(&scene2BlendAttachment, 0, sizeof(scene2BlendAttachment));
    scene2BlendAttachment.colorWriteMask = 0xF;
    scene2BlendAttachment.blendEnable    = VK_TRUE;
    scene2BlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
    scene2BlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    scene2BlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    scene2BlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
    scene2BlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    scene2BlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo scene2BlendState;
    memset(&scene2BlendState, 0, sizeof(scene2BlendState));
    scene2BlendState.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    scene2BlendState.attachmentCount = 1;
    scene2BlendState.pAttachments    = &scene2BlendAttachment;

    VkGraphicsPipelineCreateInfo scene2PipelineInfo = vkGraphicsPipelineCreateInfo;
    scene2PipelineInfo.pStages          = scene2Stages;
    scene2PipelineInfo.stageCount       = 2;
    scene2PipelineInfo.pColorBlendState = &scene2BlendState;

    vkResult = vkCreateGraphicsPipelines(gCtx_Switcher.vkDevice,
                                         scene2Cache,
                                         1,
                                         &scene2PipelineInfo,
                                         NULL,
                                         &gCtx_Switcher.vkPipeline_scene2);
    vkDestroyPipelineCache(gCtx_Switcher.vkDevice, scene2Cache, NULL);
    if (vkResult != VK_SUCCESS) return vkResult;

    fprintf(gCtx_Switcher.gpFile, "createPipeline() --> Scene2 pipeline created\n");

    /* ===== Scene 3 pipeline ===== */
    VkPipelineCacheCreateInfo scene3PipelineCacheInfo;
    memset(&scene3PipelineCacheInfo, 0, sizeof(scene3PipelineCacheInfo));
    scene3PipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkPipelineCache scene3Cache = VK_NULL_HANDLE;
    vkCreatePipelineCache(gCtx_Switcher.vkDevice, &scene3PipelineCacheInfo, NULL, &scene3Cache);

    VkPipelineShaderStageCreateInfo scene3Stages[2];
    memset(scene3Stages, 0, sizeof(scene3Stages));
    scene3Stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    scene3Stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    scene3Stages[0].module = gShaderModule_vertex_scene3;
    scene3Stages[0].pName  = "main";
    scene3Stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    scene3Stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    scene3Stages[1].module = gShaderModule_fragment_scene3;
    scene3Stages[1].pName  = "main";

    VkPipelineColorBlendAttachmentState scene3BlendAttachment;
    memset(&scene3BlendAttachment, 0, sizeof(scene3BlendAttachment));
    scene3BlendAttachment.colorWriteMask = 0xF;
    scene3BlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo scene3BlendState;
    memset(&scene3BlendState, 0, sizeof(scene3BlendState));
    scene3BlendState.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    scene3BlendState.attachmentCount = 1;
    scene3BlendState.pAttachments    = &scene3BlendAttachment;

    VkGraphicsPipelineCreateInfo scene3PipelineInfo = vkGraphicsPipelineCreateInfo;
    scene3PipelineInfo.pStages          = scene3Stages;
    scene3PipelineInfo.stageCount       = 2;
    scene3PipelineInfo.pColorBlendState = &scene3BlendState;

    vkResult = vkCreateGraphicsPipelines(gCtx_Switcher.vkDevice,
                                         scene3Cache,
                                         1,
                                         &scene3PipelineInfo,
                                         NULL,
                                         &gCtx_Switcher.vkPipeline_scene3);
    vkDestroyPipelineCache(gCtx_Switcher.vkDevice, scene3Cache, NULL);
    if (vkResult != VK_SUCCESS) return vkResult;

    fprintf(gCtx_Switcher.gpFile, "createPipeline() --> Scene3 pipeline created\n");
    return (vkResult);
}



VkResult createFrameBuffers(void)
{
    //variables
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //step1
    VkImageView vkImageView_attachment_array[1];
    memset((void*)vkImageView_attachment_array, 0, sizeof(VkImageView) * _ARRAYSIZE(vkImageView_attachment_array));
    
    // step2
    VkFramebufferCreateInfo vkFramebufferCreateInfo;
    memset((void*)&vkFramebufferCreateInfo, 0, sizeof(VkFramebufferCreateInfo));
    
    vkFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    vkFramebufferCreateInfo.pNext = NULL;
    vkFramebufferCreateInfo.flags = 0;
    vkFramebufferCreateInfo.renderPass = gCtx_Switcher.vkRenderPass;
    vkFramebufferCreateInfo.attachmentCount = _ARRAYSIZE(vkImageView_attachment_array);
    vkFramebufferCreateInfo.pAttachments = vkImageView_attachment_array;
    vkFramebufferCreateInfo.width = gCtx_Switcher.vkExtent2D_swapchain.width;
    vkFramebufferCreateInfo.height = gCtx_Switcher.vkExtent2D_swapchain.height;
    vkFramebufferCreateInfo.layers = 1;
    
    //step3:
    gCtx_Switcher.vkFrameBuffer_array = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * gCtx_Switcher.swapchainImageCount);
    //check for malloc
    
    //step4:
    
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        // Recommended Change for depth
        /*********************************************************************************************************************/
        VkImageView vkImageView_attachment_array[2]; //for color and depth
        memset((void*)vkImageView_attachment_array, 0, sizeof(VkImageView) * _ARRAYSIZE(vkImageView_attachment_array));
        
        //step2
        VkFramebufferCreateInfo vkFramebufferCreateInfo;
        memset((void*)&vkFramebufferCreateInfo, 0, sizeof(VkFramebufferCreateInfo));
        
        vkFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        vkFramebufferCreateInfo.pNext = NULL;
        vkFramebufferCreateInfo.flags = 0;
        vkFramebufferCreateInfo.renderPass = gCtx_Switcher.vkRenderPass;
        vkFramebufferCreateInfo.attachmentCount = _ARRAYSIZE(vkImageView_attachment_array);
        vkFramebufferCreateInfo.pAttachments = vkImageView_attachment_array;
        vkFramebufferCreateInfo.width = gCtx_Switcher.vkExtent2D_swapchain.width;
        vkFramebufferCreateInfo.height = gCtx_Switcher.vkExtent2D_swapchain.height;
        vkFramebufferCreateInfo.layers = 1;
        
        /*********************************************************************************************************************/
        
        vkImageView_attachment_array[0] = gCtx_Switcher.swapchainImageView_array[i];
        vkImageView_attachment_array[1] = gCtx_Switcher.vkImageView_depth;
        
        vkResult = vkCreateFramebuffer(gCtx_Switcher.vkDevice, 
                                       &vkFramebufferCreateInfo,
                                       NULL,
                                       &gCtx_Switcher.vkFrameBuffer_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile, "createFrameBuffers() --> vkCreateFramebuffer() is failed for %d iteration and error code is %d\n",i, vkResult);
            return vkResult;
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "createFrameBuffers() --> vkCreateFramebuffer() is succedded for iteration %d\n", i);
        }
    }
    
    return vkResult;
}


VkResult createSemaphores(void)
{
    //variables
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //step1
    VkSemaphoreCreateInfo vkSemaphoreCreateInfo;
    memset((void*)&vkSemaphoreCreateInfo, 0, sizeof(VkSemaphoreCreateInfo));
    
    vkSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkSemaphoreCreateInfo.pNext = NULL;  //Binary and Timeline Semaphore info, bydefault it is Binary
    vkSemaphoreCreateInfo.flags = 0; //RESERVED: must be zero
    
    //backBuffer Semaphore
    vkResult = vkCreateSemaphore(gCtx_Switcher.vkDevice, 
                                 &vkSemaphoreCreateInfo,
                                 NULL,
                                 &gCtx_Switcher.vkSemaphore_backBuffer);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createSemaphores() --> vkCreateSemaphore() is failed for gCtx_Switcher.vkSemaphore_backBuffer and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createSemaphores() --> vkCreateSemaphore() is succedded for gCtx_Switcher.vkSemaphore_backBuffer\n");
    }
    
    
    //renderComplete Semaphore
    vkResult = vkCreateSemaphore(gCtx_Switcher.vkDevice, 
                                 &vkSemaphoreCreateInfo,
                                 NULL,
                                 &gCtx_Switcher.vkSemaphore_renderComplete);
    if(vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "createSemaphores() --> vkCreateSemaphore() is failed for gCtx_Switcher.vkSemaphore_renderComplete and error code is %d\n", vkResult);
        return vkResult;
    }
    else
    {
        fprintf(gCtx_Switcher.gpFile, "createSemaphores() --> vkCreateSemaphore() is succedded for gCtx_Switcher.vkSemaphore_renderComplete\n");
    }
    
    return vkResult;
    
}


VkResult createFences(void)
{
    //variables
    VkResult vkResult = VK_SUCCESS;
    
    //code
    //step1
    VkFenceCreateInfo vkFenceCreateInfo;
    memset((void*)&vkFenceCreateInfo, 0, sizeof(VkFenceCreateInfo));
    
    vkFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkFenceCreateInfo.pNext = NULL;
    vkFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    gCtx_Switcher.vkFence_array = (VkFence*)malloc(sizeof(VkFence) * gCtx_Switcher.swapchainImageCount);
    //malloc error checking to be done
    
    for(uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; i++)
    {
        vkResult = vkCreateFence(gCtx_Switcher.vkDevice, 
                                 &vkFenceCreateInfo,
                                 NULL,
                                 &gCtx_Switcher.vkFence_array[i]);
        if(vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile, "createFences() --> vkCreateFence() is failed for %d iteration and error code is %d\n", i, vkResult);
            return vkResult;
        }
        else
        {
            fprintf(gCtx_Switcher.gpFile, "createFences() --> vkCreateFence() is succedded for %d iteration\n", i);
        }
    }

    return vkResult;
}


VkResult buildCommandBuffers(void)
{
    VkResult vkResult = VK_SUCCESS;
    float fade = Scene1_GetPendingBlendFade();
    float transition = fade;
    if (transition < 0.0f)
    {
        transition = 0.0f;
    }
    else if (transition > 1.0f)
    {
        transition = 1.0f;
    }

    if (gOffscreenRenderPass == VK_NULL_HANDLE)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!gCompositeCommandBuffer_array)
    {
        VkResult ensure = EnsureCompositeCommandBuffers();
        if (ensure != VK_SUCCESS)
        {
            return ensure;
        }
    }

    float focusPull = gCtx_Switcher.gScene23FocusPullFactor;
    VkResult compositeUniform = UpdateCompositeUniformBuffer(transition, focusPull);
    if (compositeUniform != VK_SUCCESS)
    {
        return compositeUniform;
    }

    float sceneBlend[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    for (uint32_t i = 0; i < gCtx_Switcher.swapchainImageCount; ++i)
    {
        VkCommandBuffer sceneBuffers[4] =
        {
            gCtx_Switcher.vkCommandBuffer_scene0_array[i],
            gCtx_Switcher.vkCommandBuffer_scene1_array[i],
            gCtx_Switcher.vkCommandBuffer_scene2_array[i],
            gCtx_Switcher.vkCommandBuffer_scene3_array[i]
        };

        VkPipeline pipelines[4] =
        {
            gCtx_Switcher.vkPipeline_scene0,
            gCtx_Switcher.vkPipeline_scene1,
            gCtx_Switcher.vkPipeline_scene2,
            gCtx_Switcher.vkPipeline_scene3
        };

        VkDescriptorSet descriptorSets[4] =
        {
            gCtx_Switcher.vkDescriptorSet_scene0,
            gCtx_Switcher.vkDescriptorSet_scene1,
            gCtx_Switcher.vkDescriptorSet_scene2,
            gCtx_Switcher.vkDescriptorSet_scene3
        };

        for (uint32_t sceneIndex = 0; sceneIndex < 4; ++sceneIndex)
        {
            VkCommandBuffer cb = sceneBuffers[sceneIndex];
            OffscreenTarget* target = &gOffscreenTargets[sceneIndex];

            if (cb == VK_NULL_HANDLE || target->framebuffer == VK_NULL_HANDLE)
            {
                continue;
            }

            vkResult = vkResetCommandBuffer(cb, 0);
            if (vkResult != VK_SUCCESS)
            {
                fprintf(gCtx_Switcher.gpFile,
                        "buildCommandBuffers() --> vkResetCommandBuffer() failed for image %u scene %u error %d\n",
                        i, sceneIndex, vkResult);
                return vkResult;
            }

            VkCommandBufferBeginInfo beginInfo;
            memset(&beginInfo, 0, sizeof(beginInfo));
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            vkResult = vkBeginCommandBuffer(cb, &beginInfo);
            if (vkResult != VK_SUCCESS)
            {
                fprintf(gCtx_Switcher.gpFile,
                        "buildCommandBuffers() --> vkBeginCommandBuffer() failed for image %u scene %u error %d\n",
                        i, sceneIndex, vkResult);
                return vkResult;
            }

            VkImageMemoryBarrier toColor;
            memset(&toColor, 0, sizeof(toColor));
            toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toColor.image = target->colorImage;
            toColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            toColor.subresourceRange.levelCount = 1;
            toColor.subresourceRange.layerCount = 1;
            toColor.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toColor.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toColor.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(cb,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 0,
                                 0, NULL,
                                 0, NULL,
                                 1, &toColor);

            VkClearValue clearValues[2];
            memset(clearValues, 0, sizeof(clearValues));
            clearValues[0].color = gCtx_Switcher.vkClearColorValue;
            clearValues[1].depthStencil = gCtx_Switcher.vkClearDepthStencilValue;

            VkRenderPassBeginInfo rpBegin;
            memset(&rpBegin, 0, sizeof(rpBegin));
            rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBegin.renderPass = gOffscreenRenderPass;
            rpBegin.framebuffer = target->framebuffer;
            rpBegin.renderArea.offset.x = 0;
            rpBegin.renderArea.offset.y = 0;
            rpBegin.renderArea.extent = gCtx_Switcher.vkExtent2D_swapchain;
            rpBegin.clearValueCount = _ARRAYSIZE(clearValues);
            rpBegin.pClearValues = clearValues;

            vkCmdBeginRenderPass(cb, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[sceneIndex]);
            vkCmdSetBlendConstants(cb, sceneBlend);

            VkDescriptorSet set = descriptorSets[sceneIndex];
            if (set != VK_NULL_HANDLE)
            {
                vkCmdBindDescriptorSets(cb,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        gCtx_Switcher.vkPipelineLayout,
                                        0,
                                        1,
                                        &set,
                                        0,
                                        NULL);
            }

            VkDeviceSize offsets[1] = { 0 };
            vkCmdBindVertexBuffers(cb, 0, 1, &gCtx_Switcher.vertexData_position.vkBuffer, offsets);
            vkCmdBindVertexBuffers(cb, 1, 1, &gCtx_Switcher.vertexData_texcoord.vkBuffer, offsets);

            uint32_t vertexCount = (sceneIndex >= 2) ? 6u : 36u;
            vkCmdDraw(cb, vertexCount, 1, 0, 0);

            vkCmdEndRenderPass(cb);

            VkImageMemoryBarrier toSample;
            memset(&toSample, 0, sizeof(toSample));
            toSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toSample.image = target->colorImage;
            toSample.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            toSample.subresourceRange.levelCount = 1;
            toSample.subresourceRange.layerCount = 1;
            toSample.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toSample.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cb,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0, NULL,
                                 0, NULL,
                                 1, &toSample);

            vkResult = vkEndCommandBuffer(cb);
            if (vkResult != VK_SUCCESS)
            {
                fprintf(gCtx_Switcher.gpFile,
                        "buildCommandBuffers() --> vkEndCommandBuffer() failed for image %u scene %u error %d\n",
                        i, sceneIndex, vkResult);
                return vkResult;
            }
        }

        VkCommandBuffer compositeCb = gCompositeCommandBuffer_array[i];
        if (compositeCb == VK_NULL_HANDLE)
        {
            continue;
        }

        vkResult = vkResetCommandBuffer(compositeCb, 0);
        if (vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile,
                    "buildCommandBuffers() --> vkResetCommandBuffer() failed for composite %u error %d\n",
                    i, vkResult);
            return vkResult;
        }

        VkCommandBufferBeginInfo beginInfo;
        memset(&beginInfo, 0, sizeof(beginInfo));
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkResult = vkBeginCommandBuffer(compositeCb, &beginInfo);
        if (vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile,
                    "buildCommandBuffers() --> vkBeginCommandBuffer() failed for composite %u error %d\n",
                    i, vkResult);
            return vkResult;
        }

        VkClearValue swapchainClears[2];
        memset(swapchainClears, 0, sizeof(swapchainClears));
        swapchainClears[0].color = gCtx_Switcher.vkClearColorValue;
        swapchainClears[1].depthStencil = gCtx_Switcher.vkClearDepthStencilValue;

        VkRenderPassBeginInfo compositeBegin;
        memset(&compositeBegin, 0, sizeof(compositeBegin));
        compositeBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        compositeBegin.renderPass = gCtx_Switcher.vkRenderPass;
        compositeBegin.framebuffer = gCtx_Switcher.vkFrameBuffer_array[i];
        compositeBegin.renderArea.offset.x = 0;
        compositeBegin.renderArea.offset.y = 0;
        compositeBegin.renderArea.extent = gCtx_Switcher.vkExtent2D_swapchain;
        compositeBegin.clearValueCount = _ARRAYSIZE(swapchainClears);
        compositeBegin.pClearValues = swapchainClears;

        vkCmdBeginRenderPass(compositeCb, &compositeBegin, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(compositeCb, VK_PIPELINE_BIND_POINT_GRAPHICS, gCtx_Switcher.vkPipeline_scene2);

        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(compositeCb, 0, 1, &gCtx_Switcher.vertexData_position.vkBuffer, offsets);
        vkCmdBindVertexBuffers(compositeCb, 1, 1, &gCtx_Switcher.vertexData_texcoord.vkBuffer, offsets);

        uint32_t baseScene = UINT32_MAX;
        uint32_t overlayScene = UINT32_MAX;
        float overlayWeight = 0.0f;
        float baseWeight = 1.0f;

        switch (gActiveScene)
        {
        case ACTIVE_SCENE_SCENE1:
            if (gCtx_Switcher.gScene12CrossfadeActive)
            {
                baseScene = 2;
                overlayScene = 1;
                overlayWeight = transition;
                baseWeight = 1.0f - overlayWeight;
            }
            else
            {
                baseScene = 0;
                overlayScene = 1;
                overlayWeight = transition;
            }
            break;
        case ACTIVE_SCENE_SCENE2:
            /*
             * Scene 2 is a stand-alone fullscreen composition that does not
             * require the Scene 1 offscreen colour buffer as a background
             * layer.  Rendering the previous base layer briefly exposed the
             * Scene 0 imagery while Scene 2 was being latched, so keep the
             * compositing pass limited to the Scene 2 target only.
             */
            baseScene = UINT32_MAX;
            overlayScene = 2;
            overlayWeight = transition;
            break;
        case ACTIVE_SCENE_SCENE3:
            if (gCtx_Switcher.gScene23FocusPullActive)
            {
                baseScene = 2;
                overlayScene = 3;
                float focus = gCtx_Switcher.gScene23FocusPullFactor;
                if (focus < 0.0f) focus = 0.0f;
                if (focus > 1.0f) focus = 1.0f;
                float overlayEase = powf(focus, 0.75f);
                float baseEase = powf(1.0f - focus, 1.25f);
                overlayWeight = overlayEase;
                baseWeight = baseEase;
            }
            else
            {
                baseScene = UINT32_MAX;
                overlayScene = 3;
                overlayWeight = transition;
            }
            break;
        case ACTIVE_SCENE_SCENE0:
            baseScene = 0;
            overlayScene = UINT32_MAX;
            overlayWeight = 0.0f;
            break;
        case ACTIVE_SCENE_NONE:
        default:
            baseScene = UINT32_MAX;
            overlayScene = UINT32_MAX;
            overlayWeight = 0.0f;
            break;
        }

        BOOL scene12DoubleExposure = (gCtx_Switcher.gScene12CrossfadeActive &&
                                      baseScene == 2 &&
                                      overlayScene == 1);

        if (scene12DoubleExposure)
        {
            /* Maintain Scene 2 at full intensity so Scene 1 fades out as a
               double exposure overlay instead of a soft-focus transition. */
            baseWeight = 1.0f;
        }

        if (baseWeight < 0.0f) baseWeight = 0.0f;
        if (baseWeight > 1.0f) baseWeight = 1.0f;
        VkDescriptorSet baseSet = (baseScene != UINT32_MAX) ?
                                  gCompositeDescriptorSet_array[baseScene] : VK_NULL_HANDLE;

        float baseBlend[4] = { baseWeight, baseWeight, baseWeight, baseWeight };
        BOOL drawBaseScene = (baseSet != VK_NULL_HANDLE);
        if (drawBaseScene)
        {
            if (baseWeight <= 0.0f)
            {
                drawBaseScene = FALSE;
            }
            else if (gActiveScene != ACTIVE_SCENE_SCENE0 && transition < 1.0f)
            {
                if (!gCtx_Switcher.gScene01DoubleExposureActive &&
                    !gCtx_Switcher.gScene12CrossfadeActive)
                {
                    drawBaseScene = FALSE;
                }
            }
        }

        if (drawBaseScene)
        {
            vkCmdSetBlendConstants(compositeCb, baseBlend);
            vkCmdBindDescriptorSets(compositeCb,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    gCtx_Switcher.vkPipelineLayout,
                                    0,
                                    1,
                                    &baseSet,
                                    0,
                                    NULL);
            vkCmdDraw(compositeCb, 6, 1, 0, 0);
        }

        if (overlayScene != UINT32_MAX)
        {
            float weight = overlayWeight;
            if (weight < 0.0f) weight = 0.0f;
            if (weight > 1.0f) weight = 1.0f;

            VkDescriptorSet overlaySet = gCompositeDescriptorSet_array[overlayScene];
            if (weight > 0.0f && overlaySet != VK_NULL_HANDLE)
            {
                float overlayBlend[4] = { weight, weight, weight, weight };
                vkCmdSetBlendConstants(compositeCb, overlayBlend);
                vkCmdBindDescriptorSets(compositeCb,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        gCtx_Switcher.vkPipelineLayout,
                                        0,
                                        1,
                                        &overlaySet,
                                        0,
                                        NULL);
                vkCmdDraw(compositeCb, 6, 1, 0, 0);
            }
        }

        vkCmdEndRenderPass(compositeCb);

        vkResult = vkEndCommandBuffer(compositeCb);
        if (vkResult != VK_SUCCESS)
        {
            fprintf(gCtx_Switcher.gpFile,
                    "buildCommandBuffers() --> vkEndCommandBuffer() failed for composite %u error %d\n",
                    i, vkResult);
            return vkResult;
        }
    }

    Scene1_CommitPendingBlendFade();
    return VK_SUCCESS;
}


VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(VkDebugReportFlagsEXT vkDebugReportFlagsEXT,
                                                   VkDebugReportObjectTypeEXT vkDebugReportObjectTypeEXT,
                                                   uint64_t object,
                                                   size_t location,
                                                   int32_t messageCode,
                                                   const char* pLayerPrefix,
                                                   const char* pMessage,
                                                   void* pUserData)
{
    //code
    fprintf(gCtx_Switcher.gpFile, "SSA_Validation: debugReportCallback() --> %s (%d) = %s\n", pLayerPrefix, messageCode, pMessage);

    return(VK_FALSE);
}

void InitializeFunctionTable(void)
{
    memset(&gWin32FunctionTable_Switcher, 0, sizeof(gWin32FunctionTable_Switcher));
    memset(&gFunctionTable_Switcher, 0, sizeof(gFunctionTable_Switcher));

    gWin32FunctionTable_Switcher.WndProc = WndProc;
    gWin32FunctionTable_Switcher.Update = Update;
    gWin32FunctionTable_Switcher.Initialize = Initialize;
    gWin32FunctionTable_Switcher.Resize = Resize;
    gWin32FunctionTable_Switcher.Display = Display;
    gWin32FunctionTable_Switcher.Uninitialize = Uninitialize;
    gWin32FunctionTable_Switcher.ToggleFullscreen = ToggleFullscreen;
    gFunctionTable_Switcher.createVulkanInstance = createVulkanInstance;
    gFunctionTable_Switcher.fillInstanceExtensionNames = fillInstanceExtensionNames;
    gFunctionTable_Switcher.fillValidationLayerNames = fillValidationLayerNames;
    gFunctionTable_Switcher.createValidationCallbackFunction = createValidationCallbackFunction;
    gFunctionTable_Switcher.getSupportedSurface = getSupportedSurface;
    gFunctionTable_Switcher.getPhysicalDevice = getPhysicalDevice;
    gFunctionTable_Switcher.printVkInfo = printVkInfo;
    gFunctionTable_Switcher.fillDeviceExtensionNames = fillDeviceExtensionNames;
    gFunctionTable_Switcher.createVulkanDevice = createVulkanDevice;
    gFunctionTable_Switcher.getDeviceQueue = getDeviceQueue;
    gFunctionTable_Switcher.getPhysicalDeviceSurfaceFormatAndColorSpace = getPhysicalDeviceSurfaceFormatAndColorSpace;
    gFunctionTable_Switcher.getPhysicalDevicePresentMode = getPhysicalDevicePresentMode;
    gFunctionTable_Switcher.createSwapchain = createSwapchain;
    gFunctionTable_Switcher.HasStencilComponent = HasStencilComponent;
    gFunctionTable_Switcher.createImagesAndImageViews = createImagesAndImageViews;
    gFunctionTable_Switcher.GetSupportedDepthFormat = GetSupportedDepthFormat;
    gFunctionTable_Switcher.createCommandPool = createCommandPool;
    gFunctionTable_Switcher.createCommandBuffers = createCommandBuffers;
    gFunctionTable_Switcher.createVertexBuffer = createVertexBuffer;
    gFunctionTable_Switcher.createTexture = createTexture;
    gFunctionTable_Switcher.createUniformBuffer = createUniformBuffer;
    gFunctionTable_Switcher.updateUniformBuffer = updateUniformBuffer;
    gFunctionTable_Switcher.createShaders = createShaders;
    gFunctionTable_Switcher.createDescriptorSetLayout = createDescriptorSetLayout;
    gFunctionTable_Switcher.createPipelineLayout = createPipelineLayout;
    gFunctionTable_Switcher.createDescriptorPool = createDescriptorPool;
    gFunctionTable_Switcher.createDescriptorSet = createDescriptorSet;
    gFunctionTable_Switcher.createRenderPass = createRenderPass;
    gFunctionTable_Switcher.createPipeline = createPipeline;
    gFunctionTable_Switcher.createFrameBuffers = createFrameBuffers;
    gFunctionTable_Switcher.createSemaphores = createSemaphores;
    gFunctionTable_Switcher.createFences = createFences;
    gFunctionTable_Switcher.buildCommandBuffers = buildCommandBuffers;
    gFunctionTable_Switcher.debugReportCallback = debugReportCallback;
}

// Scene sequence management functions
void StartSceneSequence(void)
{
    RequestScenePhase(SEQUENCE_PHASE_SCENE0);
    fprintf(gCtx_Switcher.gpFile, "StartSceneSequence() --> Scene sequence primed for Scene0\n");
}

void UpdateSceneSequence(void)
{
    if (!gCtx_Switcher.gSequenceActive)
    {
        return;
    }

    ApplyScenePhase(gCtx_Switcher.gSequencePhase);
}

void StopSceneSequence(void)
{
    gCtx_Switcher.gSequenceActive = FALSE;
    gCtx_Switcher.gSequencePhase = SEQUENCE_PHASE_COMPLETE;
    ApplyScenePhase(SEQUENCE_PHASE_COMPLETE);
    fprintf(gCtx_Switcher.gpFile, "StopSceneSequence() --> Scene sequence halted\n");
}

BOOL IsSceneSequenceActive(void)
{
    return gCtx_Switcher.gSequenceActive;
}
