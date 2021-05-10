#pragma once


#include <experimental/propagate_const>

#include <string>


namespace ad {
namespace sqlite {


class Statement;


class Connection
{
    struct Impl;

public:
    Connection(std::string aFilename);
    ~Connection();

    Statement prepare(const std::string & aSqlStatement);

private:
    std::experimental::propagate_const<std::unique_ptr<Impl>> mImpl;
};


class Statement
{
    friend class Connection;
    struct Impl;

private:
    Statement(std::unique_ptr<Impl> aImpl);

public:
    ~Statement();

    std::string getSqlText() const;

private:
    std::experimental::propagate_const<std::unique_ptr<Impl>> mImpl;
};



} // namespace storage
} // namespace ad
