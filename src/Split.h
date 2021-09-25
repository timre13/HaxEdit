#pragma once

#include "Buffer.h"
#include <vector>
#include <variant>
#include <memory>
#include <cassert>

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

    inline std::vector<child_t>& getChildren() { return m_children; }
    inline bool hasChild() { return !m_children.empty(); }
    inline child_t& getActiveChild()
    {
        assert(m_activeChildI < m_children.size());
        return m_children[m_activeChildI];
    }
    inline size_t getActiveChildI() const { return m_activeChildI; }
    inline Buffer* getActiveBufferRecursive()
    {
        if (m_children.empty())
            return nullptr;
        assert(m_activeChildI < m_children.size());
        //m_activeChildI = std::min(int(m_activeChildI), int(m_children.size())-1);
        auto& child = m_children[m_activeChildI];
        if (std::holds_alternative<std::unique_ptr<Split>>(child))
        {
            return std::get<std::unique_ptr<Split>>(child)->getActiveBufferRecursive();
        }
        else if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
        {
            return std::get<std::unique_ptr<Buffer>>(child).get();
        }
        else
        {
            assert(false);
        }
    }
    inline void closeActiveBufferRecursive()
    {
        if (m_children.empty())
            return;
        assert(m_activeChildI < m_children.size());
        auto& child = m_children[m_activeChildI];
        if (std::holds_alternative<std::unique_ptr<Buffer>>(child)
         || !std::get<std::unique_ptr<Split>>(child)->hasChild())
        {
            m_children.erase(m_children.begin()+m_activeChildI);
            m_activeChildI = std::min(m_activeChildI, m_children.size()-1);
        }
        else if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
        {
            std::get<std::unique_ptr<Split>>(child)->closeActiveBufferRecursive();
        }
        else
        {
            assert(false);
        }
    }

    ~Split();
};
