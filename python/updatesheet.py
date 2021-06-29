#!/usr/bin/env python3

import dbaccess, gsheet

import argparse
from datetime import date


def last_integer(sheet_api, range_a1, default):
    column = sheet_api.read(range_a1)
    try:
        return int(column[-1][0] if column else default)
    except ValueError:
        return default


def insert_order_formulas(orders):
    for order in orders:
        yield order[:1] \
            + ('=from_epoch(INDIRECT(CONCAT("K"; ROW())))',) \
            + order[1:] \
            + ('=DSUM(Fragments!$A:$I; "taken_home"; {Fragments!$I$1;INDIRECT(CONCAT("A"; ROW()))})',) \
            + ('=IF(H:H=0; P:P/F:F; P:P/(F:F*L:L))',)   \
            + ('=IF(H:H=0; F:F*(G:G-L:L); "")',)        \
            + ('=F:F*L:L',)


def append_orders(sheet_api, database):
    """ returns ID of last order in the sheet """
    previous_id = last_integer(sheet_api, "Orders!A:A", 0)
    print("Previous order id {}.".format(previous_id))

    new_orders = database.get_orders_idrange(previous_id)
    if new_orders:
        sheet_api.append("Orders", list(insert_order_formulas(new_orders)))
        return new_orders[-1][0] # Last row, first column -> the ID of last appended order
    else:
        return previous_id # no orders appended, so last order id stays the same


def append_fragments(sheet_api, database, last_order):
    try:
        max_order_id = max([int(value[0]) for value in sheet_api.read("Fragments!I2:I")])
    except ValueError:
        max_order_id = 0
    print("Maximum order id in Fragments {}.".format(max_order_id))
    sheet_api.append("Fragments", database.get_fragments(max_order_id, last_order))


def insert_balance_formulas(balances, previous_time, database):
    for balance in balances:
        time = balance[1]
        count = database.count_launch_period(previous_time, time)
        previous_time = time
        yield balance[:1]   \
            + ('=from_epoch(INDIRECT("C" & ROW()))',) \
            + balance[1:]   \
            + (count,)


def append_balances(sheet_api, database):
    previous_time = last_integer(sheet_api, "Balances!C:C", 0)
    print("Previous balance time {}.".format(previous_time))
    balances = database.get_balances_after_time(previous_time)
    sheet_api.append("Balances", list(insert_balance_formulas(balances, previous_time, database)))


def main():
    parser = argparse.ArgumentParser("Write missing content from dogebot SQLite database to Google Sheet.")
    parser.add_argument("sqlitefile", help="SQLite3 database file.")
    parser.add_argument("tokenfile", help="The file containing user access tokens.")
    parser.add_argument("spreadsheet", help="The ID of the Google spreadhseet.")
    args = parser.parse_args()

    database = dbaccess.Database(args.sqlitefile)
    sheet_api = gsheet.Spreadsheet(args.tokenfile, args.spreadsheet)
    last_order_id = append_orders(sheet_api, database)
    append_fragments(sheet_api, database, last_order_id)
    append_balances(sheet_api, database)


if __name__ == "__main__":
    main()
