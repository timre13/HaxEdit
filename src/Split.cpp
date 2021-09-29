#include "Split.h"
#include "Logger.h"

Split::Split()
{
    assert(false);
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
    makeChildrenSizesEqual();
    Logger::log(Logger::End);
}

void Split::addChild(Split* split)
{
    m_children.emplace_back(std::unique_ptr<Split>(split));
    Logger::log("Split");
    Logger::log << "Added a child (a split): " << split << Logger::End;
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

Split::~Split()
{
    Logger::log << "Destroyed a split (" << m_children.size() << " children)" << Logger::End;
}