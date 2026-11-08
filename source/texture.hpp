#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace texture {

struct Texture {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
};

bool loadFromFile(const std::string& path, Texture& out, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

bool createFromPixels(const uint8_t* pixels, uint32_t w, uint32_t h, Texture& out,
                      VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

Texture makeSolid1x1(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void destroy(Texture& t);

void generatePerlinEmissive(uint32_t size, Texture& out);
void generateSpecularSpots(uint32_t size, Texture& out);

} // namespace texture