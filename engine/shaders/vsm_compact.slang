struct CompactUniform
{
    uint virtualPages;
    uint physicalPages;
    uint meshletGroups;
    uint pad;
};

struct MarkedPage
{
    uint virtualIndex;
    uint physicalIndex;
    uint level;
    uint pageXY; // x in low 16, y in high 16
};

struct CompactState
{
    uint assigned;
    uint overflow;
    uint indirectX;
    uint indirectY;
    uint indirectZ;
    uint pad0;
    uint pad1;
    uint pad2;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CompactUniform> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<uint> pageFlags;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> pageTable;

[[vk::binding(3, 0)]]
RWStructuredBuffer<MarkedPage> markedPages;

[[vk::binding(4, 0)]]
RWStructuredBuffer<CompactState> state;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint virt = dtid.x;
    if (virt >= ubo.virtualPages)
    {
        return;
    }
    if (pageFlags[virt] == 0u)
    {
        pageTable[virt] = 0xFFFFFFFFu;
        return;
    }
    uint slot = 0;
    InterlockedAdd(state[0].assigned, 1u, slot);
    if (slot >= ubo.physicalPages)
    {
        pageTable[virt] = 0xFFFFFFFFu;
        InterlockedAdd(state[0].overflow, 1u);
        return;
    }
    pageTable[virt] = slot;
    MarkedPage page;
    page.virtualIndex = virt;
    page.physicalIndex = slot;
    uint pagesPerLevel = 128u * 128u;
    page.level = 6u + virt / pagesPerLevel;
    uint local = virt % pagesPerLevel;
    uint pageX = local % 128u;
    uint pageY = local / 128u;
    page.pageXY = pageX | (pageY << 16u);
    markedPages[slot] = page;
}
