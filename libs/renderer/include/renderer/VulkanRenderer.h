#pragma once

#include <Windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

// VMA's own header is included in the .cpp with VMA_IMPLEMENTATION; here we
// only need the opaque VmaAllocator/VmaAllocation typedefs, which the VMA
// header provides even without VMA_IMPLEMENTATION defined.
#include <vk_mem_alloc.h>

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "assets/DdsImage.h"
#include "assets/StaticMesh.h"
#include "terrain/TerrainMesh.h"

namespace renderer {

// An uploaded static mesh (immutable vertex/index buffers) - returned by
// loadStaticMesh(), passed back into drawMesh(). Callers should cache this
// themselves (see runVisualizer()'s mesh cache) rather than re-upload the
// same geometry every frame. Vulkan has no ComPtr-style automatic reference
// counting, so this struct owns its buffers' lifetime explicitly via
// VulkanRenderer::destroyMesh() - callers holding a MeshHandle past the
// VulkanRenderer's own lifetime is a use-after-free, same caution as the
// D3D11 version's ComPtr-owned buffers being tied to the device.
struct MeshHandle {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation indexAllocation = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
    // A real per-submesh texture's descriptor set (see loadTexture()) -
    // VK_NULL_HANDLE means untextured, in which case drawMesh() binds a
    // shared 1x1 white fallback instead of leaving the pipeline's texture
    // descriptor set unbound (Vulkan requires every set the pipeline layout
    // declares to be bound at draw time). Callers set this directly after
    // both loadStaticMesh() and loadTexture() succeed - plain field, not a
    // constructor parameter, since texture resolution is a separate,
    // independently-fallible step from mesh geometry upload.
    VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;
};

// A dynamically-updatable mesh (added Phase 21, animation) - unlike
// MeshHandle (device-local, uploaded once via loadStaticMesh(), immutable
// afterward), this is host-visible and persistently mapped, meant to be
// rewritten via updateDynamicMesh() every frame its contents change (e.g.
// a skinned character's CPU-skinned pose). Reuses the exact same mesh
// pipeline/shader/vertex layout as MeshHandle/drawMesh() - only the upload
// strategy differs, so no new shader or pipeline object is needed. Sized
// once at allocateDynamicMesh() time to a caller-chosen max vertex/index
// count (a real submesh's own counts, known ahead of time from its
// resolved assets::SkeletalMeshSubmesh); updateDynamicMesh() may write
// fewer than the max (it does not resize the mesh's shape, only its
// current vertex/index count changes are never expected in practice since
// a submesh's own vertex/index count is fixed once resolved - callers are
// still expected to pass the same counts every update).
struct DynamicMeshHandle {
    std::vector<VkBuffer> vertexBuffers;           // one per frame-in-flight
    std::vector<VmaAllocation> vertexAllocations;
    std::vector<void*> vertexMapped;
    size_t maxVertices = 0;

    std::vector<VkBuffer> indexBuffers;             // one per frame-in-flight
    std::vector<VmaAllocation> indexAllocations;
    std::vector<void*> indexMapped;
    size_t maxIndices = 0;

    uint32_t currentIndexCount = 0;
    VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE; // same meaning as MeshHandle's own
};

// An uploaded label text texture (see createTextTexture()) plus its
// pre-bound combined-image-sampler descriptor set, ready to pass straight
// into drawLabel(). Replaces the D3D11 version's
// Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> - Vulkan's equivalent
// (image + view + descriptor) has no single-handle automatic-refcounting
// type, so this project exposes its own small opaque handle instead,
// mirroring MeshHandle's existing pattern.
struct TextureHandle {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

// An uploaded terrain chunk mesh - same ownership shape as MeshHandle
// (immutable vertex/index buffers, VulkanRenderer owns and frees every one
// it creates). Kept as its own type rather than reusing MeshHandle since
// terrain chunks are drawn with a different pipeline/vertex layout
// (position+normal+color, no UV, no per-draw model matrix - see
// Terrain.hlsl).
struct TerrainChunkHandle {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation indexAllocation = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
};

// Vulkan port of the D3D11 crude-visualizer renderer (see PLAN.md's Vulkan
// port plan / DISCOVERY.txt's "DECIDED 2026-07-18" entry) - same public API
// surface as the retired D3D11Renderer, same visual feature set (wireframe
// boxes, ground grid, wall cage, top-down minimap, GDI text labels,
// solid-fill real-mesh rendering), different backend. No new rendering
// capability was added in this port - it is a backend swap, not a feature
// change.
class VulkanRenderer {
public:
    VulkanRenderer(HWND hwnd, int width, int height);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    void beginFrame(float clearR, float clearG, float clearB);
    void setViewProjection(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);

    void drawWireBox(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& halfExtents,
                      float yawRadians, const DirectX::XMFLOAT4& color);
    void drawCircle(const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT4& color);
    void beginMinimapPass(int x, int y, int width, int height);

    TextureHandle createTextTexture(const std::wstring& text, int& outPixelWidth,
                                     int& outPixelHeight);
    void beginLabelPass();
    void drawLabel(const DirectX::XMFLOAT3& worldCenter, float width, float height,
                    const DirectX::XMFLOAT3& right, const DirectX::XMFLOAT3& up,
                    const TextureHandle& texture);

    MeshHandle loadStaticMesh(const assets::MeshData& mesh);
    void drawMesh(const MeshHandle& handle, const DirectX::XMFLOAT3& center, float yawRadians,
                  const DirectX::XMFLOAT4& color);

    // Allocates a DynamicMeshHandle's GPU buffers, sized for up to
    // `maxVertices`/`maxIndices` - see DynamicMeshHandle's own comment.
    // Contents are undefined until the first updateDynamicMesh() call.
    DynamicMeshHandle allocateDynamicMesh(size_t maxVertices, size_t maxIndices);

    // Rewrites this frame-in-flight's copy of `handle`'s vertex/index data
    // (see DynamicMeshHandle's own comment on why there's one copy per
    // frame-in-flight rather than a single shared buffer) - call once per
    // frame, right before drawDynamicMesh(), whenever the mesh's pose has
    // changed. `mesh.positions.size()`/`mesh.indices.size()` must not
    // exceed the handle's own maxVertices/maxIndices.
    void updateDynamicMesh(DynamicMeshHandle& handle, const assets::MeshData& mesh);

    // Same draw semantics as drawMesh(), against whichever frame-in-flight
    // copy updateDynamicMesh() last wrote this frame.
    void drawDynamicMesh(const DynamicMeshHandle& handle, const DirectX::XMFLOAT3& center,
                          float yawRadians, const DirectX::XMFLOAT4& color);

    // Frees a DynamicMeshHandle's GPU buffers - unlike MeshHandle (a bounded
    // set of template meshes, kept for the renderer's whole lifetime, see
    // ownedMeshes_), dynamic meshes are per-VISIBLE-OBJECT and callers are
    // expected to free one as soon as its owning object leaves view. Safe to
    // call at most once per handle.
    void destroyDynamicMesh(DynamicMeshHandle& handle);

    // Uploads a real, already-decoded compressed texture (see
    // assets::DdsImage) - REPEAT-addressed (tiled), unlike label textures'
    // CLAMP sampler, since real mesh UVs commonly exceed [0,1] to tile a
    // small real texture across a large real surface. Returned handle's
    // `descriptorSet` is assignable directly to a MeshHandle's own
    // `textureDescriptorSet` field - both are allocated against the same
    // shared textureDescriptorSetLayout_ (see its own comment).
    TextureHandle loadTexture(const assets::DdsImageData& dds);

    // Terrain chunk vertices are already baked in absolute world space
    // (see terrain::buildChunkMesh()), so unlike drawMesh() there's no
    // center/yaw/color parameter - just the handle.
    TerrainChunkHandle loadTerrainChunk(const terrain::TerrainMeshData& mesh);
    void drawTerrainChunk(const TerrainChunkHandle& handle);

    // Unlike mesh/texture handles (a bounded set of object templates, kept
    // for the renderer's whole lifetime - see ownedTextures_'s comment),
    // terrain chunks stream in/out unboundedly as a player roams a planet,
    // so callers are expected to free a chunk's GPU buffers as soon as
    // terrain::TerrainChunkManager evicts it, rather than let them
    // accumulate for the process lifetime. Safe to call at most once per
    // handle; the handle is invalid afterward.
    void unloadTerrainChunk(const TerrainChunkHandle& handle);

    void endFrame();

    // Diagnostic capture aid, added for live visual comparison work (walk
    // animation debugging) - reads back the swapchain image that was JUST
    // presented by the most recent endFrame() call and returns it as tightly
    // packed, top-to-bottom RGBA8 (converted from the swapchain's native
    // BGRA8 order). Must be called after endFrame() and before the next
    // beginFrame() - relies on the swapchain image at currentImageIndex_
    // still holding this frame's contents and nothing yet queued to
    // overwrite it (true in that window because the render pass's own
    // color attachment uses initialLayout=UNDEFINED, i.e. "discard whatever
    // was there" - this capture doesn't need to leave the image in any
    // particular layout afterward). Does a full GPU wait-idle internally
    // (via endSingleTimeCommands()) - fine for an occasional debug capture,
    // never called in the normal per-frame path.
    bool captureFrameRGBA8(std::vector<uint8_t>& outRGBA, uint32_t& outWidth,
                            uint32_t& outHeight) const;

private:
    static constexpr int kMaxFramesInFlight = 2;

    // ---- Phase 1: instance/device/swapchain/depth/render pass/sync ----
    void createInstance();
    void createSurface(HWND hwnd);
    void selectPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createSwapchainImageViews();
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();

    // Real, previously-unhandled Vulkan condition (Phase 19 live crash,
    // VkResult=-1000001004 = VK_ERROR_OUT_OF_DATE_KHR): the swapchain
    // becomes invalid for its current size whenever the window is resized,
    // minimized/restored, moved to a different-DPI monitor, or the display
    // mode changes - previously fell through to vkCheck() and crashed the
    // whole process instead of recovering, since window-resize handling had
    // never been implemented. Destroys and recreates every swapchain-sized
    // resource (image views, depth buffer, framebuffers, the swapchain
    // itself) at the surface's own current actual size - NOT the
    // constructor's original width_/height_, which may be stale. The render
    // pass itself is untouched (its format doesn't change on a resize, only
    // extent does, so recreating it isn't needed). A no-op if the surface is
    // currently zero-sized (a fully minimized window) - nothing sane to
    // create yet; the caller retries until this becomes real again.
    void recreateSwapchain();
    void createCommandPoolAndBuffers();
    void createSyncObjects();
    void createAllocator();

    // ---- Later phases: pipelines/geometry, filled in incrementally ----
    void createWireframePipeline();
    void createMinimapPipeline();
    void createLabelPipeline();
    void createMeshPipelineObjects();
    void createTerrainPipelineObjects();
    void createFixedGeometry(); // box/grid/wall/circle vertex buffers

    VkShaderModule loadShaderModule(const char* spirvFileBaseName, const char* entryPoint) const;

    // One-time immediate-mode command buffer (allocated from commandPool_,
    // submitted to graphicsQueue_, waited on synchronously) - used only
    // during setup (fixed-geometry/mesh uploads via a staging buffer), never
    // in the per-frame draw path.
    VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer cmd) const;

    // Uploads `data` (size bytes) into a new device-local buffer with the
    // given usage flags, via a temporary host-visible staging buffer and a
    // one-time copy command. Used for all "upload once, never change again"
    // geometry (fixed box/grid/wall/circle meshes, per-loaded-mesh vertex/
    // index buffers) - mirrors the D3D11 pass' D3D11_USAGE_IMMUTABLE buffers.
    void uploadDeviceLocalBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                                  VkBuffer& outBuffer, VmaAllocation& outAllocation) const;

    // Records a full-resource pipeline barrier transitioning `image` between
    // the two given layouts onto an ALREADY-OPEN command buffer (unlike the
    // retired transitionImageLayout(), this doesn't allocate/submit its own -
    // callers compose it with other recorded commands into one submission,
    // see createTextTexture()). Only supports the two specific transitions
    // this renderer actually needs, not a general-purpose barrier helper.
    void recordImageLayoutBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout,
                                   VkImageLayout newLayout) const;

    // The real "upload raw pixel/block bytes as a sampled VkImage + view +
    // descriptor set" sequence, factored out of createTextTexture() so
    // loadTexture() (real compressed mesh textures) can reuse it instead of
    // re-deriving the same staging-buffer/barrier/descriptor-set
    // boilerplate a second time. `format` may be an uncompressed format
    // (createTextTexture's GDI-rasterized RGBA8) or a real BC1/BC2/BC3
    // block-compressed one (loadTexture()) - Vulkan treats both uniformly
    // here, the only difference is which bytes/format get passed in.
    TextureHandle uploadTextureImage(uint32_t width, uint32_t height, VkFormat format,
                                      const void* pixelData, size_t dataSize, VkSampler sampler);

    // Phase 14 (engine loop redesign) - the async counterpart to
    // uploadDeviceLocalBuffer(). Submits with a fence and returns
    // immediately, WITHOUT waiting - unlike the sync version's
    // vkQueueWaitIdle (fine for one-time startup geometry in
    // createFixedGeometry(), but a real per-frame stall once terrain/mesh/
    // label streaming made this a hot path, not just a setup-time call -
    // see GAP_ANALYSIS.md section 0.4). Tracks the pending work in
    // pendingUploads_, polled once per frame by pollPendingUploads().
    // `ownerKey` identifies which logical resource this upload belongs to
    // (a MeshHandle/TerrainChunkHandle's own vertexBuffer, or a
    // TextureHandle's own image, cast to uint64_t as a stable opaque
    // identity - Vulkan non-dispatchable handles are uint64_t-typed on the
    // 64-bit builds this project targets) - a resource with more than one
    // pending upload (e.g. a mesh's vertex+index buffers) isn't marked
    // ready until every upload sharing its ownerKey completes. `cmd` is
    // already recorded and ended by the caller (composing multiple commands
    // - e.g. a texture's barrier+copy+barrier - into one submission where
    // needed); this function only submits it and tracks completion.
    void submitAsyncUpload(VkCommandBuffer cmd, VkBuffer stagingBuffer,
                            VmaAllocation stagingAllocation, uint64_t ownerKey);

    // Convenience wrapper around submitAsyncUpload() for the common single-
    // buffer-copy case (loadStaticMesh()/loadTerrainChunk()'s vertex/index
    // buffers) - creates the device-local buffer, records+submits the
    // staging copy asynchronously.
    void uploadDeviceLocalBufferAsync(const void* data, VkDeviceSize size,
                                       VkBufferUsageFlags usage, VkBuffer& outBuffer,
                                       VmaAllocation& outAllocation, uint64_t ownerKey);

    // Non-blocking - checks every pending async upload's fence
    // (vkGetFenceStatus, never waits) and cleans up/marks-ready whatever
    // has completed. Called once per frame from beginFrame().
    void pollPendingUploads();

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily_ = 0;
    uint32_t presentQueueFamily_ = 0;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;

    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VmaAllocation depthImageAllocation_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_; // one per frame-in-flight

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    size_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;
    VkCommandBuffer currentCmd_ = VK_NULL_HANDLE; // cached each beginFrame(), used by draw calls

    // ---- Wireframe pipeline (boxes/grid/walls/circle/minimap) ----
    VkPipelineLayout wireframePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline wireframePipeline_ = VK_NULL_HANDLE;    // depth test + write
    VkPipeline minimapPipeline_ = VK_NULL_HANDLE;      // no depth test, same shaders/layout

    VkBuffer boxVertexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation boxVertexAllocation_ = VK_NULL_HANDLE;
    VkBuffer boxIndexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation boxIndexAllocation_ = VK_NULL_HANDLE;

    VkBuffer circleVertexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation circleVertexAllocation_ = VK_NULL_HANDLE;
    uint32_t circleVertexCount_ = 0;

    // ---- Label pipeline (billboarded text quads) ----
    // textureDescriptorSetLayout_ (one combined-image-sampler binding,
    // fragment-visible) is shared by both the label pipeline (GDI-rendered
    // text) AND the mesh pipeline's real per-submesh textures below - same
    // binding shape, just a different physical VkSampler/VkImageView baked
    // into each descriptor set at creation time, not a different layout.
    VkDescriptorSetLayout textureDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout labelPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline labelPipeline_ = VK_NULL_HANDLE;
    VkSampler labelSampler_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE; // shared: label + mesh textures + mesh UBO sets

    // Per-frame-in-flight host-visible scratch buffer for drawLabel()'s
    // dynamic quad vertices - reset (offset back to 0) at the start of each
    // beginFrame(), written to at an incrementing offset by each drawLabel()
    // call, mirroring the D3D11 dynamic-buffer-Map-per-draw pattern but
    // sized once per frame instead of re-mapped every call.
    std::vector<VkBuffer> labelVertexBuffers_;
    std::vector<VmaAllocation> labelVertexAllocations_;
    std::vector<void*> labelVertexMapped_;
    VkDeviceSize labelVertexBufferOffset_ = 0;

    // Every TextureHandle createTextTexture() has ever returned - Vulkan has
    // no ComPtr-style automatic reference counting (unlike the D3D11 pass'
    // ID3D11ShaderResourceView), so this renderer takes ownership of every
    // texture it creates and frees them all in ~VulkanRenderer() rather than
    // pushing that responsibility onto callers (tools/dummyclient's
    // labelCache just holds copies of the handle, same as it held ComPtrs
    // before - it never needs to free anything itself).
    std::vector<TextureHandle> ownedTextures_;

    // ---- Mesh pipeline (solid-fill real geometry) ----
    VkDescriptorSetLayout meshDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout meshPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline meshPipeline_ = VK_NULL_HANDLE;

    // Per-frame-in-flight host-visible uniform buffer for drawMesh()'s
    // per-draw constants (worldViewProj/world/color = 144 bytes, over the
    // guaranteed push-constant budget - see Mesh.hlsl's comment) - one
    // descriptor set per frame-in-flight, rewritten at an incrementing
    // offset per drawMesh() call this frame, same reasoning as the label
    // vertex buffers above.
    std::vector<VkBuffer> meshUniformBuffers_;
    std::vector<VmaAllocation> meshUniformAllocations_;
    std::vector<void*> meshUniformMapped_;
    std::vector<VkDescriptorSet> meshUniformDescriptorSets_;
    VkDeviceSize meshUniformOffset_ = 0;
    VkDeviceSize meshUniformAlignment_ = 0; // minUniformBufferOffsetAlignment, queried at init

    // Every MeshHandle loadStaticMesh() has ever returned - same ownership
    // reasoning as ownedTextures_ above (RealMeshResolver's meshCache_ in
    // tools/dummyclient just holds copies, never frees anything itself).
    std::vector<MeshHandle> ownedMeshes_;

    // REPEAT-addressed (see loadTexture()'s own comment for why, unlike
    // labelSampler_'s CLAMP), created once in createMeshPipelineObjects().
    VkSampler meshTextureSampler_ = VK_NULL_HANDLE;
    // A real 1x1 opaque-white texture, bound whenever a MeshHandle's own
    // textureDescriptorSet is VK_NULL_HANDLE (untextured, or texture
    // resolution failed) - Vulkan requires every descriptor set the
    // pipeline layout declares to be bound at draw time, so this is the
    // "no real texture" case rather than a special shader code path;
    // sampling opaque white and multiplying by the existing flat `color`
    // reproduces today's untextured behavior exactly.
    TextureHandle whiteFallbackTexture_;

    // ---- Terrain pipeline (vertex-colored chunk meshes) ----
    // Push-constant based like the wireframe pipeline, not UBO-based like
    // the mesh pipeline - see Terrain.hlsl's own comment for why.
    VkPipelineLayout terrainPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline terrainPipeline_ = VK_NULL_HANDLE;
    std::vector<TerrainChunkHandle> ownedTerrainChunks_; // same ownership reasoning as ownedMeshes_

    // Phase 14 (engine loop redesign) - async GPU upload tracking. A
    // resource (identified by its own vertexBuffer/image, see
    // submitAsyncUpload()'s comment) stays in pendingNotReady_ from the
    // moment loadStaticMesh()/loadTerrainChunk()/createTextTexture()
    // registers it until pollPendingUploads() confirms every upload
    // sharing its ownerKey has completed on the GPU - drawMesh()/
    // drawTerrainChunk()/drawLabel() check this set and silently skip
    // drawing that one resource (not an error - just not ready yet,
    // typically 1-2 frames) rather than the caller needing to track
    // readiness itself.
    struct PendingUpload {
        VkFence fence = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VmaAllocation stagingAllocation = VK_NULL_HANDLE;
        uint64_t ownerKey = 0;
    };
    std::vector<PendingUpload> pendingUploads_;
    std::unordered_set<uint64_t> pendingNotReady_;

    DirectX::XMMATRIX viewProjection_ = DirectX::XMMatrixIdentity();

    int width_ = 0;
    int height_ = 0;
};

} // namespace renderer
