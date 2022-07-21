#pragma once

#include <string>
#include <vector>
#include <git2.h>

namespace Git
{

extern bool hasInitBeenCalled;

enum class ChangeType
{
    Add,
    Delete,
    Modify,
};

class Repo
{
public:
    using diffList_t = std::vector<std::pair<int, ChangeType>>;

private:
    bool m_isRepo{};
    git_repository* m_repo{};
    std::string m_repoRootPath;

public:
    Repo(const std::string& path);

    inline const std::string& getRepoRoot() const { return m_repoRootPath; }
    inline bool isRepo() const { return m_isRepo; }

    diffList_t getDiff(const std::string& filename);
    struct GitObjectName
    {
        std::string name;
        bool isHash{}; // Hashes can be truncated, but branch names can't
    };
    GitObjectName getCheckedOutObjName() const;

    ~Repo();
};

void init();
void shutdown();

} // namespace Git
