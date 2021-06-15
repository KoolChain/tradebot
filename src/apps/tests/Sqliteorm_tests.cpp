#include "catch.hpp"

#include <trademath/Decimal.h>


#include <sqlite_orm/sqlite_orm.h>
#include <tradebot/OrmDecimalAdaptor-impl.h>


using namespace sqlite_orm;
using namespace ad;


struct Blob
{
    long integer;
    Decimal decimal;
    std::string text;
    long id{-1};
};


SCENARIO("Sqlite_orm unique constraint.", "[orm]")
{
    GIVEN("An Sqlite table with two columns in a unique constraint.")
    {
        auto storage = make_storage(":memory:",
                            make_table("uniquetest",
                                       make_column("id", &Blob::id, autoincrement(), primary_key()),
                                       make_column("integer", &Blob::integer),
                                       make_column("decimal", &Blob::decimal),
                                       make_column("text", &Blob::text),
                                       unique(&Blob::decimal, &Blob::integer)
                                       )
                       );
        storage.sync_schema();

        THEN("Several entries can be added if they have different integer-decimal pairs")
        {
            Blob a{
                1,
                1,
                "un"
            };

            Blob b{
                2,
                2,
                "deux"
            };

            Blob mix{
                1,
                2,
                "undeux"
            };

            storage.insert(a);
            storage.insert(b);
            storage.insert(mix);
            REQUIRE(storage.count<Blob>() == 3);

            THEN("Adding an existing entry a second time fails.")
            {
                REQUIRE_THROWS(storage.insert(a));
                REQUIRE(storage.count<Blob>() == 3);
            }

            THEN("Adding a new entry but with already existing unique pair fails.")
            {
                Blob bbis{
                    2,
                    2,
                    "maisdifferent"
                };
                REQUIRE_THROWS(storage.insert(bbis));
                REQUIRE(storage.count<Blob>() == 3);
            }
        }
    }
}
