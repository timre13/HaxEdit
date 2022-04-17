#include "signs.h"
#include "Logger.h"
#include "App.h"
#include <cassert>

std::unique_ptr<Image> signImages[(int)Sign::_Count]{};

void loadSigns()
{
    static constexpr const char* signImgFileNames[(int)Sign::_Count]{
        "../img/signs/git_add.png",
        "../img/signs/git_del.png",
        "../img/signs/git_mod.png",
    };

    Logger::dbg << "Loading sign images" << Logger::End;
    for (int i{}; i < (int)Sign::_Count; ++i)
    {
        signImages[i] = std::make_unique<Image>(App::getResPath(signImgFileNames[i]));
    }
}

int getSignColumn(Sign sign)
{
    switch (sign)
    {
        case Sign::GitAdd:
        case Sign::GitRemove:
        case Sign::GitModify:
            return 0;

        case Sign::_Count:
            assert(false);
            return -1;
    }
}
