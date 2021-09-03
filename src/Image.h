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

public:
    /*
     * Loads an image from file.
     * If fails, generates a checkered pattern texture.
     *
     * Arguments:
     *      filePath: The file to load.
     *      size: The display size. If {0, 0}, the original size is used.
     */
    Image(const std::string& filePath,
          const glm::ivec2& size={0, 0}
        );

    void render(const glm::ivec2& pos) const;

    inline int getWidth() const { return m_size.x; }
    inline int getHeight() const { return m_size.y; }
    inline uint getSamplerId() const { return m_sampler; }

    inline void setWidth(int val) { m_size.x = val; }
    inline void setHeight(int val) { m_size.y = val; }
    inline void setSize(const glm::ivec2& val) { m_size = val; }

    ~Image();
};
