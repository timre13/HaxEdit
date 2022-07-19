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
    std::string m_filePath;
    bool m_isOpenFailed{};
    uint8_t* m_data{};
    bool m_preventLogging{};

public:
    static constexpr int UPSCALE_FILT_DEF = GL_LINEAR;
    static constexpr int DOWNSCALE_FILT_DEF = GL_LINEAR;

public:
    /*
     * Loads an image from file.
     * If fails, generates a checkered pattern texture.
     *
     * Arguments:
     *      filePath: The file to load.
     */
    Image(
            const std::string& filePath,
            uint upscaleFilt=UPSCALE_FILT_DEF, uint downscaleFilt=DOWNSCALE_FILT_DEF,
            bool preventLogging=false);

    /*
     * Render the image on the screen.
     *
     * pos: Where to render the image.
     * size: The display size. If {0, 0}, the original size is used.
     */
    void render(const glm::ivec2& pos, const glm::ivec2& size={0, 0}) const;

    inline uint getSamplerId() const { return m_sampler; }
    inline glm::ivec2 getPhysicalSize() const { return m_physicalSize; }
    RGBAColor getColorAt(glm::ivec2 pos);

    inline bool isOpenFailed() const { return m_isOpenFailed; }

    ~Image();
};
