struct GridUniformData
{
    float4x4 view;
    float4x4 projection;
    float4 cameraPositionAndProjType;
    float4 cameraForwardAndFarClip;
    float4 planeOrigin;
    float4 planeAxisU;
    float4 planeAxisV;
    float4 params0; // step, majorScale, halfExtent, lineCountPerAxis
    float4 params1; // edgeFade, angleFade, farFade, stippleScale
    float4 params2; // stippleDuty, alphaScale, iteration, reserved
};

[[vk::binding(0, 0)]]
ConstantBuffer<GridUniformData> ubo;

struct VertexOutput
{
    float4 sv_position : SV_Position;
    [[vk::location(0)]] float3 worldPos;
    [[vk::location(1)]] float lineCoord;
    [[vk::location(2)]] float alphaWeight;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    uint lineId = vertexId / 2;
    uint endpointId = vertexId & 1;
    uint lineCount = max(1u, (uint)ubo.params0.w);
    uint axisId = lineId / lineCount;
    uint localLineId = lineId - axisId * lineCount;

    float halfExtent = ubo.params0.z;
    float step = ubo.params0.x;
    float lineOffset = (float(localLineId) - float(lineCount - 1) * 0.5) * step;
    float endpoint = endpointId == 0 ? -halfExtent : halfExtent;

    float3 axisU = ubo.planeAxisU.xyz;
    float3 axisV = ubo.planeAxisV.xyz;
    float3 world = ubo.planeOrigin.xyz;
    if (axisId == 0)
    {
        world += axisU * endpoint + axisV * lineOffset;
    }
    else
    {
        world += axisU * lineOffset + axisV * endpoint;
    }

    float4 viewPos = mul(ubo.view, float4(world, 1.0));
    output.sv_position = mul(ubo.projection, viewPos);
    output.worldPos = world;
    output.lineCoord = endpoint;
    output.alphaWeight = (abs(lineOffset) < 0.5 * step) ? 1.0 : 0.7;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float3 cameraPos = ubo.cameraPositionAndProjType.xyz;
    float3 cameraForward = normalize(ubo.cameraForwardAndFarClip.xyz);
    float farClip = max(ubo.cameraForwardAndFarClip.w, 0.001);

    float3 toPixel = input.worldPos - cameraPos;
    float distance = length(toPixel);
    float edgeFade = saturate(1.0 - abs(input.lineCoord) / max(ubo.params0.z, 0.001));

    float angleDot = abs(dot(normalize(toPixel), cameraForward));
    float angleFade = saturate(pow(angleDot, ubo.params1.y));

    float distanceFade = saturate(1.0 - distance / farClip);
    distanceFade = pow(distanceFade, ubo.params1.z);

    float stipplePhase = frac(distance * ubo.params1.w);
    float stippleMask = step(stipplePhase, saturate(ubo.params2.x));

    float alpha = edgeFade * angleFade * distanceFade * stippleMask;
    alpha *= ubo.params2.y * input.alphaWeight;

    return float4(0.68, 0.73, 0.80, alpha);
}
