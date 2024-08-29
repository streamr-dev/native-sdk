//
// Created by Santtu Rantanen on 28.8.2024.
//

#ifndef LOCATIONSHARE_RESULT_H
#define LOCATIONSHARE_RESULT_H

#include <string>

struct Result {
    enum class ResultCode{ Ok, Error };
    ResultCode code;
    std::string message;
};


#endif //LOCATIONSHARE_RESULT_H
