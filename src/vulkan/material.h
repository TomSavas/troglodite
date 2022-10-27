enum class MeshPassType : uint8 {
    SHADOW = 0,
    FORWARD, // DEFERRED
    TRANSPARENT,

    PASS_COUNT,
}

struct MaterialData {
    std::vector<VkDescriptorSet> perPassDescriptorSets[MeshPassType::PASS_COUNT];
};

struct Material {
    const char* name;

    ShaderBundle* perPassShaders[MeshPassType::PASS_COUNT];
    MaterialData perPassData;
};
