//
//  Result.h
//  LocationShare
//
//  Created by Santtu Rantanen on 25.8.2024.
//

#ifndef Result_hpp
#define Result_hpp

#include <string>

struct Result {
    enum class ResultCode{ Ok, Error };
    ResultCode code;
    std::string message;
};

#endif /* Result_h */
