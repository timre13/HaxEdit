#include "Split.h"
#include "Logger.h"

Split::Split()
{
    assert(false);
    Logger::log << "Created a split" << Logger::End;
}

Split::Split(Buffer* buffer)
{
    m_children.emplace_back(std::unique_ptr<Buffer>(buffer));
    Logger::log << "Created a split (with buffer)" << Logger::End;
}

Split::Split(Split* split)
{
    m_children.emplace_back(std::unique_ptr<Split>(split));
    Logger::log << "Created a split (with split)" << Logger::End;
}

Split::~Split()
{
    Logger::log << "Destroyed a split (" << m_children.size() << " children)" << Logger::End;
}
