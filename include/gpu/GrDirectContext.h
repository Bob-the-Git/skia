/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrDirectContext_DEFINED
#define GrDirectContext_DEFINED

#include "include/private/GrContext.h"

class GrAtlasManager;
class GrSmallPathAtlasMgr;

class SK_API GrDirectContext : public GrContext {
public:
#ifdef SK_GL
    /**
     * Creates a GrDirectContext for a backend context. If no GrGLInterface is provided then the
     * result of GrGLMakeNativeInterface() is used if it succeeds.
     */
    static sk_sp<GrDirectContext> MakeGL(sk_sp<const GrGLInterface>, const GrContextOptions&);
    static sk_sp<GrDirectContext> MakeGL(sk_sp<const GrGLInterface>);
    static sk_sp<GrDirectContext> MakeGL(const GrContextOptions&);
    static sk_sp<GrDirectContext> MakeGL();
#endif

#ifdef SK_VULKAN
    /**
     * The Vulkan context (VkQueue, VkDevice, VkInstance) must be kept alive until the returned
     * GrDirectContext is destroyed. This also means that any objects created with this
     * GrDirectContext (e.g. SkSurfaces, SkImages, etc.) must also be released as they may hold
     * refs on the GrDirectContext. Once all these objects and the GrDirectContext are released,
     * then it is safe to delete the vulkan objects.
     */
    static sk_sp<GrDirectContext> MakeVulkan(const GrVkBackendContext&, const GrContextOptions&);
    static sk_sp<GrDirectContext> MakeVulkan(const GrVkBackendContext&);
#endif

#ifdef SK_METAL
    /**
     * Makes a GrDirectContext which uses Metal as the backend. The device parameter is an
     * MTLDevice and queue is an MTLCommandQueue which should be used by the backend. These objects
     * must have a ref on them which can be transferred to Ganesh which will release the ref
     * when the GrDirectContext is destroyed.
     */
    static sk_sp<GrDirectContext> MakeMetal(void* device, void* queue, const GrContextOptions&);
    static sk_sp<GrDirectContext> MakeMetal(void* device, void* queue);
#endif

#ifdef SK_DIRECT3D
    /**
     * Makes a GrDirectContext which uses Direct3D as the backend. The Direct3D context
     * must be kept alive until the returned GrDirectContext is first destroyed or abandoned.
     */
    static sk_sp<GrDirectContext> MakeDirect3D(const GrD3DBackendContext&, const GrContextOptions&);
    static sk_sp<GrDirectContext> MakeDirect3D(const GrD3DBackendContext&);
#endif

#ifdef SK_DAWN
    static sk_sp<GrDirectContext> MakeDawn(const wgpu::Device&,
                                           const GrContextOptions&);
    static sk_sp<GrDirectContext> MakeDawn(const wgpu::Device&);
#endif

    static sk_sp<GrDirectContext> MakeMock(const GrMockOptions*, const GrContextOptions&);
    static sk_sp<GrDirectContext> MakeMock(const GrMockOptions*);

    ~GrDirectContext() override;

    /**
     * The context normally assumes that no outsider is setting state
     * within the underlying 3D API's context/device/whatever. This call informs
     * the context that the state was modified and it should resend. Shouldn't
     * be called frequently for good performance.
     * The flag bits, state, is dependent on which backend is used by the
     * context, either GL or D3D (possible in future).
     */
    void resetContext(uint32_t state = kAll_GrBackendState);

    /**
     * If the backend is GrBackendApi::kOpenGL, then all texture unit/target combinations for which
     * the context has modified the bound texture will have texture id 0 bound. This does not
     * flush the context. Calling resetContext() does not change the set that will be bound
     * to texture id 0 on the next call to resetGLTextureBindings(). After this is called
     * all unit/target combinations are considered to have unmodified bindings until the context
     * subsequently modifies them (meaning if this is called twice in a row with no intervening
     * context usage then the second call is a no-op.)
     */
    void resetGLTextureBindings();

    /**
     * Abandons all GPU resources and assumes the underlying backend 3D API context is no longer
     * usable. Call this if you have lost the associated GPU context, and thus internal texture,
     * buffer, etc. references/IDs are now invalid. Calling this ensures that the destructors of the
     * GrContext and any of its created resource objects will not make backend 3D API calls. Content
     * rendered but not previously flushed may be lost. After this function is called all subsequent
     * calls on the GrContext will fail or be no-ops.
     *
     * The typical use case for this function is that the underlying 3D context was lost and further
     * API calls may crash.
     *
     * For Vulkan, even if the device becomes lost, the VkQueue, VkDevice, or VkInstance used to
     * create the context must be kept alive even after abandoning the context. Those objects must
     * live for the lifetime of the context object itself. The reason for this is so that
     * we can continue to delete any outstanding GrBackendTextures/RenderTargets which must be
     * cleaned up even in a device lost state.
     */
    void abandonContext() override;

    /**
     * Returns true if the context was abandoned or if the if the backend specific context has
     * gotten into an unrecoverarble, lost state (e.g. in Vulkan backend if we've gotten a
     * VK_ERROR_DEVICE_LOST). If the backend context is lost, this call will also abandon the
     * GrContext.
     */
    bool abandoned() override;

    // TODO: Remove this from public after migrating Chrome.
    sk_sp<GrContextThreadSafeProxy> threadSafeProxy();

    /**
     * Checks if the underlying 3D API reported an out-of-memory error. If this returns true it is
     * reset and will return false until another out-of-memory error is reported by the 3D API. If
     * the context is abandoned then this will report false.
     *
     * Currently this is implemented for:
     *
     * OpenGL [ES] - Note that client calls to glGetError() may swallow GL_OUT_OF_MEMORY errors and
     * therefore hide the error from Skia. Also, it is not advised to use this in combination with
     * enabling GrContextOptions::fSkipGLErrorChecks. That option may prevent the context from ever
     * checking the GL context for OOM.
     *
     * Vulkan - Reports true if VK_ERROR_OUT_OF_HOST_MEMORY or VK_ERROR_OUT_OF_DEVICE_MEMORY has
     * occurred.
     */
    bool oomed();

    /**
     * This is similar to abandonContext() however the underlying 3D context is not yet lost and
     * the context will cleanup all allocated resources before returning. After returning it will
     * assume that the underlying context may no longer be valid.
     *
     * The typical use case for this function is that the client is going to destroy the 3D context
     * but can't guarantee that context will be destroyed first (perhaps because it may be ref'ed
     * elsewhere by either the client or Skia objects).
     *
     * For Vulkan, even if the device becomes lost, the VkQueue, VkDevice, or VkInstance used to
     * create the context must be alive before calling releaseResourcesAndAbandonContext.
     */
    void releaseResourcesAndAbandonContext();

    ///////////////////////////////////////////////////////////////////////////
    // Resource Cache

    /** DEPRECATED
     *  Return the current GPU resource cache limits.
     *
     *  @param maxResources If non-null, will be set to -1.
     *  @param maxResourceBytes If non-null, returns maximum number of bytes of
     *                          video memory that can be held in the cache.
     */
    void getResourceCacheLimits(int* maxResources, size_t* maxResourceBytes) const;

    /**
     *  Return the current GPU resource cache limit in bytes.
     */
    size_t getResourceCacheLimit() const;

    /**
     *  Gets the current GPU resource cache usage.
     *
     *  @param resourceCount If non-null, returns the number of resources that are held in the
     *                       cache.
     *  @param maxResourceBytes If non-null, returns the total number of bytes of video memory held
     *                          in the cache.
     */
    void getResourceCacheUsage(int* resourceCount, size_t* resourceBytes) const;

    /**
     *  Gets the number of bytes in the cache consumed by purgeable (e.g. unlocked) resources.
     */
    size_t getResourceCachePurgeableBytes() const;

    /** DEPRECATED
     *  Specify the GPU resource cache limits. If the current cache exceeds the maxResourceBytes
     *  limit, it will be purged (LRU) to keep the cache within the limit.
     *
     *  @param maxResources Unused.
     *  @param maxResourceBytes The maximum number of bytes of video memory
     *                          that can be held in the cache.
     */
    void setResourceCacheLimits(int maxResources, size_t maxResourceBytes);

    /**
     *  Specify the GPU resource cache limit. If the cache currently exceeds this limit,
     *  it will be purged (LRU) to keep the cache within the limit.
     *
     *  @param maxResourceBytes The maximum number of bytes of video memory
     *                          that can be held in the cache.
     */
    void setResourceCacheLimit(size_t maxResourceBytes);

    /**
     * Frees GPU created by the context. Can be called to reduce GPU memory
     * pressure.
     */
    void freeGpuResources();

    /**
     * Purge GPU resources that haven't been used in the past 'msNotUsed' milliseconds or are
     * otherwise marked for deletion, regardless of whether the context is under budget.
     */
    void performDeferredCleanup(std::chrono::milliseconds msNotUsed);

    // Temporary compatibility API for Android.
    void purgeResourcesNotUsedInMs(std::chrono::milliseconds msNotUsed) {
        this->performDeferredCleanup(msNotUsed);
    }

    /**
     * Purge unlocked resources from the cache until the the provided byte count has been reached
     * or we have purged all unlocked resources. The default policy is to purge in LRU order, but
     * can be overridden to prefer purging scratch resources (in LRU order) prior to purging other
     * resource types.
     *
     * @param maxBytesToPurge the desired number of bytes to be purged.
     * @param preferScratchResources If true scratch resources will be purged prior to other
     *                               resource types.
     */
    void purgeUnlockedResources(size_t bytesToPurge, bool preferScratchResources);

    /**
     * This entry point is intended for instances where an app has been backgrounded or
     * suspended.
     * If 'scratchResourcesOnly' is true all unlocked scratch resources will be purged but the
     * unlocked resources with persistent data will remain. If 'scratchResourcesOnly' is false
     * then all unlocked resources will be purged.
     * In either case, after the unlocked resources are purged a separate pass will be made to
     * ensure that resource usage is under budget (i.e., even if 'scratchResourcesOnly' is true
     * some resources with persistent data may be purged to be under budget).
     *
     * @param scratchResourcesOnly   If true only unlocked scratch resources will be purged prior
     *                               enforcing the budget requirements.
     */
    void purgeUnlockedResources(bool scratchResourcesOnly);

    /**
     * Gets the maximum supported texture size.
     */
    using GrRecordingContext::maxTextureSize;

    /**
     * Gets the maximum supported render target size.
     */
    using GrRecordingContext::maxRenderTargetSize;

    /**
     * Can a SkImage be created with the given color type.
     */
    using GrRecordingContext::colorTypeSupportedAsImage;

    /**
     * Can a SkSurface be created with the given color type. To check whether MSAA is supported
     * use maxSurfaceSampleCountForColorType().
     */
    using GrRecordingContext::colorTypeSupportedAsSurface;

    /**
     * Gets the maximum supported sample count for a color type. 1 is returned if only non-MSAA
     * rendering is supported for the color type. 0 is returned if rendering to this color type
     * is not supported at all.
     */
    using GrRecordingContext::maxSurfaceSampleCountForColorType;

    ///////////////////////////////////////////////////////////////////////////
    // Misc.

    /**
     * Inserts a list of GPU semaphores that the current GPU-backed API must wait on before
     * executing any more commands on the GPU. If this call returns false, then the GPU back-end
     * will not wait on any passed in semaphores, and the client will still own the semaphores,
     * regardless of the value of deleteSemaphoresAfterWait.
     *
     * If deleteSemaphoresAfterWait is false then Skia will not delete the semaphores. In this case
     * it is the client's responsibility to not destroy or attempt to reuse the semaphores until it
     * knows that Skia has finished waiting on them. This can be done by using finishedProcs on
     * flush calls.
     */
    bool wait(int numSemaphores, const GrBackendSemaphore* waitSemaphores,
              bool deleteSemaphoresAfterWait = true);

    /**
     * Call to ensure all drawing to the context has been flushed and submitted to the underlying 3D
     * API. This is equivalent to calling GrContext::flush with a default GrFlushInfo followed by
     * GrContext::submit(syncCpu).
     */
    void flushAndSubmit(bool syncCpu = false) {
        this->flush(GrFlushInfo());
        this->submit(syncCpu);
    }

    /**
     * Call to ensure all drawing to the context has been flushed to underlying 3D API specific
     * objects. A call to `submit` is always required to ensure work is actually sent to
     * the gpu. Some specific API details:
     *     GL: Commands are actually sent to the driver, but glFlush is never called. Thus some
     *         sync objects from the flush will not be valid until a submission occurs.
     *
     *     Vulkan/Metal/D3D/Dawn: Commands are recorded to the backend APIs corresponding command
     *         buffer or encoder objects. However, these objects are not sent to the gpu until a
     *         submission occurs.
     *
     * If the return is GrSemaphoresSubmitted::kYes, only initialized GrBackendSemaphores will be
     * submitted to the gpu during the next submit call (it is possible Skia failed to create a
     * subset of the semaphores). The client should not wait on these semaphores until after submit
     * has been called, and must keep them alive until then. If this call returns
     * GrSemaphoresSubmitted::kNo, the GPU backend will not submit any semaphores to be signaled on
     * the GPU. Thus the client should not have the GPU wait on any of the semaphores passed in with
     * the GrFlushInfo. Regardless of whether semaphores were submitted to the GPU or not, the
     * client is still responsible for deleting any initialized semaphores.
     * Regardleess of semaphore submission the context will still be flushed. It should be
     * emphasized that a return value of GrSemaphoresSubmitted::kNo does not mean the flush did not
     * happen. It simply means there were no semaphores submitted to the GPU. A caller should only
     * take this as a failure if they passed in semaphores to be submitted.
     */
    GrSemaphoresSubmitted flush(const GrFlushInfo& info);

    void flush() { this->flush({}); }

    /**
     * Submit outstanding work to the gpu from all previously un-submitted flushes. The return
     * value of the submit will indicate whether or not the submission to the GPU was successful.
     *
     * If the call returns true, all previously passed in semaphores in flush calls will have been
     * submitted to the GPU and they can safely be waited on. The caller should wait on those
     * semaphores or perform some other global synchronization before deleting the semaphores.
     *
     * If it returns false, then those same semaphores will not have been submitted and we will not
     * try to submit them again. The caller is free to delete the semaphores at any time.
     *
     * If the syncCpu flag is true this function will return once the gpu has finished with all
     * submitted work.
     */
    bool submit(bool syncCpu = false);

    /**
     * Checks whether any asynchronous work is complete and if so calls related callbacks.
     */
    void checkAsyncWorkCompletion();

    /** Enumerates all cached GPU resources and dumps their memory to traceMemoryDump. */
    // Chrome is using this!
    void dumpMemoryStatistics(SkTraceMemoryDump* traceMemoryDump) const;

    bool supportsDistanceFieldText() const;

    void storeVkPipelineCacheData();

    // Returns the gpu memory size of the the texture that backs the passed in SkImage. Returns 0 if
    // the SkImage is not texture backed. For external format textures this will also return 0 as we
    // cannot determine the correct size.
    static size_t ComputeImageSize(sk_sp<SkImage> image, GrMipmapped, bool useNextPow2 = false);

    /**
     * Retrieve the default GrBackendFormat for a given SkColorType and renderability.
     * It is guaranteed that this backend format will be the one used by the following
     * SkColorType and SkSurfaceCharacterization-based createBackendTexture methods.
     *
     * The caller should check that the returned format is valid.
     */
    GrBackendFormat defaultBackendFormat(SkColorType ct, GrRenderable renderable) const {
        return INHERITED::defaultBackendFormat(ct, renderable);
    }
protected:
    GrDirectContext(GrBackendApi backend, const GrContextOptions& options);

    bool init() override;

    GrAtlasManager* onGetAtlasManager() override { return fAtlasManager.get(); }
    GrSmallPathAtlasMgr* onGetSmallPathAtlasMgr() override;

    GrDirectContext* asDirectContext() override { return this; }

private:
    std::unique_ptr<GrAtlasManager> fAtlasManager;

    std::unique_ptr<GrSmallPathAtlasMgr> fSmallPathAtlasMgr;

    using INHERITED = GrContext;
};


#endif
