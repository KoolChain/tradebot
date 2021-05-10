#include "Sqlite.h"

#include <spdlog/spdlog.h>

#include <sqlite3.h>


namespace ad {
namespace sqlite {


namespace detail {

    std::string getMessage(int aSqliteCode)
    {
        return {sqlite3_errstr(aSqliteCode)};
    }

} // namespace detail


struct Connection::Impl
{
    Impl(std::string aFilename);

    const std::string filename;
    sqlite3 * database;
};


Connection::Impl::Impl(std::string aFilename) :
    filename{std::move(std::move(aFilename))},
    database{nullptr}
{
    int code = sqlite3_open_v2(filename.c_str(),
                               &database,
                               SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                               nullptr /*default vfs*/);
    if (code != SQLITE_OK)
    {
        spdlog::critical("Could not open database connection to: '{}'. Error: '{}'",
                         filename,
                         detail::getMessage(code));
        throw std::runtime_error(detail::getMessage(code));
    }
}


struct Statement::Impl
{
    sqlite3_stmt * statement{nullptr};
};


Connection::Connection(std::string aFilename) :
    mImpl{std::make_unique<Impl>(aFilename)}
{}


Connection::~Connection()
{
    if (int code = sqlite3_close(mImpl->database); code != SQLITE_OK)
    {
        spdlog::error("Could not close database connection to: '{}'. Error: '{}'",
                      mImpl->filename,
                      detail::getMessage(code));
    }
}


Statement Connection::prepare(const std::string & aSqlStatement)
{
    auto statementImpl = std::make_unique<Statement::Impl>();
    int code = sqlite3_prepare_v2(
            mImpl->database,
            aSqlStatement.data(),
            aSqlStatement.size(),
            &(statementImpl->statement),
            nullptr /*no tail, all statement is read at once*/);

    if (code != SQLITE_OK)
    {
        spdlog::critical("Could not prepare sql statement '{}'. Error: '{}'",
                         aSqlStatement,
                         detail::getMessage(code));
        throw std::logic_error(detail::getMessage(code));
    }

    return Statement{std::move(statementImpl)};
}


Statement::Statement(std::unique_ptr<Impl> aImpl) :
    mImpl{std::move(aImpl)}
{}


Statement::~Statement()
{
    if (int code = sqlite3_finalize(mImpl->statement); code != SQLITE_OK)
    {
        spdlog::error("Error while finalizing statement: '{}'. Error: '{}'",
                      getSqlText(),
                      detail::getMessage(code));
    }
}


std::string Statement::getSqlText() const
{
    return {sqlite3_sql(mImpl->statement)};
}


} // namespace sqlite
} // namespace ad
