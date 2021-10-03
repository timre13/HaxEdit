#include "Split.h"
#include "Logger.h"

Split::Split()
{
    Logger::log << "Created a split" << Logger::End;
}

Split::Split(Buffer* buffer)
{
    addChild(buffer);
    Logger::log << "Created a split (with buffer)" << Logger::End;
}

Split::Split(Split* split)
{
    addChild(split);
    Logger::log << "Created a split (with split)" << Logger::End;
}

void Split::addChild(Buffer* buff)
{
    m_children.emplace_back(std::unique_ptr<Buffer>(buff));
    Logger::log("Split");
    Logger::log << "Added a child (a buffer): " << buff << Logger::End;
    setActiveChildI(m_children.size()-1);
    makeChildrenSizesEqual();
    Logger::log(Logger::End);
}

void Split::addChild(Split* split)
{
    m_children.emplace_back(std::unique_ptr<Split>(split));
    Logger::log("Split");
    Logger::log << "Added a child (a split): " << split << Logger::End;
    setActiveChildI(m_children.size()-1);
    makeChildrenSizesEqual();
    Logger::log(Logger::End);
}

Buffer* Split::getActiveBufferRecursively()
{
    if (m_children.empty())
        return nullptr;
    assert(m_activeChildI < m_children.size());
    auto& child = m_children[m_activeChildI];
    if (std::holds_alternative<std::unique_ptr<Split>>(child))
    {
        return std::get<std::unique_ptr<Split>>(child)->getActiveBufferRecursively();
    }
    else if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
    {
        return std::get<std::unique_ptr<Buffer>>(child).get();
    }
    else
    {
        assert(false);
        return nullptr;
    }
}

void Split::closeActiveBufferRecursively()
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
        if (!m_children.empty())
            setActiveChildI(m_activeChildI);
        makeChildrenSizesEqual();
    }
    else if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
    {
        std::get<std::unique_ptr<Split>>(child)->closeActiveBufferRecursively();
    }
    else
    {
        assert(false);
    }
}

void Split::forEachBuffer(const std::function<void(Buffer*, size_t, size_t)>& func)
{
    for (size_t i{}; i < m_children.size(); ++i)
    {
        auto& child = m_children[i];
        if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
        {
            func(std::get<std::unique_ptr<Buffer>>(
                        child).get(), i, m_children.size());
        }
    }
}

void Split::forEachBufferRecursively(const std::function<void(Buffer*)>& func)
{
    for (auto& child : m_children)
    {
        if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
        {
            func(std::get<std::unique_ptr<Buffer>>(child).get());
        }
        else if (std::holds_alternative<std::unique_ptr<Split>>(child))
        {
            std::get<std::unique_ptr<Split>>(child)->forEachBufferRecursively(func);
        }
        else
        {
            assert(false);
        }
    }
}

void Split::makeChildrenSizesEqual()
{
    forEachBuffer([](Buffer* buff, size_t i, size_t len){
            assert(len > 0);
            buff->setXPos((float)g_windowWidth/len*i);
            buff->setWidth((float)g_windowWidth/len);
            buff->setHeight(g_windowHeight);
            if (i == 0)
            {
                Logger::dbg << "Resized children to " << (float)g_windowWidth/len << "px"
                    << Logger::End;
            }
    });
}

void Split::increaseChildWidth(size_t index, int by)
{
    assert(index+1 < m_children.size());

    auto& child1 = m_children[index];
    auto& child2 = m_children[index+1];
    if (std::holds_alternative<std::unique_ptr<Buffer>>(child1)
     && std::holds_alternative<std::unique_ptr<Buffer>>(child2))
    {
        auto& child1P = std::get<std::unique_ptr<Buffer>>(child1);
        auto& child2P = std::get<std::unique_ptr<Buffer>>(child2);

        if (child1P->getWidth()+by < 10
         || child2P->getWidth()-by < 10)
            return;

        child1P->setWidth(child1P->getWidth()+by);
        child2P->setXPos(child2P->getXPos()+by);
        child2P->setWidth(child2P->getWidth()-by);
    }
}

Split::~Split()
{
    Logger::log << "Destroyed a split (" << m_children.size() << " children)" << Logger::End;
}
