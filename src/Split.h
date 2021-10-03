#pragma once

#include "Buffer.h"
#include <vector>
#include <variant>
#include <memory>
#include <cassert>
#include <functional>

class Split final
{
public:
    using child_t = std::variant<std::unique_ptr<Buffer>, std::unique_ptr<Split>>;

    enum class Orientation
    {
        Vertical,
        Horizontal
    };

private:
    size_t m_activeChildI{};
    std::vector<child_t> m_children{};
    Orientation m_orientation;

public:
    /*
     * Constructs an empty Split object.
     */
    Split();
    /*
     * Constructs a Split object and takes ownership over `buffer`.
     */
    Split(Buffer* buffer);
    /*
     * Constructs a Split object and takes ownership over `split`.
     */
    Split(Split* split);

    inline Orientation getOrientation() const { return m_orientation; }

    inline size_t getNumOfChildren() const { return m_children.size(); }

    inline std::vector<child_t>& getChildren() { return m_children; }
    inline const std::vector<child_t>& getChildren() const { return m_children; }
    inline bool hasChild() { return !m_children.empty(); }
    inline child_t& getActiveChild()
    {
        assert(m_activeChildI < m_children.size());
        return m_children[m_activeChildI];
    }
    inline size_t getActiveChildI() const { return m_activeChildI; }
    inline void setActiveChildI(size_t index)
    {
        assert(index < m_children.size());
        {
            auto& activeChild = m_children[m_activeChildI];
            if (std::holds_alternative<std::unique_ptr<Buffer>>(activeChild))
            {
                std::get<std::unique_ptr<Buffer>>(activeChild)->setDimmed(true);
            }
        }
        m_activeChildI = index;
        {
            auto& activeChild = m_children[m_activeChildI];
            if (std::holds_alternative<std::unique_ptr<Buffer>>(activeChild))
            {
                std::get<std::unique_ptr<Buffer>>(activeChild)->setDimmed(false);
            }
        }
    }
    Buffer* getActiveBufferRecursively();

    void closeActiveBufferRecursively();

    void addChild(Buffer* buff);
    void addChild(Split* split);

    void forEachBuffer(const std::function<void(Buffer*, size_t, size_t)>& func);
    void forEachBufferRecursively(const std::function<void(Buffer*)>& func);

    void makeChildrenSizesEqual();

    void increaseChildWidth(size_t index, int by);

    ~Split();
};
