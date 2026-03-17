#pragma once
// Private gizmo pipeline declarations - included only inside VulkanRHI.
// Light sprite billboards (additive) - spot-cone wireframe (alpha-blend).

// Methods
void createLightSpritePipeline();
void createLightConePipeline();
void recordLightSpritePass(const std::vector<PointLight>& points,
                           const std::vector<SpotLight>&  spots);

// Members

/// Triangle-list, additive blend - Gizmo_LightSprite.spv
vk::raii::PipelineLayout m_lightSpritePipelineLayout = nullptr;
vk::raii::Pipeline       m_lightSpritePipeline       = nullptr;

/// Line-list, alpha blend - Gizmo_SpotCone.spv
vk::raii::PipelineLayout m_lightConePipelineLayout = nullptr;
vk::raii::Pipeline       m_lightConePipeline       = nullptr;

/// Cached each frame so recordLightSpritePass() has the data it needs.
std::vector<PointLight> m_lastPointLights;
std::vector<SpotLight>  m_lastSpotLights;
