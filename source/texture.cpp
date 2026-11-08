#include "texture.hpp"
#include "graphics_internal.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <random>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace texture {

namespace {

void transitionImage(VkImage image, VkImageLayout from, VkImageLayout to)
{
    auto& ctx = graphics::internal::context;

    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = ctx.graphics_queue_index;
    VkCommandPool pool;
    vkCreateCommandPool(ctx.device, &cpci, nullptr, &pool);

    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx.device, &cbai, &cmd);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkPipelineStageFlags srcStage, dstStage;
    if (from == VK_IMAGE_LAYOUT_UNDEFINED) {
        barrier.srcAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    } else if (from == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    if (to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphics_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.graphics_queue);

    vkDestroyCommandPool(ctx.device, pool, nullptr);
}

void createSampler(Texture& t, bool srgb)
{
    (void)srgb;
    auto& ctx = graphics::internal::context;
    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    
    sci.anisotropyEnable = VK_FALSE; 
    sci.maxAnisotropy = 1.0f;
    
    sci.compareEnable = VK_FALSE;
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sci.unnormalizedCoordinates = VK_FALSE;
    vkCreateSampler(ctx.device, &sci, nullptr, &t.sampler);
}

bool uploadPixels(const uint8_t* pixels, uint32_t w, uint32_t h, Texture& out, VkFormat format)
{
    auto& ctx = graphics::internal::context;

    out.width = w; out.height = h;

    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent = {w, h, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    if (vmaCreateImage(ctx.allocator, &ici, &aci, &out.image, &out.alloc, nullptr) != VK_SUCCESS)
        return false;

    VkDeviceSize size = VkDeviceSize(w) * h * 4;
    VkBuffer staging; VmaAllocation stagingAlloc; void* mapped;
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo sbai{};
    sbai.usage = VMA_MEMORY_USAGE_AUTO;
    sbai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo info{};
    vmaCreateBuffer(ctx.allocator, &bci, &sbai, &staging, &stagingAlloc, &info);
    mapped = info.pMappedData;
    std::memcpy(mapped, pixels, size);

    transitionImage(out.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = ctx.graphics_queue_index;
    VkCommandPool pool;
    vkCreateCommandPool(ctx.device, &cpci, nullptr, &pool);
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx.device, &cbai, &cmd);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {w, h, 1};
    vkCmdCopyBufferToImage(cmd, staging, out.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphics_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.graphics_queue);

    transitionImage(out.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyCommandPool(ctx.device, pool, nullptr);
    vmaDestroyBuffer(ctx.allocator, staging, stagingAlloc);

    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = out.image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = format;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(ctx.device, &vci, nullptr, &out.view);

    createSampler(out, format == VK_FORMAT_R8G8B8A8_SRGB);
    return true;
}

} // namespace

bool loadFromFile(const std::string& path, Texture& out, VkFormat format)
{
    int w, h, ch;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return false;
    }
    bool ok = uploadPixels(data, (uint32_t)w, (uint32_t)h, out, format);
    stbi_image_free(data);
    return ok;
}

bool createFromPixels(const uint8_t* pixels, uint32_t w, uint32_t h, Texture& out, VkFormat format) {
    return uploadPixels(pixels, w, h, out, format);
}

Texture makeSolid1x1(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Texture t{};
    uint8_t px[4] = {r, g, b, a};
    createFromPixels(px, 1, 1, t, VK_FORMAT_R8G8B8A8_UNORM);
    return t;
}

void destroy(Texture& t) {
    auto& ctx = graphics::internal::context;
    if (t.sampler) vkDestroySampler(ctx.device, t.sampler, nullptr);
    if (t.view)    vkDestroyImageView(ctx.device, t.view, nullptr);
    if (t.image)   vmaDestroyImage(ctx.allocator, t.image, t.alloc);
    t = {};
}


namespace {
float fade(float t)
{
    return t*t*t*(t*(t*6.0f-15.0f)+10.0f);
}

float lerp(float a, float b, float t)
{
    return a + t*(b-a);
}

float grad(int hash, float x, float y)
{
    switch(hash & 3) {
        case 0: return  x + y;
        case 1: return -x + y;
        case 2: return  x - y;
        default:return -x - y;
    }
}

float perlin(float x, float y)
{
    int xi = (int)std::floor(x) & 255;
    int yi = (int)std::floor(y) & 255;
    float xf = x - std::floor(x);
    float yf = y - std::floor(y);
    float u = fade(xf), v = fade(yf);
    auto h = [](int a, int b){ return ((a*374761393 + b*668265263) ^ (a*1274126177)) & 255; };
    int aa = h(xi,   yi);
    int ab = h(xi,   yi+1);
    int ba = h(xi+1, yi);
    int bb = h(xi+1, yi+1);
    float x1 = lerp(grad(aa, xf, yf),   grad(ba, xf-1, yf),   u);
    float x2 = lerp(grad(ab, xf, yf-1), grad(bb, xf-1, yf-1), u);
    return lerp(x1, x2, v);
}
}

void generatePerlinEmissive(uint32_t size, Texture& out) {
    std::vector<uint8_t> px(size * size * 4);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            float fx = (float)x / size * 6.0f;
            float fy = (float)y / size * 6.0f;
            float n = 0.0f, amp = 1.0f, freq = 1.0f, norm = 0.0f;
            for (int o = 0; o < 4; ++o) {
                n += perlin(fx * freq, fy * freq) * amp;
                norm += amp;
                amp *= 0.5f;
                freq *= 2.0f;
            }
            n = n / norm * 0.5f + 0.5f;
            n = std::clamp(n, 0.0f, 1.0f);
            float v = std::pow(n, 4.0f);
            uint8_t R = (uint8_t)(v * 255);
            uint8_t G = (uint8_t)(v * 220);
            uint8_t B = (uint8_t)(v * 160);
            size_t i = (y * size + x) * 4;
            px[i+0] = R; px[i+1] = G; px[i+2] = B; px[i+3] = 255;
        }
    }
    createFromPixels(px.data(), size, size, out, VK_FORMAT_R8G8B8A8_UNORM);
}

void generateSpecularSpots(uint32_t size, Texture& out)
{
    std::vector<uint8_t> px(size * size * 4, 0);
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> rad(0.05f, 0.25f);
    for (int s = 0; s < 6; ++s) {
        float cx = dist(rng) * size;
        float cy = dist(rng) * size;
        float r  = rad(rng) * size;
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                float dx = x - cx, dy = y - cy;
                float d = std::sqrt(dx*dx + dy*dy);
                if (d < r) {
                    float a = 1.0f - d / r;
                    a = a * a;
                    size_t i = (y * size + x) * 4;
                    px[i+0] = std::max<uint8_t>(px[i+0], (uint8_t)(a*255));
                    px[i+1] = std::max<uint8_t>(px[i+1], (uint8_t)(a*255));
                    px[i+2] = std::max<uint8_t>(px[i+2], (uint8_t)(a*255));
                    px[i+3] = 255;
                }
            }
        }
    }
    for (size_t i = 0; i < px.size(); i += 4) {
        if (px[i+3] == 0) px[i+3] = 255;
    }
    createFromPixels(px.data(), size, size, out, VK_FORMAT_R8G8B8A8_UNORM);
}

} // namespace texture