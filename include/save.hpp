#pragma once
#include "game.hpp"
#include <string>
namespace bricks {
enum class LoadStatus { Missing,Loaded,Recovered,Invalid };
class SaveStore {
public:
    explicit SaveStore(std::string directory):directory_(directory){}
    LoadStatus load(Profile& profile);
    bool save(const Profile& profile);
private:
    std::string directory_;
    uint32_t sequence_=0;
    int active_=-1;
};
}
