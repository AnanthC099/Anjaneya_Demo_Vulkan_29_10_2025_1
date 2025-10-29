#include <string.h>

#include "SceneSwitcher.h"
#include "Scenes.h"

VkShaderModule gShaderModule_vertex_scene2 = VK_NULL_HANDLE;
VkShaderModule gShaderModule_fragment_scene2 = VK_NULL_HANDLE;

VkResult Scene2_UpdateUniformBuffer(void)
{
    GlobalContext_MyUniformData scene2;
    memset(&scene2, 0, sizeof(scene2));
    scene2.modelMatrix = glm::mat4(1.0f);
    scene2.viewMatrix = glm::mat4(1.0f);
    scene2.projectionMatrix = glm::mat4(1.0f);

    void* p = NULL;
    VkResult vkResult = vkMapMemory(gCtx_Switcher.vkDevice,
                                    gCtx_Switcher.uniformData_scene2.vkDeviceMemory,
                                    0,
                                    sizeof(scene2),
                                    0,
                                    &p);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Scene2_UpdateUniformBuffer() --> vkMapMemory() failed %d\n", vkResult);
        return vkResult;
    }

    memcpy(p, &scene2, sizeof(scene2));
    vkUnmapMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene2.vkDeviceMemory);
    return VK_SUCCESS;
}

