struct ShadowSamplingUniform
{
    float4 vsmOriginLevels;
    float4 vsmParams;
    float4x4 vsmLightView;
    float4x4 spotViewProjections[8];
};
