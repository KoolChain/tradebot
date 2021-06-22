namespace sqlite_orm
{
    /*
     * ad::Decimal
     */
    template<>
    struct type_printer<ad::Decimal> : public real_printer {};

    template<>
    struct statement_binder<ad::Decimal>
    {
        int bind(sqlite3_stmt *stmt, int index, const ad::Decimal & value)
        {
            // The Decimal is rounded to the correct precision before being bound (DB input operations).
            // This way only the correct number of decimal digits is written/compared
            // (even if `value` was carrying more).
            return sqlite3_bind_double(stmt, index++, static_cast<double>(ad::fromFP(value)));
        }
    };

    template<>
    struct field_printer<ad::Decimal>
    {
        std::string operator()(const ad::Decimal & t) const
        {
            return ad::to_str(t);
        }
    };

    template<>
    struct row_extractor<ad::Decimal>
    {
        ad::Decimal extract(double row_value)
        {
            return ad::fromFP(row_value);
        }

        ad::Decimal extract(sqlite3_stmt *stmt, int columnIndex)
        {
            return this->extract(sqlite3_column_double(stmt, columnIndex));
        }
    };
}
