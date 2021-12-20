#include "Git.h"
#include "Logger.h"
#include <cassert>
#include <algorithm>

namespace Git
{

bool hasInitBeenCalled = false;

void init()
{
    Logger::log << "Initializing libgit2" << Logger::End;
    if (git_libgit2_init() < 0)
    {
        const git_error* err = git_error_last();
        Logger::err << "Failed to initialize libgit2: " << err->klass << ": " << err->message << Logger::End;
        return;
    }
    hasInitBeenCalled = true;
}

Repo::Repo(const std::string& path)
{
    assert(hasInitBeenCalled);
    Logger::log << "Opening repo: " << path << Logger::End;
    git_repository* repo;
    int err = git_repository_open_ext(&repo, path.c_str(), 0, "/tmp:/usr:/home:/mnt");
    if (err == GIT_ENOTFOUND)
    {
        Logger::warn << "Git repo not found at " << path << Logger::End;
        m_isRepo = false;
        return;
    }
    else if (err < -1)
    {
        const git_error* err = git_error_last();
        Logger::err << "Failed to open git repo at " << path << ": " << err->klass
            << ": " << err->message << Logger::End;
        m_isRepo = false;
        return;
    }
    m_repo = repo;
    m_isRepo = true;
    m_repoRootPath = git_repository_workdir(repo);
    assert(!m_repoRootPath.empty());
    Logger::dbg << "Repo root: " << m_repoRootPath << Logger::End;
}

static int lineDiffCallback(const git_diff_delta*, const git_diff_hunk*, const git_diff_line* line, void* payload)
{
    if (line->origin != 'F' && line->origin != '+' && line->origin != '-')
        return 0;

    auto _payload = (std::pair<const std::string&, Repo::diffList_t*>*)payload;
    Repo::diffList_t* output = _payload->second;
    const std::string& fileToDiff = _payload->first;

    static bool isFileToDiff = false;
    static std::string currentFile;
    static std::vector<int> changedLines{};

    if (line->origin == 'F')
    {
        currentFile = std::string(line->content, line->content_len);
        currentFile = currentFile.substr(currentFile.find('\n')+1);
        currentFile = currentFile.substr(currentFile.find('\n')+1);
        currentFile = currentFile.substr(currentFile.find('/')+1);
        currentFile = currentFile.substr(0, currentFile.find('\n'));
        Logger::dbg << "Current file is now " << currentFile << Logger::End;
        if (currentFile == fileToDiff)
        {
            isFileToDiff = true;
            Logger::dbg << "Found the file to diff" << Logger::End;
        }
        else if (isFileToDiff)
        {
            Logger::dbg << "End of file to diff, exiting" << Logger::End;
            isFileToDiff = false;
            changedLines.clear();
            return 1;
        }
    }
    else if (isFileToDiff)
    {
        const int changedLine = (line->origin == '+' ? line->new_lineno : line->old_lineno);
        assert(changedLine != -1);
        if (std::find(changedLines.begin(), changedLines.end(), changedLine) != changedLines.end())
        {
            Logger::dbg << "Modified line: " << changedLine << Logger::End;
            auto found = std::find_if(output->begin(), output->end(),
                    [&](std::pair<int, ChangeType> x){ return x.first == changedLine; }
            );
            *found = std::pair<int, ChangeType>{found->first, ChangeType::Modify};
        }
        else
        {
            ChangeType changeType;
            Logger::dbg << (line->origin == '+' ? "New" : "Deleted")
                << " line: " << changedLine << Logger::End;
            changeType = (line->origin == '+' ? ChangeType::Add : ChangeType::Delete);
            output->push_back({changedLine, changeType});
            changedLines.push_back(changedLine);
        }
    }
    return 0;
}

Repo::diffList_t Repo::getDiff(const std::string& filename)
{
    Logger::log << "Getting git diff of file " << filename << " in repo " << m_repoRootPath << Logger::End;
    if (!m_isRepo)
        return {};

    git_diff* diff;
    if (git_diff_index_to_workdir(&diff, m_repo, NULL, NULL) < 0)
    {
        const git_error* err = git_error_last();
        Logger::err << "Failed to get diff: " << err->klass << ": " << err->message << Logger::End;
        return {};
    }

    diffList_t diffList;
    std::pair<const std::string&, diffList_t*> pair{filename, &diffList};
    git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, lineDiffCallback, &pair);

    git_diff_free(diff);

    Logger::dbg << "Found " << diffList.size() << " differences in the file " << filename << Logger::End;
    return diffList;
}

Repo::~Repo()
{
    git_repository_free(m_repo);
    Logger::dbg << "Closed git repo: " << m_repoRootPath << Logger::End;
}

void shutdown()
{
    Logger::log << "Shutting down libgit2" << Logger::End;
    if (git_libgit2_shutdown() < 0)
    {
        const git_error* err = git_error_last();
        Logger::err << "Failed to shut down libgit2: " << err->klass << ": " << err->message << Logger::End;
    }
    hasInitBeenCalled = false;
}

} // namespace Git
