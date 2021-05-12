#define BOILERENUM(enumname) \
    template<>  \
    struct type_printer<enumname> : public integer_printer {};   \
    \
    template<>  \
    struct statement_binder<enumname>    \
    {   \
        int bind(sqlite3_stmt *stmt, int index, const enumname &value) { \
            return statement_binder<int>().bind(stmt, index, static_cast<int>(value));  \
        }   \
    };  \
    \
    template<>  \
    struct field_printer<enumname>   \
    {   \
        int operator()(const enumname &t) const  \
        {   \
            return static_cast<int>(t); \
        }   \
    };  \
    \
    template<>  \
    struct row_extractor<enumname>   \
    {   \
        enumname extract(int row_value)  \
        {   \
            return static_cast<enumname>(row_value); \
        }   \
    \
        enumname extract(sqlite3_stmt *stmt, int columnIndex) {  \
            return this->extract(sqlite3_column_int(stmt, columnIndex));    \
        }   \
    };

namespace sqlite_orm
{
    BOILERENUM(ad::tradebot::Order::Status)
    BOILERENUM(ad::tradebot::Order::FulfillResponse)
    BOILERENUM(ad::tradebot::Direction)
}

#undef BOILERENUM
