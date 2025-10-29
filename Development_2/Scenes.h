#pragma once

#include "SceneSwitcher.h"

#ifdef __cplusplus
extern "C" {
#endif

// Scene selection enum exported for cross-module use

typedef enum ActiveSceneTag
{
    ACTIVE_SCENE_NONE   = -1,
    ACTIVE_SCENE_SCENE0 = 0,
    ACTIVE_SCENE_SCENE1 = 1,
    ACTIVE_SCENE_SCENE2 = 2,
    ACTIVE_SCENE_SCENE3 = 3
} ActiveScene;

extern ActiveScene gActiveScene;

// Scene 0 interface
VkResult Scene0_UpdateUniformBuffer(void);

// Scene 1 shader modules (shared with SceneSwitcher.cpp)
extern VkShaderModule gShaderModule_vertex_scene1;
extern VkShaderModule gShaderModule_fragment_scene1;

// Scene 2 shader modules
extern VkShaderModule gShaderModule_vertex_scene2;
extern VkShaderModule gShaderModule_fragment_scene2;

// Scene 3 shader modules
extern VkShaderModule gShaderModule_vertex_scene3;
extern VkShaderModule gShaderModule_fragment_scene3;

// Scene 1 interface
VkResult Scene1_CreateTextures(void);
void Scene1_DestroyTextures(void);
void Scene1_BeginNewPan(void);
void Scene1_UpdateCameraAnim(void);
VkResult Scene1_UpdateUniformBuffer(void);
void Scene1_StartSequence(void);
void Scene1_StopSequence(void);
void Scene1_UpdateSequence(void);
BOOL Scene1_IsSequenceActive(void);
void Scene1_AdjustPanSpeed(float delta);
void Scene1_AdjustOverlaySize(float delta);
float Scene1_GetPendingBlendFade(void);
void Scene1_CommitPendingBlendFade(void);
void Scene1_UpdateBlendFade(float fade);
BOOL Scene1_IsCommandBufferDirty(void);
void Scene1_ClearCommandBufferDirty(void);
void Scene1_MarkCommandBufferDirty(void);
BOOL Scene1_HasPendingOverlay(void);
VkResult Scene1_BindPendingOverlay(VkDescriptorSet descriptorSet);
void Scene1_GetSkyDescriptor(VkDescriptorImageInfo* info);
void Scene1_GetOverlayDescriptor(VkDescriptorImageInfo* info);

// Scene 2 interface
VkResult Scene2_UpdateUniformBuffer(void);

// Scene 3 interface
VkResult Scene3_UpdateUniformBuffer(void);

#ifdef __cplusplus
}
#endif

