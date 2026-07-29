struct ShadowUniformData
{
    float4x4 model;
    float4x4 lightView;
    float4x4 lightProjection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<ShadowUniformData> ubo;

static const uint kMaxSkinJoints = 128;

struct BonePaletteData
{
    float4x4 jointMatrices[kMaxSkinJoints];
};

[[vk::binding(1, 0)]]
ConstantBuffer<BonePaletteData> bonePalette;

struct VertexInput
{
    [[vk::location(0)]]
    float3 inPosition;
    [[vk::location(1)]]
    float3 inNormal;
    [[vk::location(2)]]
    float2 inUv;
    [[vk::location(3)]]
    float4 inTangent;
    [[vk::location(4)]]
    int4 inJointIndices;
    [[vk::location(5)]]
    float4 inWeights;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
};

float4x4 blendSkinMatrix(float4 weights, int4 joints)
{
    float4x4 result = (float4x4)0;
    float weight_sum = weights.x + weights.y + weights.z + weights.w;
    if (weight_sum <= 1e-6)
    {
        return float4x4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1);
    }

    if (weights.x > 0.0 && joints.x >= 0 && joints.x < (int)kMaxSkinJoints)
    {
        result += bonePalette.jointMatrices[joints.x] * weights.x;
    }
    if (weights.y > 0.0 && joints.y >= 0 && joints.y < (int)kMaxSkinJoints)
    {
        result += bonePalette.jointMatrices[joints.y] * weights.y;
    }
    if (weights.z > 0.0 && joints.z >= 0 && joints.z < (int)kMaxSkinJoints)
    {
        result += bonePalette.jointMatrices[joints.z] * weights.z;
    }
    if (weights.w > 0.0 && joints.w >= 0 && joints.w < (int)kMaxSkinJoints)
    {
        result += bonePalette.jointMatrices[joints.w] * weights.w;
    }

    if (weight_sum < 0.999 || weight_sum > 1.001)
    {
        result /= weight_sum;
    }
    return result;
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4x4 skinMatrix = blendSkinMatrix(input.inWeights, input.inJointIndices);
    float4 skinnedPos = mul(skinMatrix, float4(input.inPosition, 1.0));
    float4 worldPos = mul(ubo.model, skinnedPos);
    float4 lightViewPos = mul(ubo.lightView, worldPos);
    output.sv_position = mul(ubo.lightProjection, lightViewPos);
    return output;
}

[shader("fragment")]
void fragmentMain(VertexOutput input)
{
}
