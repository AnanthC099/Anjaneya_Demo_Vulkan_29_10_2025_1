#include <string.h>

#include "SceneSwitcher.h"
#include "Scenes.h"

VkShaderModule gShaderModule_vertex_scene3 = VK_NULL_HANDLE;
VkShaderModule gShaderModule_fragment_scene3 = VK_NULL_HANDLE;

VkResult Scene3_UpdateUniformBuffer(void)
{
    GlobalContext_MyUniformData scene3;
    memset(&scene3, 0, sizeof(scene3));

    scene3.modelMatrix = glm::mat4(1.0f);
    scene3.viewMatrix = glm::mat4(1.0f);
    scene3.projectionMatrix = glm::mat4(1.0f);

    void* p = NULL;
    VkResult vkResult = vkMapMemory(gCtx_Switcher.vkDevice,
                                    gCtx_Switcher.uniformData_scene3.vkDeviceMemory,
                                    0,
                                    sizeof(scene3),
                                    0,
                                    &p);
    if (vkResult != VK_SUCCESS)
    {
        fprintf(gCtx_Switcher.gpFile, "Scene3_UpdateUniformBuffer() --> vkMapMemory() failed %d\n", vkResult);
        return vkResult;
    }

    memcpy(p, &scene3, sizeof(scene3));
    vkUnmapMemory(gCtx_Switcher.vkDevice, gCtx_Switcher.uniformData_scene3.vkDeviceMemory);
    return VK_SUCCESS;
}
