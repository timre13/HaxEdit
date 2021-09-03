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
    glm::ivec2 m_size{};
    glm::ivec2 m_pos{};

public:
    /*
     * Loads an image from file.
     * If fails, generates a checkered pattern texture.
     *
     * Arguments:
     *      filePath: The file to load.
     *      pos: The position of the image on the screen.
     *      size: The display size. If {0, 0}, the original size is used.
     */
    Image(const std::string& filePath,
          const glm::ivec2& pos,
          const glm::ivec2& size={0, 0}
        );

    void render();

    inline int getWidth() const { return m_size.x; }
    inline int getHeight() const { return m_size.y; }
    inline int getXPos() const { return m_pos.x; }
    inline int getYPos() const { return m_pos.y; }
    inline uint getSamplerId() const { return m_sampler; }

    ~Image();
};
