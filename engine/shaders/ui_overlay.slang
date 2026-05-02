[[vk::binding(0, 0)]]
Texture2D<float4> uiTex;

[[vk::binding(1, 0)]]
SamplerState uiSamp;

struct VsOut
{
    float4 pos : SV_Position;
    [[vk::location(0)]]
    float2 uv;
};

[shader("vertex")]
VsOut uiVertexMain(uint vid : SV_VertexID)
{
    VsOut o;
    float x = (vid == 2) ? 3.0 : -1.0;
    float y = (vid == 0) ? -3.0 : 1.0;
    o.pos = float4(x, y, 0.0, 1.0);
    o.uv = float2((x + 1.0) * 0.5, (y + 1.0) * 0.5);
    o.uv.y = 1.0 - o.uv.y;
    return o;
}

[shader("fragment")]
float4 uiFragmentMain(VsOut input) : SV_Target
{
    // Slint 位图为左上原点；与当前全屏三角插值组合后整体旋转了 180°，对 UV 同时翻转 X/Y 对齐
    float2 uv = float2(1.0 - input.uv.x, 1.0 - input.uv.y);
    float4 c = uiTex.Sample(uiSamp, uv);
    return c;
}
