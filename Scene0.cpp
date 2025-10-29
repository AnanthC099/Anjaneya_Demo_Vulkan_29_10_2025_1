#include <string.h>

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include "glm/gtc/matrix_transform.hpp"

#include "SceneSwitcher.h"
#include "Scenes.h"

VkResult Scene0_UpdateUniformBuffer(void)
{
    GlobalContext_MyUniformData myUniformData;
    memset(&myUniformData, 0, sizeof(GlobalContext_MyUniformData));

    myUniformData.modelMatrix = glm::mat4(1.0f);

    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -4.0f));

    glm::mat4 rotationMatrix_X = glm::rotate(glm::mat4(1.0f), glm::radians(gCtx_Switcher.angle), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotationMatrix_Y = glm::rotate(glm::mat4(1.0f), glm::radians(gCtx_Switcher.angle), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotationMatrix_Z = glm::rotate(glm::mat4(1.0f), glm::radians(gCtx_Switcher.angle), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 rotationMatrix = rotationMatrix_X * rotationMatrix_Y * rotationMatrix_Z;

    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(2.05f, 1.25f, 1.25f));

    myUniformData.modelMatrix = translationMatrix * scaleMatrix * rotationMatrix;
    myUniformData.viewMatrix = glm::mat4(1.0f);

    glm::mat4 perspectiveProjectionMatrix = glm::perspective(glm::radians(45.0f),
                                                             (float)gCtx_Switcher.winWidth / (float)gCtx_Switcher.winHeight,
                                                             0.1f,
                                                             100.0f);
    perspectiveProjectionMatrix[1][1] *= -1.0f;
    myUniformData.projectionMatrix = perspectiveProjectionMatrix;

    float fade = gCtx_Switcher.gFade;
    if (gActiveScene == ACTIVE_SCENE_SCENE1)
    {
        if (gCtx_Switcher.gScene01DoubleExposureActive)
        {
            fade = 1.0f - fade;
        }
        else
        {
            fade = 0.0f;
        }
    }
    else if (gActiveScene != ACTIVE_SCENE_SCENE0)
    {
        fade = 0.0f;
    }
    myUniformData.fade = glm::vec4(fade, 0.0f, 0.0f, 0.0f);

    void* data = NULL;
    VkResult vkResult = vkMapMemory(gCtx_Switcher.vkDevice,
                                    gCtx_Switcher.uniformData_scene0.vkDeviceMemory,
                                    0,
                                    sizeof(GlobalContext_MyUniformData),
                                    0,
                                    &data);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Scene0_UpdateUniformBuffer() --> vkMapMemory() failed %d\n", vkResult);
        return vkResult;
    }

    memcpy(data, &myUniformData, sizeof(GlobalContext_MyUniformData));
    vkUnmapMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene0.vkDeviceMemory);
    return VK_SUCCESS;
}

