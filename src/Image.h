#pragma once

#include <string>
#include <glm/glm.hpp>
#include "UiRenderer.h"
#include "types.h"

class Image final
{
private:
    uint m_sampler{};
    glm::ivec2 m_physicalSize{};

public:
    /*
     * Loads an image from file.
     * If fails, generates a checkered pattern texture.
     *
     * Arguments:
     *      filePath: The file to load.
     */
    Image(const std::string& filePath);

    /*
     * Render the image on the screen.
     *
     * pos: Where to render the image.
     * size: The display size. If {0, 0}, the original size is used.
     */
    void render(const glm::ivec2& pos, const glm::ivec2& size={0, 0}) const;

    inline uint getSamplerId() const { return m_sampler; }

    ~Image();
};
